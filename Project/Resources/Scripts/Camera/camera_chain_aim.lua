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

    -- On the first frame of chain aim, inherit the current orbit yaw/pitch so
    -- the camera zooms in along the direction it was already looking.
    -- self._yaw is never wrapped (accumulates freely), so we cannot add 180
    -- directly — that would make _chainAimYaw a huge number and cause
    -- applyRotation to interpolate through hundreds of degrees (visible spin).
    -- Instead, collapse through sin/cos then atan2 to get a canonical angle
    -- in (-180, 180] that represents the same look direction.
    if not self._chainAimInitialized then
        local yr = math.rad(self._yaw)
        self._chainAimYaw         = math.deg(atan2(-math.sin(yr), -math.cos(yr)))
        self._chainAimPitch       = self._pitch
        self._chainAimInitialized = true
    end

    -- Compute the zoomed-in camera position: same orbital direction as current
    -- aim but at chainAimZoomDistance radius instead of followDistance.
    local zoomDist  = self.chainAimZoomDistance or 1.5
    -- _chainAimYaw is look-direction; add 180 to get orbit position-offset convention.
    local orbitYaw  = math.rad(self._chainAimYaw + 180.0)
    local aimPitchR = math.rad(self._chainAimPitch or 0.0)

    local minZ = self.minZoom or 2.0
    local maxZ = self.maxZoom or 15.0
    local zf   = math.max(0.0, math.min(1.0, (zoomDist - minZ) / (maxZ - minZ)))
    local lookAtH = 0.5 + zf * 0.7
    local scaleH  = (self.heightOffset or 1.0) * (0.3 + zf * 0.7)

    local pivX = self._targetPos.x
    local pivY = self._targetPos.y + lookAtH
    local pivZ = self._targetPos.z

    local hRadius = zoomDist * math.cos(aimPitchR)
    local camX = pivX + hRadius * math.sin(orbitYaw)
    local camY = pivY + zoomDist * math.sin(aimPitchR) + scaleH
    local camZ = pivZ + hRadius * math.cos(orbitYaw)

    -- Over-the-shoulder: shift camera right relative to the look direction.
    -- Right vector in XZ = (cos(lookYaw), 0, -sin(lookYaw)).
    local sideOffset = self.chainAimSideOffset or 0.3
    local lookYawRad = math.rad(self._chainAimYaw)
    camX = camX - math.cos(lookYawRad) * sideOffset
    camZ = camZ + math.sin(lookYawRad) * sideOffset

    -- Soft aim assist: gently pull _chainAimYaw/_chainAimPitch toward the
    -- nearest enemy within the configured angular window.
    if self._chainAiming and self._chainAimYaw then
        M.updateAimAssist(self, dt, camX, camY, camZ)
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

-- Wraps an angle difference to the [-180, 180] range so we always pull the
-- short way around the circle.
local function shortestDelta(from, to)
    local d = (to - from) % 360.0
    if d > 180.0 then d = d - 360.0 end
    return d
end

-- Soft aim assist: nudge _chainAimYaw/_chainAimPitch toward the nearest enemy
-- inside the angular window. Called only while _chainAiming is true.
-- camX/Y/Z is the chain-aim camera anchor position.
function M.updateAimAssist(self, dt, camX, camY, camZ)
    if not (Engine and Engine.FindEntitiesWithScript and Engine.GetEntityPosition) then return end

    local assistAngle    = self.chainAimAssistAngle        or 20.0
    local assistStrength = self.chainAimAssistStrength     or 3.0
    local assistRange    = self.chainAimAssistRange        or 25.0
    local heightOffset   = self.chainAimAssistHeightOffset or 1.0
    local enemyNames     = self.chainAimAssistEnemyNames   or {}

    local currentYaw   = self._chainAimYaw
    local currentPitch = self._chainAimPitch or 0.0

    local bestDeviation = math.huge
    local bestDYaw      = 0.0
    local bestDPitch    = 0.0

    for _, scriptName in ipairs(enemyNames) do
        local entities = Engine.FindEntitiesWithScript(scriptName)
        if entities then
            for i = 1, #entities do
                local ex, ey, ez = Engine.GetEntityPosition(entities[i])
                if ex then
                    -- World-distance gate (XZ plane, cheap check first)
                    local dx = ex - camX
                    local dy = (ey + heightOffset) - camY  -- aim at body center, not feet
                    local dz = ez - camZ
                    local distSq = dx*dx + dz*dz
                    if distSq <= assistRange * assistRange then
                        -- Angular direction from camera to enemy
                        local len3d = math.sqrt(dx*dx + dy*dy + dz*dz)
                        if len3d > 0.01 then
                            local targetYaw   = math.deg(atan2(dx, dz))
                            local targetPitch = -math.deg(math.asin(
                                math.max(-1.0, math.min(1.0, dy / len3d))
                            ))

                            -- Angular deviation (shortest path in each axis)
                            local dYaw   = shortestDelta(currentYaw,   targetYaw)
                            local dPitch = shortestDelta(currentPitch, targetPitch)
                            local deviation = math.sqrt(dYaw*dYaw + dPitch*dPitch)

                            if deviation < bestDeviation then
                                bestDeviation = deviation
                                bestDYaw      = dYaw
                                bestDPitch    = dPitch
                            end
                        end
                    end
                end
            end
        end
    end

    -- Apply pull only when best candidate is inside the angular window
    if bestDeviation < assistAngle then
        -- Strength falls off linearly to zero at the edge of the window
        local factor = 1.0 - (bestDeviation / assistAngle)
        local pull   = assistStrength * factor * dt

        self._chainAimYaw   = currentYaw   + bestDYaw   * pull
        self._chainAimPitch = math.max(
            self.minPitch or -80.0,
            math.min(self.maxPitch or 80.0,
                currentPitch + bestDPitch * pull
            )
        )
    end
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
