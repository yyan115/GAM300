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

-- Aim visibility uses solid world geometry; enemy/player capsules and debris
-- must not obscure their own target or require guessed collider tolerances.
local function hasLineOfSight(ox, oy, oz, ex, ey, ez)
    if not (Physics and Physics.RaycastLOS) then return false end
    local dx, dy, dz = ex - ox, ey - oy, ez - oz
    local distance = math.sqrt(dx * dx + dy * dy + dz * dz)
    if distance < 0.01 then return true end
    local hit = Physics.RaycastLOS(ox, oy, oz, dx / distance, dy / distance, dz / distance, distance)
    return not hit or hit < 0 or hit >= distance
end

function M.clearAssist(self)
    self._assistTargetId = nil
    self._assistTargetX = nil
    self._assistTargetY = nil
    self._assistTargetZ = nil
    self._assistPrevTargetYaw = nil
    self._assistPrevTargetPitch = nil
end

-- Advance the chain-aim blend each frame and compute the aim camera position.
-- Returns: chainAimActive (bool), chainDesiredX, chainDesiredY, chainDesiredZ
-- chainDesiredX/Y/Z are nil when chainAimActive is false.
function M.updateChainAim(self, dt)
    if not self._chainAiming or not self.chainAimAssistEnabled then
        M.clearAssist(self)
    end
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
    if self.chainAimAssistEnabled and self._chainAiming and self._chainAimYaw and not manuallyAiming then
        M.updateAimAssist(self, dt, camX, camY, camZ)
    elseif manuallyAiming then
        -- A touch drag owns both the camera and the eventual throw direction.
        M.clearAssist(self)
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

        -- Determine if crosshair is over an enemy: aim assist locked OR raycast hit
        local crosshairOnEnemy = self._assistTargetX ~= nil
        if not crosshairOnEnemy
           and Physics and Physics.RaycastGetEntity
           and Engine and Engine.GetEntityTag and Engine.GetParentEntity then
            local ok, r1, r2 = pcall(Physics.RaycastGetEntity, camX, camY, camZ, fx, fy, fz, 100.0)
            if ok then
                local hitId = r2 or -1
                if type(r1) == "table" or type(r1) == "userdata" then
                    hitId = r1.entityId or r1[2] or -1
                end
                if hitId and hitId >= 0 then
                    local rootId = hitId
                    local safety = 20
                    while safety > 0 do
                        local pOk, parentId = pcall(Engine.GetParentEntity, rootId)
                        if pOk and parentId and parentId >= 0 then
                            rootId = parentId
                        else
                            break
                        end
                        safety = safety - 1
                    end
                    local tOk, tag = pcall(Engine.GetEntityTag, rootId)
                    if tOk and (tag == "Enemy" or tag == "Boss") then
                        if not (self._deadEnemies and self._deadEnemies[rootId]) then
                            crosshairOnEnemy = true
                        end
                    end
                end
            end
        end

        if event_bus and event_bus.publish then
            event_bus.publish("ChainAim_basis", { forward = { x = fx, y = fy, z = fz } })
            event_bus.publish("ChainAim_worldTarget", { x = wx, y = wy, z = wz })
            event_bus.publish("chain.crosshair_on_enemy", { active = crosshairOnEnemy })
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

-- Retain a visible, reachable enemy inside the assist window. Tracking history
-- belongs to that entity, so acquiring another enemy cannot turn a target switch
-- into artificial angular velocity.
function M.updateAimAssist(self, dt, camX, camY, camZ)
    if not self.chainAimAssistEnabled
        or not (Engine and Engine.FindEntitiesWithScript and Engine.GetEntityPosition) then
        M.clearAssist(self)
        return
    end
    dt = math.max(dt, 0.0)
    local assistAngle = self.chainAimAssistAngle or 30.0
    local assistStrength = self.chainAimAssistStrength or 15.0
    local assistRange = self.chainAimAssistRange or 12.0
    local heightOffset = self.chainAimAssistHeightOffset or 1.0
    local currentYaw, currentPitch = self._chainAimYaw, self._chainAimPitch or 0.0

    -- ChainBootstrap supplies its authored reach and launch entity when aiming
    -- starts. Read the entity's current position because the hand moves while
    -- the player aims; a cached starting position would drift out of date.
    local originX, originY, originZ
    local originId = self._chainAimOriginEntity
    if originId and (not Engine.IsEntityActive or Engine.IsEntityActive(originId)) then
        originX, originY, originZ = Engine.GetEntityPosition(originId)
    end
    if not originX then
        originX, originY, originZ = self._targetPos.x,
            self._targetPos.y + (self.chainAimHeightOffset or 1.5), self._targetPos.z
    end
    local reach = self._chainAimMaxLength or assistRange

    local function candidate(id)
        if self._deadEnemies and self._deadEnemies[id] then return nil end
        if Engine.IsEntityActive and not Engine.IsEntityActive(id) then return nil end
        if Engine.GetEntityTag then
            local tag = Engine.GetEntityTag(id)
            if tag ~= "Enemy" and tag ~= "Boss" then return nil end
        end
        local x, y, z = Engine.GetEntityPosition(id)
        if not x then return nil end
        y = y + heightOffset
        local dx, dy, dz = x - camX, y - camY, z - camZ
        local distanceSquared = dx * dx + dy * dy + dz * dz
        local rx, ry, rz = x - originX, y - originY, z - originZ
        if distanceSquared > assistRange * assistRange
            or rx * rx + ry * ry + rz * rz > reach * reach then return nil end
        local distance = math.sqrt(distanceSquared)
        if distance < 0.01 then return nil end
        local yaw = math.deg(atan2(dx, dz))
        local pitch = -math.deg(math.asin(utils.clamp(dy / distance, -1.0, 1.0)))
        local dYaw, dPitch = shortestDelta(currentYaw, yaw), shortestDelta(currentPitch, pitch)
        local deviation = math.sqrt(dYaw * dYaw + dPitch * dPitch)
        if deviation >= assistAngle then return nil end
        if not hasLineOfSight(camX, camY, camZ, x, y, z)
            or not hasLineOfSight(originX, originY, originZ, x, y, z) then return nil end
        return {id = id, x = x, y = y, z = z, yaw = yaw, pitch = pitch,
            dYaw = dYaw, dPitch = dPitch, deviation = deviation, distance = distanceSquared}
    end

    local best = self._assistTargetId and candidate(self._assistTargetId)
    if not best then
        local visited = {}
        for _, scriptName in ipairs(self.chainAimAssistComponents or {}) do
            for _, id in ipairs(Engine.FindEntitiesWithScript(scriptName) or {}) do
                if not visited[id] then
                    visited[id] = true
                    local target = candidate(id)
                    if target and (not best or target.deviation < best.deviation
                        or (target.deviation == best.deviation and (target.distance < best.distance
                            or (target.distance == best.distance and target.id < best.id)))) then
                        best = target
                    end
                end
            end
        end
    end
    if not best then
        M.clearAssist(self)
        return
    end

    if best.id ~= self._assistTargetId then
        self._assistPrevTargetYaw = nil
        self._assistPrevTargetPitch = nil
    end
    local safeDt = math.max(dt, 0.001)
    local trackYaw = utils.clamp(shortestDelta(self._assistPrevTargetYaw or best.yaw, best.yaw)
        / safeDt, -180.0, 180.0) * dt
    local trackPitch = utils.clamp(shortestDelta(self._assistPrevTargetPitch or best.pitch, best.pitch)
        / safeDt, -90.0, 90.0) * dt
    local correction = assistStrength * dt
    self._chainAimYaw = currentYaw + trackYaw + utils.clamp(best.dYaw, -correction, correction)
    self._chainAimPitch = utils.clamp(currentPitch + trackPitch
        + utils.clamp(best.dPitch, -correction, correction), self.minPitch or -80.0, self.maxPitch or 80.0)
    self._assistTargetId = best.id
    self._assistPrevTargetYaw, self._assistPrevTargetPitch = best.yaw, best.pitch
    self._assistTargetX, self._assistTargetY, self._assistTargetZ = best.x, best.y, best.z
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
