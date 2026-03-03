-- Camera/camera_collision.lua
-- Pulls the camera in front of walls using a raycast from the look-at point.
-- Uses exponential lerp so the camera snaps quickly into walls but eases back out.

local M = {}

-- Adjusts desiredX/Y/Z based on scene geometry.
-- tx,ty,tz      = camera look-at (target) position
-- desiredX/Y/Z  = ideal camera position before collision
-- dt            = delta time for smoothing
-- Returns the adjusted (x, y, z) position.
function M.applyCollision(self, tx, ty, tz, desiredX, desiredY, desiredZ, dt)
    if not (self.collisionEnabled and Physics and Physics.Raycast) then
        return desiredX, desiredY, desiredZ
    end

    local dirX = desiredX - tx
    local dirY = desiredY - ty
    local dirZ = desiredZ - tz
    local dirLen = math.sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ)

    if dirLen <= 0.001 then
        return desiredX, desiredY, desiredZ
    end

    dirX, dirY, dirZ = dirX/dirLen, dirY/dirLen, dirZ/dirLen

    local dist = Physics.Raycast(tx, ty, tz, dirX, dirY, dirZ, dirLen + 0.5)
    local hit  = (dist >= 0 and dist < dirLen)

    local collisionOffset = self.collisionOffset or 0.2
    local targetDist      = dirLen

    if hit then
        targetDist = math.max(dist - collisionOffset, 0.5)
    end

    -- Initialise on first call
    if self._currentCollisionDist == nil then
        self._currentCollisionDist = targetDist
    else
        -- Fast snap when hitting a wall, slow ease-out when it clears
        local lerpSpeed = (targetDist < self._currentCollisionDist)
            and (self.collisionLerpIn  or 20.0)
            or  (self.collisionLerpOut or  5.0)
        local t = 1.0 - math.exp(-lerpSpeed * dt)
        self._currentCollisionDist = self._currentCollisionDist
            + (targetDist - self._currentCollisionDist) * t
    end

    local d = self._currentCollisionDist
    return tx + dirX*d, ty + dirY*d, tz + dirZ*d
end

return M
