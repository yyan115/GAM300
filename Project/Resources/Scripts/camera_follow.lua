-- camera_follow.lua
require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

local POST_HOLD_MULTIPLIER = 3.0         -- how much longer Action Mode lasts after attacks stop
local ATTACK_GRACE = 0.12                -- small grace window to consider rapid taps part of same attack sequence

local function clamp(x, minv, maxv)
    if x < minv then return minv end
    if x > maxv then return maxv end
    return x
end

-- portable atan2(y, x)
local function atan2(y, x)
    if math.atan2 then return math.atan2(y, x) end
    local ok, res = pcall(math.atan, y, x)
    if ok then return res end
    if x > 0 then return math.atan(y / x)
    elseif x < 0 and y >= 0 then return math.atan(y / x) + math.pi
    elseif x < 0 and y < 0 then return math.atan(y / x) - math.pi
    elseif x == 0 and y > 0 then return math.pi / 2
    elseif x == 0 and y < 0 then return -math.pi / 2
    else return 0.0 end
end

-- Helper: Euler (deg) → Quaternion
local function eulerToQuat(pitch, yaw, roll)
    local p = math.rad(pitch or 0) * 0.5
    local y = math.rad(yaw or 0) * 0.5
    local r = math.rad(roll or 0) * 0.5
    local sinP, cosP = math.sin(p), math.cos(p)
    local sinY, cosY = math.sin(y), math.cos(y)
    local sinR, cosR = math.sin(r), math.cos(r)
    return {
        w = cosP * cosY * cosR + sinP * sinY * sinR,
        x = sinP * cosY * cosR - cosP * sinY * sinR,
        y = cosP * sinY * cosR + sinP * cosY * sinR,
        z = cosP * cosY * sinR - sinP * sinY * cosR
    }
end

return Component {
    mixins = { TransformMixin },

    fields = {
        followDistance   = 2.0,
        heightOffset     = 1.0,
        followLerp       = 10.0,
        mouseSensitivity = 0.15,
        minPitch         = -30.0,
        maxPitch         = 60.0,
        minZoom          = 2.0,
        maxZoom          = 15.0,
        zoomSpeed        = 1.0,
        -- Camera collision settings
        collisionEnabled = true,
        collisionOffset  = 0.2,    -- How far to pull camera in front of hit point
        collisionLerpIn  = 20.0,   -- Fast snap when hitting wall
        collisionLerpOut = 5.0,    -- Slower ease when wall clears
        -- Action mode settings
        actionModeEnabled   = false,
        actionModeDuration  = 3.0,
        actionModePitch     = 25.0,
        actionModeDistance  = 3.5,
        actionModeTransition = 8.0,
        actionModeLockRotation = false,  -- Set to true to lock camera rotation when in action mode
        -- Chain mode aim settings
        chainAimPosName = "ChainAimPointLeft",
        chainAimTargetName = "ChainAimPointLeftEnd",
        chainAimTransitionSpeed = 5.0,
        -- Camera rotation lock
        lockCameraRotation  = false,
    },

    Awake = function(self)
        self._yaw   = 180.0   -- start orbit rotated 180° around Y
        self._pitch = 15.0    -- keep a slight downward tilt

        self._targetPos  = { x = 0.0, y = 0.0, z = 0.0 }
        self._hasTarget  = false
        self._posSub     = nil
        self._lastMouseX = 0.0
        self._lastMouseY = 0.0
        self._firstMouse = true
        self._currentCollisionDist = nil  -- Track current collision-adjusted distance
        
        -- Action mode state
        self._actionModeActive = false
        self._normalPitch = 15.0
        self._normalDistance = 2.0
        self._toggleCooldown = 0.0

        -- Chain mode aim settings
        self._chainAiming = false
        self._chainAimPos = nil
        self._normalCameraPos = nil
        self._chainAimInitialized = false

        if event_bus and event_bus.subscribe then
            self._posSub = event_bus.subscribe("player_position", function(payload)
                if not payload then return end
                local x = payload.x or payload[1] or 0.0
                local y = payload.y or payload[2] or 0.0
                local z = payload.z or payload[3] or 0.0
                -- Store raw position, height offset applied in Update based on zoom
                self._targetPos.x, self._targetPos.y, self._targetPos.z = x, y, z

                -- Lock cursor only when we first receive player position (entering gameplay)
                -- This prevents cursor lock in menu scenes where there's no player
                if not self._hasTarget then
                    if Screen and Screen.SetCursorLocked then
                        Screen.SetCursorLocked(true)
                    end
                end
                self._hasTarget = true
            end)    

            self._chainAimSub = event_bus.subscribe("chain.aim_camera", function(payload)
            if not payload then return end
            local wasAiming = self._chainAiming
            self._chainAiming = payload.active or false
            
            -- Reset initialization flag when entering chain aim mode
            if self._chainAiming and not wasAiming then
                self._chainAimInitialized = false
            end
            end)
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe and self._posSub then
            event_bus.unsubscribe(self._posSub)
            self._posSub = nil
        end

        if event_bus and event_bus.unsubscribe and self._chainAimSub then
            event_bus.unsubscribe(self._chainAimSub)
            self._chainAimSub = nil
        end

        -- Unlock cursor when camera is disabled
        if Screen and Screen.SetCursorLocked then
            Screen.SetCursorLocked(false)
        end
    end,

    _updateScrollZoom = function(self)
        if not (Input and Input.GetScrollY) then return end

        local scrollY = Input.GetScrollY()
        if scrollY ~= 0 then
            local zoomSpeed = self.zoomSpeed or 1.0
            self._normalDistance = self._normalDistance - scrollY * zoomSpeed
            self._normalDistance = clamp(self._normalDistance, self.minZoom or 2.0, self.maxZoom or 15.0)
            
            if not self._actionModeActive then
                self.followDistance = self._normalDistance
            end

            -- Consume scroll so it doesn't accumulate
            if Input.ConsumeScroll then
                Input.ConsumeScroll()
            end
        end
    end,

    _updateMouseLook = function(self, dt)
        -- Process mouse look for camera rotation using unified input
        if not (Input and Input.GetAxis) then
            print("[CameraFollow] ERROR: Input.GetAxis not available")
            return
        end

        -- Get mouse delta from "Look" axis (configured as mouse_delta on desktop, touch_drag on Android)
        local lookAxis = Input.GetAxis("Look")
        if not lookAxis then
            print("[CameraFollow] Look axis returned nil")
            return
        end

        -- Check platform for sensitivity adjustment
        -- Android uses normalized coords (0-1), desktop uses pixel delta - need different sensitivity
        local isAndroid = Platform and Platform.IsAndroid and Platform.IsAndroid()
        local baseSensitivity = self.mouseSensitivity or 0.15

        -- Android touch coords are normalized (0-1), so we need MUCH higher sensitivity
        -- Desktop mouse delta is in pixels, so lower sensitivity works
        local sensitivity = isAndroid and 800.0 or baseSensitivity  -- Direct value for Android

        local xoffset = lookAxis.x * sensitivity
        local yoffset = lookAxis.y * sensitivity

        -- Debug: Log actual values being applied
        if not self._logCount then self._logCount = 0 end
        self._logCount = self._logCount + 1
        if self._logCount % 30 == 1 and (lookAxis.x ~= 0 or lookAxis.y ~= 0) then
            print("[CameraFollow] SENS=" .. sensitivity .. " offset=(" .. xoffset .. "," .. yoffset .. ") yaw=" .. self._yaw)
        end

        self._yaw   = self._yaw   - xoffset  -- Subtract for correct left/right direction
        self._pitch = clamp(self._pitch + yoffset, self.minPitch or -80.0, self.maxPitch or 80.0)  -- Add for correct up/down direction
        
        -- Track normal pitch when not in action mode
        if not self._actionModeActive then
            self._normalPitch = self._pitch
        end

        -- Store camera yaw in global for player movement (bypass event_bus)
        _G.CAMERA_YAW = self._yaw

        -- Also publish via event_bus for backwards compatibility
        if event_bus and event_bus.publish then
            event_bus.publish("camera_yaw", self._yaw)
        end
    end,

    Update = function(self, dt)
        if not (self.GetPosition and self.SetPosition and self.SetRotation) then return end
        if not self._hasTarget then return end

        -- Cooldown timer
        if self._toggleCooldown > 0 then
            self._toggleCooldown = self._toggleCooldown - dt
        end

        -----------------------------------------------------------------------------------------------------------
        -- Action mode toggle
        -- ensure the timer variable exists
        self._actionModeTimer = self._actionModeTimer or 0.0

        -- Extended duration after attacks finish
        local extendedDuration = (self.actionModeDuration or 3.0) * POST_HOLD_MULTIPLIER

        -- Attack pressed or held -> (re)start the timer and enable action mode
        if Input then
            if (Input.IsActionJustPressed and Input.IsActionJustPressed("Attack")) or
            (Input.IsActionPressed and Input.IsActionPressed("Attack")) then

                -- re-lock cursor (existing behaviour)
                if Screen and Screen.SetCursorLocked and Screen.IsCursorLocked then
                    if not Screen.IsCursorLocked() then
                        Screen.SetCursorLocked(true)
                        self._firstMouse = true
                    end
                end

                -- reset timer and enable action mode
                self._actionModeTimer = extendedDuration
                self._actionModeActive = true
            end
        end

        -- countdown the single timer and disable when expired
        if self._actionModeTimer > 0 then
            self._actionModeTimer = self._actionModeTimer - dt
            if self._actionModeTimer <= 0 then
                self._actionModeTimer = 0
                self._actionModeActive = false
                -- optional debug:
                -- print("[CameraFollow] Action Mode DISABLED (timer expired)")
            end
        end

        -- Only update camera look when cursor is locked (desktop) or always on Android
        local isAndroid = Platform and Platform.IsAndroid and Platform.IsAndroid()

        if not self._loggedPlatform then
            print("[CameraFollow] isAndroid=" .. tostring(isAndroid))
            self._loggedPlatform = true
        end

        if isAndroid or (Screen and Screen.IsCursorLocked and Screen.IsCursorLocked()) then
            local shouldLockRotation = self.lockCameraRotation or (self._actionModeActive and self.actionModeLockRotation)
            
            if not shouldLockRotation then
                self:_updateMouseLook(dt)
            end
        end

        -- Update scroll zoom
        self:_updateScrollZoom()

        -- Action mode smooth transition
        local targetPitch = self._actionModeActive and self.actionModePitch or self._normalPitch
        local targetDistance = self._actionModeActive and self.actionModeDistance or self._normalDistance
        local transitionSpeed = self.actionModeTransition or 8.0
        local t = 1.0 - math.exp(-transitionSpeed * dt)
        
        self._pitch = self._pitch + (targetPitch - self._pitch) * t
        self.followDistance = self.followDistance + (targetDistance - self.followDistance) * t

        -- ==========================================
        -- CAMERA TARGET CALCULATION (Extensible)
        -- ==========================================
        local cameraTarget = { x = 0, y = 0, z = 0 }  -- Where camera looks at
        local desiredX, desiredY, desiredZ             -- Where camera should be positioned
        local useOrbitFollow = true                    -- Default: orbit around player
        local useFreeRotation = false                  -- Use yaw/pitch for look direction
        
        -- PRIORITY 1: Chain aiming mode (overrides orbit)
        if self._chainAiming then
          -- Get both transform positions
          local aimPos = Engine.FindTransformByName(self.chainAimPosName)
          local aimTarget = Engine.FindTransformByName(self.chainAimTargetName)
          
          if aimPos and aimTarget then
            -- Get position of camera anchor point (where camera should be)
            local camX, camY, camZ = 0, 0, 0
            local ok, a, b, c = pcall(function()
              if Engine and Engine.GetTransformWorldPosition then
                return Engine.GetTransformWorldPosition(aimPos)
              end
              return nil
            end)
            
            if ok and a ~= nil then
              if type(a) == "table" then
                camX, camY, camZ = a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
              else
                camX, camY, camZ = a, b, c
              end
            end
            
            -- ONLY ON FIRST FRAME: Lock camera rotation to look at target
            if not self._chainAimInitialized then
              -- Get position of look-at target
              local targetX, targetY, targetZ = 0, 0, 0
              ok, a, b, c = pcall(function()
                if Engine and Engine.GetTransformWorldPosition then
                  return Engine.GetTransformWorldPosition(aimTarget)
                end
                return nil
              end)
              
              if ok and a ~= nil then
                if type(a) == "table" then
                  targetX, targetY, targetZ = a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
                else
                  targetX, targetY, targetZ = a, b, c
                end
              end
              
              -- Calculate direction from camera position to target
              local dirX = targetX - camX
              local dirY = targetY - camY
              local dirZ = targetZ - camZ
              
              local dirLen = math.sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ)
              
              if dirLen > 0.0001 then
                -- Normalize direction
                dirX, dirY, dirZ = dirX/dirLen, dirY/dirLen, dirZ/dirLen
                
                -- Calculate yaw (rotation around Y axis)
                self._yaw = math.deg(atan2(dirX, dirZ))
                
                -- Calculate pitch (rotation around X axis)
                self._pitch = -math.deg(math.asin(dirY))
                
                -- Mark as initialized so we don't re-lock
                self._chainAimInitialized = true
              end
            end
            
            -- Set camera position (every frame)
            desiredX = camX
            desiredY = camY
            desiredZ = camZ
            
            -- Camera now uses free rotation (yaw/pitch from mouse input)
            useOrbitFollow = false
            useFreeRotation = true

            -- Publish camera basis for chain
            local yaw_rad = math.rad(self._yaw)
            local pitch_rad = math.rad(self._pitch)
            local fx = math.sin(yaw_rad) * math.cos(pitch_rad)
            local fy = -math.sin(pitch_rad)  -- negative if pitch up = negative
            local fz = math.cos(yaw_rad) * math.cos(pitch_rad)
            if event_bus and event_bus.publish then
                event_bus.publish("ChainAim_basis", {
                    forward = { x = fx, y = fy, z = fz },
                })
            end
          else
            -- Transforms not found, fall back to orbit
            useOrbitFollow = true
            useFreeRotation = false
          end
        end
        
        -- DEFAULT: Player orbit follow
        if useOrbitFollow then
            local radius = self.followDistance or 5.0
            local pitchRad = math.rad(self._pitch)
            local yawRad = math.rad(self._yaw)

            -- Scale height offset based on zoom distance
            local minZoom = self.minZoom or 2.0
            local maxZoom = self.maxZoom or 15.0
            local zoomFactor = (radius - minZoom) / (maxZoom - minZoom)
            zoomFactor = clamp(zoomFactor, 0.0, 1.0)

            -- Target look-at point
            local lookAtHeight = 0.5 + zoomFactor * 0.7
            cameraTarget.x = self._targetPos.x
            cameraTarget.y = self._targetPos.y + lookAtHeight
            cameraTarget.z = self._targetPos.z

            -- Publish camera basis for player movement
            if event_bus and event_bus.publish then
                local fx = math.sin(yawRad)
                local fz = math.cos(yawRad)
                event_bus.publish("camera_basis", {
                    forward = { x = fx, y = 0.0, z = fz },
                })
            end

            -- Camera position offset
            local baseHeightOffset = self.heightOffset or 1.0
            local scaledHeightOffset = baseHeightOffset * (0.3 + zoomFactor * 0.7)
            local horizontalRadius = radius * math.cos(pitchRad)
            local offsetX = horizontalRadius * math.sin(yawRad)
            local offsetZ = horizontalRadius * math.cos(yawRad)
            local offsetY = radius * math.sin(pitchRad) + scaledHeightOffset

            desiredX = cameraTarget.x + offsetX
            desiredY = cameraTarget.y + offsetY
            desiredZ = cameraTarget.z + offsetZ
        end

        -- ==========================================
        -- CAMERA COLLISION (only for orbit mode)
        -- ==========================================
        if useOrbitFollow and self.collisionEnabled and Physics and Physics.Raycast then
            local tx, ty, tz = cameraTarget.x, cameraTarget.y, cameraTarget.z
            local dirX = desiredX - tx
            local dirY = desiredY - ty
            local dirZ = desiredZ - tz
            local dirLen = math.sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ)

            if dirLen > 0.001 then
                dirX, dirY, dirZ = dirX/dirLen, dirY/dirLen, dirZ/dirLen

                local dist = Physics.Raycast(
                    tx, ty, tz,
                    dirX, dirY, dirZ,
                    dirLen + 0.5
                )

                local hit = (dist >= 0 and dist < dirLen)
                local collisionOffset = self.collisionOffset or 0.2
                local targetDist = dirLen

                if hit then
                    targetDist = math.max(dist - collisionOffset, 0.5)
                end

                if self._currentCollisionDist == nil then
                    self._currentCollisionDist = targetDist
                else
                    local lerpSpeed = targetDist < self._currentCollisionDist and (self.collisionLerpIn or 20.0) or (self.collisionLerpOut or 5.0)
                    local collisionT = 1.0 - math.exp(-lerpSpeed * dt)
                    self._currentCollisionDist = self._currentCollisionDist + (targetDist - self._currentCollisionDist) * collisionT
                end

                local adjustedDist = self._currentCollisionDist
                desiredX = tx + dirX * adjustedDist
                desiredY = ty + dirY * adjustedDist
                desiredZ = tz + dirZ * adjustedDist
            end
        end

        -- ==========================================
        -- SMOOTH FOLLOW & ROTATION
        -- ==========================================
        local cx, cy, cz = 0.0, 0.0, 0.0
        local px, py, pz = self:GetPosition()
        if type(px) == "table" then
            cx, cy, cz = px.x or 0.0, px.y or 0.0, px.z or 0.0
        else
            cx, cy, cz = px or 0.0, py or 0.0, pz or 0.0
        end

        -- Smooth lerp to desired position
        local followSpeed = self._chainAiming and (self.chainAimTransitionSpeed or 5.0) or (self.followLerp or 10.0)
        local lerpT = 1.0 - math.exp(-followSpeed * dt)
        local newX = cx + (desiredX - cx) * lerpT
        local newY = cy + (desiredY - cy) * lerpT
        local newZ = cz + (desiredZ - cz) * lerpT
        self:SetPosition(newX, newY, newZ)

        -- Set rotation based on mode
        if useFreeRotation then
            -- Use current yaw/pitch directly (camera looks forward from its position)
            local quat = eulerToQuat(self._pitch, self._yaw, 0.0)
            self:SetRotation(quat.w, quat.x, quat.y, quat.z)
        else
            -- Look at target (orbit mode)
            local fx, fy, fz = cameraTarget.x - newX, cameraTarget.y - newY, cameraTarget.z - newZ
            local flen = math.sqrt(fx*fx + fy*fy + fz*fz)
            if flen > 0.0001 then
                fx, fy, fz = fx/flen, fy/flen, fz/flen
                local yawDeg   = math.deg(atan2(fx, fz))
                local pitchDeg = -math.deg(math.asin(fy))
                local quat = eulerToQuat(pitchDeg, yawDeg, 0.0)
                self:SetRotation(quat.w, quat.x, quat.y, quat.z)
            end
        end

        self.isDirty = true
    end,
}