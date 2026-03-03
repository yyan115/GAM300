-- Camera/camera_chain_aim.lua
-- Chain-aim camera: smoothly blends between orbit mode and a fixed aim anchor.
-- Also handles blended rotation when both modes are partially active.

local utils     = require("Camera.camera_utils")
local atan2     = utils.atan2
local eulerToQuat = utils.eulerToQuat

local event_bus = _G.event_bus

local M = {}

-- Advance the chain-aim blend each frame and compute the aim camera position.
-- Returns: chainAimActive (bool), chainDesiredX, chainDesiredY, chainDesiredZ
-- chainDesiredX/Y/Z are nil when chainAimActive is false.
function M.updateChainAim(self, dt)
    -- Smooth blend factor toward target (0 = orbit, 1 = chain aim)
    local blendTarget = self._chainAiming and 1.0 or 0.0
    local blendSpeed  = self.chainAimTransitionSpeed or 5.0
    local blendT      = 1.0 - math.exp(-blendSpeed * dt)
    self._chainAimBlend = self._chainAimBlend + (blendTarget - self._chainAimBlend) * blendT

    -- Snap to exact 0/1 to avoid micro-lerp stall
    if self._chainAimBlend < 0.001 then self._chainAimBlend = 0.0 end
    if self._chainAimBlend > 0.999 then self._chainAimBlend = 1.0 end

    local chainAimActive = self._chainAimBlend > 0.0
    if not chainAimActive then
        return false, nil, nil, nil
    end

    local aimPos    = Engine.FindTransformByName(self.chainAimPosName)
    local aimTarget = Engine.FindTransformByName(self.chainAimTargetName)

    if not (aimPos and aimTarget) then
        -- Named transforms gone; abort blend
        self._chainAimBlend = 0.0
        return false, nil, nil, nil
    end

    -- Resolve world position of the camera anchor
    local camX, camY, camZ = 0, 0, 0
    local ok, a, b, c = pcall(function()
        if Engine and Engine.GetTransformWorldPosition then
            return Engine.GetTransformWorldPosition(aimPos)
        end
    end)
    if ok and a ~= nil then
        if type(a) == "table" then
            camX, camY, camZ = a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
        else
            camX, camY, camZ = a, b, c
        end
    end

    -- On the very first frame of chain aim, lock yaw/pitch toward the end target
    if not self._chainAimInitialized then
        local targetX, targetY, targetZ = 0, 0, 0
        ok, a, b, c = pcall(function()
            if Engine and Engine.GetTransformWorldPosition then
                return Engine.GetTransformWorldPosition(aimTarget)
            end
        end)
        if ok and a ~= nil then
            if type(a) == "table" then
                targetX, targetY, targetZ = a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
            else
                targetX, targetY, targetZ = a, b, c
            end
        end

        local dirX = targetX - camX
        local dirY = targetY - camY
        local dirZ = targetZ - camZ
        local dirLen = math.sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ)
        if dirLen > 0.0001 then
            dirX, dirY, dirZ = dirX/dirLen, dirY/dirLen, dirZ/dirLen
            self._chainAimYaw   = math.deg(atan2(dirX, dirZ))
            self._chainAimPitch = -math.deg(math.asin(dirY))
            self._chainAimInitialized = true
        end
    end

    -- Publish forward basis for chain-throw direction while actively aiming
    if self._chainAiming then
        local aimYaw   = self._chainAimYaw   or self._yaw
        local aimPitch = self._chainAimPitch or self._pitch
        local yr  = math.rad(aimYaw)
        local pr  = math.rad(aimPitch)
        local fx  = math.sin(yr) * math.cos(pr)
        local fy  = -math.sin(pr)
        local fz  = math.cos(yr) * math.cos(pr)
        if event_bus and event_bus.publish then
            event_bus.publish("ChainAim_basis", { forward = { x = fx, y = fy, z = fz } })
        end
    end

    -- Override CAMERA_YAW with the chain-aim yaw so player movement matches
    -- what the camera is actually showing. updateMouseLook already ran this
    -- frame and set _G.CAMERA_YAW to the orbit yaw, so we overwrite it here.
    --
    -- Convention difference: orbit _yaw is the camera's *position* offset angle
    -- (camera sits at sin/cos of yaw relative to player), while _chainAimYaw is
    -- the camera's *look* direction. The two are 180° apart, so we add 180 to
    -- convert chain-aim look-direction into the orbit-offset convention that the
    -- player movement formula expects.
    if self._chainAimYaw then
        local blend = self._chainAimBlend
        local chainAsOrbit = self._chainAimYaw + 180.0
        local effectiveYaw = self._yaw + (chainAsOrbit - self._yaw) * blend
        _G.CAMERA_YAW = effectiveYaw
        if event_bus and event_bus.publish then
            event_bus.publish("camera_yaw", effectiveYaw)
        end
    end

    return true, camX, camY, camZ
end

-- Set the camera's rotation, blending between orbit look-at and chain-aim yaw/pitch.
-- newX/Y/Z   = final camera world position after lerp
-- cameraTarget = {x,y,z} look-at point used for orbit rotation
-- chainAimActive, blend = state from updateChainAim
function M.applyRotation(self, newX, newY, newZ, cameraTarget, chainAimActive, blend)
    if chainAimActive and blend > 0.0 and self._chainAimYaw then
        -- Compute what the orbit rotation would be
        local ofx = cameraTarget.x - newX
        local ofy = cameraTarget.y - newY
        local ofz = cameraTarget.z - newZ
        local olen = math.sqrt(ofx*ofx + ofy*ofy + ofz*ofz)
        local orbitYaw, orbitPitch = self._yaw, self._pitch
        if olen > 0.0001 then
            ofx, ofy, ofz = ofx/olen, ofy/olen, ofz/olen
            orbitYaw   = math.deg(atan2(ofx, ofz))
            orbitPitch = -math.deg(math.asin(ofy))
        end

        -- Blend toward chain-aim rotation
        local blendedYaw   = orbitYaw   + (self._chainAimYaw   - orbitYaw)   * blend
        local blendedPitch = orbitPitch + (self._chainAimPitch - orbitPitch) * blend
        local q = eulerToQuat(blendedPitch, blendedYaw, 0.0)
        self:SetRotation(q.w, q.x, q.y, q.z)
    else
        -- Pure orbit: look directly at the target
        local fx = cameraTarget.x - newX
        local fy = cameraTarget.y - newY
        local fz = cameraTarget.z - newZ
        local flen = math.sqrt(fx*fx + fy*fy + fz*fz)
        if flen > 0.0001 then
            fx, fy, fz = fx/flen, fy/flen, fz/flen
            local q = eulerToQuat(-math.deg(math.asin(fy)), math.deg(atan2(fx, fz)), 0.0)
            self:SetRotation(q.w, q.x, q.y, q.z)
        end
    end
end

return M
