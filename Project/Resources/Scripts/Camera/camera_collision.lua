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

    local collisionOffset = self.collisionOffset or 0.2
    local targetDist      = dirLen

    -- Forward ray: pivot → desired camera (catches walls/floors)
    local dist = Physics.Raycast(tx, ty, tz, dirX, dirY, dirZ, dirLen + 0.5)
    if dist >= 0 and dist < dirLen then
        targetDist = math.max(dist - collisionOffset, 0.5)
    end

    -- Reverse ray: desired → pivot (catches back-facing surfaces the forward ray misses)
    local revDist = Physics.Raycast(
        desiredX, desiredY, desiredZ, -dirX, -dirY, -dirZ, dirLen + 0.5)
    if revDist >= 0 and revDist < dirLen then
        targetDist = math.min(targetDist, math.max(dirLen - revDist - collisionOffset, 0.5))
    end

    -- Ceiling clamp: cast straight up from both the pivot and the camera's XZ
    -- position (the orbit may push the camera out from under the ceiling the
    -- pivot-only ray covers). Use whichever gives the tighter constraint.
    if dirY > 0.01 then
        local bestMaxDist = math.huge

        local function tryCeilRay(ox, oz)
            local d = Physics.Raycast(ox, ty, oz, 0, 1, 0, 50.0)
            if d >= 0 then
                local md = (d - collisionOffset) / dirY
                if md < bestMaxDist then bestMaxDist = md end
            end
        end

        tryCeilRay(tx, tz)               -- directly above pivot
        tryCeilRay(desiredX, desiredZ)   -- above camera's horizontal position

        if bestMaxDist < math.huge then
            targetDist = math.min(targetDist, math.max(bestMaxDist, 0.5))
        end
    end

    -- Initialise on first call
    if self._currentCollisionDist == nil then
        self._currentCollisionDist = targetDist
    elseif targetDist < self._currentCollisionDist then
        -- Snap immediately into geometry — no lerp, eliminates the single frame
        -- where the camera clips through a wall before the lerp catches up.
        self._currentCollisionDist = targetDist
    else
        -- Slow ease-out when geometry clears so the camera doesn't snap back.
        local t = 1.0 - math.exp(-(self.collisionLerpOut or 5.0) * dt)
        self._currentCollisionDist = self._currentCollisionDist
            + (targetDist - self._currentCollisionDist) * t
    end

    local d = self._currentCollisionDist
    return tx + dirX*d, ty + dirY*d, tz + dirZ*d
end

return M
