-- Camera/camera_chain_aim.lua
-- Chain-aim camera: smoothly blends between orbit mode and a fixed aim anchor.
-- Also handles blended rotation when both modes are partially active.

local utils     = require("Camera.camera_utils")
local atan2     = utils.atan2
local eulerToQuat = utils.eulerToQuat

local event_bus = _G.event_bus

local M = {}

-- Wraps an angle difference to the [-180, 180] range so we always interpolate
-- the short way around the circle.
local function shortestDelta(from, to)
    local d = (to - from) % 360.0
    if d > 180.0 then d = d - 360.0 end
    return d
end

-- Returns true if there is an unobstructed line from the given origin to the enemy.
-- ox/oy/oz should be the camera position, which is outside the player's physics
-- capsule. Starting inside the capsule causes Physics.Raycast to return -1 (no hit)
-- making every enemy appear visible regardless of walls.
local function hasLineOfSight(ox, oy, oz, ex, ey, ez)
    if not (Physics and Physics.Raycast) then return true end

    local dx = ex - ox
    local dy = ey - oy
    local dz = ez - oz
    local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
    if dist < 0.01 then return true end

    local ndx, ndy, ndz = dx / dist, dy / dist, dz / dist

    -- hitDist > 0 means geometry was hit. If it's closer than the enemy
    -- (with 0.3 m tolerance for the enemy's own collider) the LOS is blocked.
    local hitDist = Physics.Raycast(ox, oy, oz, ndx, ndy, ndz, dist)
    if hitDist and hitDist > 0 and hitDist < dist - 0.3 then
        return false
    end
    return true
end

-- Advance the chain-aim blend each frame and compute the aim camera position.
-- Returns: chainAimActive (bool), chainDesiredX, chainDesiredY, chainDesiredZ
-- chainDesiredX/Y/Z are nil when chainAimActive is false.
function M.updateChainAim(self, dt)
    -- Blend factor: snap to 1 immediately when aiming so there is zero lerp
    -- on position, rotation, and CAMERA_YAW. Smooth out only on release.
    if self._chainAiming then
        self._chainAimBlend = 1.0
    else
        local blendSpeed = self.chainAimTransitionSpeed or 5.0
        local blendT     = 1.0 - math.exp(-blendSpeed * dt)
        self._chainAimBlend = self._chainAimBlend + (0.0 - self._chainAimBlend) * blendT
        if self._chainAimBlend < 0.001 then self._chainAimBlend = 0.0 end
    end

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
        self._chainAimPitch       = 0.0   -- always start looking forward (horizontal)
        self._chainAimInitialized = true
    end

    -- Compute the chain-aim camera position starting from the player's world
    -- position, then apply chainAimHeightOffset (up), chainAimSideOffset
    -- (over-the-shoulder), and chainAimZoomDistance (behind the player).
    local zoomDist     = self.chainAimZoomDistance or 0.8
    local heightOffset = self.chainAimHeightOffset or 1.5
    local sideOffset   = self.chainAimSideOffset   or 0.3

    -- _chainAimYaw is look-direction; add 180 to get orbit position-offset convention.
    local orbitYaw  = math.rad(self._chainAimYaw + 180.0)
    local aimPitchR = math.rad(self._chainAimPitch or 0.0)

    local hRadius = zoomDist * math.cos(aimPitchR)
    local camX = self._targetPos.x + hRadius * math.sin(orbitYaw)
    local camY = self._targetPos.y + heightOffset + zoomDist * math.sin(aimPitchR)
    local camZ = self._targetPos.z + hRadius * math.cos(orbitYaw)

    -- Over-the-shoulder: shift camera sideways relative to the look direction.
    -- Right vector in XZ = (cos(lookYaw), 0, -sin(lookYaw)).
    local lookYawRad = math.rad(self._chainAimYaw)
    camX = camX - math.cos(lookYawRad) * sideOffset
    camZ = camZ + math.sin(lookYawRad) * sideOffset

    -- Tick down the manual-aim cooldown so aim assist stays off while the
    -- player is actively moving the camera.
    if self._chainAimManualTimer and self._chainAimManualTimer > 0 then
        self._chainAimManualTimer = self._chainAimManualTimer - dt
    end

    -- Soft aim assist: gently pull _chainAimYaw/_chainAimPitch toward the
    -- nearest enemy within the configured angular window.
    -- Skip when the player is manually aiming so it doesn't fight their input.
    local manuallyAiming = self._chainAimManualTimer and self._chainAimManualTimer > 0
    if self._chainAiming and self._chainAimYaw and not manuallyAiming then
        M.updateAimAssist(self, dt, camX, camY, camZ)
    elseif manuallyAiming then
        -- Clear stale assist target so the chain fires where the player is
        -- actually looking, not at the last auto-aimed enemy.
        self._assistTargetX = nil
        self._assistTargetY = nil
        self._assistTargetZ = nil
        self._assistPrevTargetYaw   = nil
        self._assistPrevTargetPitch = nil
    end

    -- Publish forward basis and crosshair world target for chain-throw direction.
    -- When aim assist has a locked target, fire from player toward that enemy
    -- so the chain travels toward the actual enemy rather than along raw camera angles.
    if self._chainAiming then
        -- Camera forward direction (always from camera look angles).
        local aimYaw   = self._chainAimYaw   or self._yaw
        local aimPitch = self._chainAimPitch or self._pitch
        local yr = math.rad(aimYaw)
        local pr = math.rad(aimPitch)
        local fx = math.sin(yr) * math.cos(pr)
        local fy = -math.sin(pr)
        local fz = math.cos(yr) * math.cos(pr)

        -- World target: if aim assist has a locked enemy, use its position
        -- directly. Otherwise raycast from camera to find the crosshair hit.
        -- ChainBootstrap computes the actual fire direction from the chain's
        -- start position (hand bone) toward this world target.
        local wx, wy, wz
        if self._assistTargetX then
            wx = self._assistTargetX
            wy = self._assistTargetY
            wz = self._assistTargetZ
        else
            local crosshairMaxDist = 100.0
            local hitDist = crosshairMaxDist
            if Physics and Physics.Raycast then
                local d = Physics.Raycast(camX, camY, camZ, fx, fy, fz, crosshairMaxDist)
                if d and d > 0 then hitDist = d end
            end
            -- Use camera position as origin so the world target matches exactly
            -- where the crosshair is pointing. Previously this used the player's
            -- eye position with the camera's hit distance, causing parallax error.
            wx = camX + fx * hitDist
            wy = camY + fy * hitDist
            wz = camZ + fz * hitDist
        end

        if event_bus and event_bus.publish then
            event_bus.publish("ChainAim_basis", { forward = { x = fx, y = fy, z = fz } })
            event_bus.publish("ChainAim_worldTarget", { x = wx, y = wy, z = wz })
            event_bus.publish("chain.aim_assist_locked", { locked = self._assistTargetX ~= nil })
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
        local effectiveYaw = self._yaw + shortestDelta(self._yaw, chainAsOrbit) * blend
        _G.CAMERA_YAW = effectiveYaw
        if event_bus and event_bus.publish then
            event_bus.publish("camera_yaw", effectiveYaw)
        end
    end

    return true, camX, camY, camZ
end

-- Aim assist during chain aim.
-- Two-component design:
--   1) Velocity tracking  — matches the enemy's angular movement so the camera
--      keeps pace automatically with no trailing.
--   2) Corrective pull    — small constant-speed nudge that closes any remaining
--      gap. Weak enough that normal mouse input overrides it easily.
-- Direction is computed from the player's world position (self._targetPos).
function M.updateAimAssist(self, dt, camX, camY, camZ)
    if not (Engine and Engine.FindEntitiesWithScript and Engine.GetEntityPosition) then return end

    local assistAngle    = self.chainAimAssistAngle         or 30.0
    local assistStrength = self.chainAimAssistStrength     or 15.0   -- corrective pull deg/s
    local assistRange    = self.chainAimAssistRange        or 12.0
    local heightOffset   = self.chainAimAssistHeightOffset or 1.0
    local enemyNames     = self.chainAimAssistComponents   or {}

    local currentYaw   = self._chainAimYaw
    local currentPitch = self._chainAimPitch or 0.0

    local bestDeviation  = math.huge
    local bestDYaw       = 0.0
    local bestDPitch     = 0.0
    local bestTargetYaw  = currentYaw
    local bestTargetPitch= currentPitch
    local bestEX, bestEY, bestEZ = nil, nil, nil

    for _, scriptName in ipairs(enemyNames) do
        local entities = Engine.FindEntitiesWithScript(scriptName)
        if entities then
            for i = 1, #entities do
                local entityId = entities[i]
                local isDead = self._deadEnemies and self._deadEnemies[entityId]
                if not isDead and not (Engine.IsEntityActive and not Engine.IsEntityActive(entityId)) then
                    local ex, ey, ez = Engine.GetEntityPosition(entityId)
                    if ex then
                        -- Use camera position as origin for angular calculation
                        -- so the camera crosshair aligns with the enemy
                        local dx = ex - camX
                        local dy = (ey + heightOffset) - camY
                        local dz = ez - camZ
                        local distSq = dx*dx + dy*dy + dz*dz
                        if distSq <= assistRange * assistRange
                        and hasLineOfSight(camX, camY, camZ, ex, ey + heightOffset, ez) then
                            local len3d = math.sqrt(dx*dx + dy*dy + dz*dz)
                            if len3d > 0.01 then
                                local targetYaw   = math.deg(atan2(dx, dz))
                                local targetPitch = -math.deg(math.asin(
                                    math.max(-1.0, math.min(1.0, dy / len3d))
                                ))
                                local dYaw      = shortestDelta(currentYaw,   targetYaw)
                                local dPitch    = shortestDelta(currentPitch, targetPitch)
                                local deviation = math.sqrt(dYaw*dYaw + dPitch*dPitch)
                                if deviation < bestDeviation then
                                    bestDeviation  = deviation
                                    bestDYaw       = dYaw
                                    bestDPitch     = dPitch
                                    bestTargetYaw  = targetYaw
                                    bestTargetPitch= targetPitch
                                    bestEX = ex
                                    bestEY = ey + heightOffset
                                    bestEZ = ez
                                end
                            end
                        end
                    end
                end
            end
        end
    end

    if bestDeviation < assistAngle then
        -- Component 1: velocity tracking
        -- Measure how far the enemy's angular position moved since last frame
        -- and apply the same delta to the camera so it keeps pace automatically.
        local safeDt = math.max(dt, 0.001)
        local prevYaw   = self._assistPrevTargetYaw   or bestTargetYaw
        local prevPitch = self._assistPrevTargetPitch or bestTargetPitch
        local angVelYaw   = math.max(-180, math.min(180,
            shortestDelta(prevYaw,   bestTargetYaw)   / safeDt))
        local angVelPitch = math.max(-90,  math.min(90,
            shortestDelta(prevPitch, bestTargetPitch) / safeDt))

        local trackYaw   = angVelYaw   * dt
        local trackPitch = angVelPitch * dt

        -- Component 2: corrective pull — small fixed-speed nudge toward center.
        -- assistStrength deg/s is intentionally weak so mouse input beats it.
        local corrStep = assistStrength * dt
        local corrYaw   = math.max(-corrStep, math.min(corrStep, bestDYaw))
        local corrPitch = math.max(-corrStep, math.min(corrStep, bestDPitch))

        self._chainAimYaw   = currentYaw + trackYaw + corrYaw
        self._chainAimPitch = math.max(
            self.minPitch or -80.0,
            math.min(self.maxPitch or 80.0, currentPitch + trackPitch + corrPitch)
        )

        self._assistPrevTargetYaw   = bestTargetYaw
        self._assistPrevTargetPitch = bestTargetPitch
        -- Store enemy world position so ChainAim_basis can fire toward it from player pos
        self._assistTargetX = bestEX
        self._assistTargetY = bestEY
        self._assistTargetZ = bestEZ
    else
        -- Enemy left the window — clear velocity memory so there's no stale jump
        self._assistPrevTargetYaw   = nil
        self._assistPrevTargetPitch = nil
        self._assistTargetX = nil
        self._assistTargetY = nil
        self._assistTargetZ = nil
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

        -- Blend toward chain-aim rotation (shortest path to avoid 360° spin)
        local blendedYaw   = orbitYaw   + shortestDelta(orbitYaw, self._chainAimYaw) * blend
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
