-- camera_follow.lua
-- Third-person camera with orbit controls, action mode, chain aiming, and collision detection
-- NEW: Added automatic enemy detection system for action mode triggering (NO CACHING)
require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

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
        actionModeEnabled   = true,
        actionModeKey       = "Attack",
        actionModePitch     = 25.0,
        actionModeDistance  = 3.5,
        actionModeTransition = 8.0,
        actionModeLockRotation = false,  -- Set to true to lock camera rotation when in action mode
        -- Chain mode aim settings
        chainAimTargetName = "ChainAimPoint",
        chainAimTransitionSpeed = 5.0,
        -- Camera rotation lock
        lockCameraRotation  = false,
        
        -- Enemy Detection Settings
        enableEnemyDetection = true,
        enemyDetectionRange  = 8.0,
        enemyDisengageRange  = 10.0,
        enemyDisengageDelay  = 2.0,
        enemyScriptNames = {"EnemyAI", "FlyingEnemyLogic"},
        cacheUpdateInterval = 1.0,  -- C++ cache update interval (seconds)
        debugEnemyDetection = false,       
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
        
        --  Enemy detection state
        self._enemyInRange = false               -- Is an enemy currently within detection range?
        self._enemyDisengageTimer = 0.0          -- Timer for delayed action mode exit
        self._enemyTriggeredActionMode = false   -- Did an enemy trigger action mode (vs manual)?
        self._triggeringEnemyId = nil            -- tracks which enemy triggered action mode

        -- Configure C++ cache intervals
        if Engine and Engine.SetCacheUpdateInterval then
            for _, scriptName in ipairs(self.enemyScriptNames) do
                Engine.SetCacheUpdateInterval(scriptName, self.cacheUpdateInterval)
                if self.debugEnemyDetection then
                    print(string.format("[CameraFollow] Set cache interval for '%s' to %.1fs", 
                                      scriptName, self.cacheUpdateInterval))
                end
            end
        end

        if self.debugEnemyDetection then
            print("[CameraFollow] Initialized with enemy detection enabled (NO CACHING)")
        end

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
                self._chainAiming = payload.active or false
                if not self._chainAiming then
                    self._chainAimPos = nil
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
    
    -- Find the closest enemy to the player
    _findClosestEnemy = function(self)
        if not self._hasTarget or not Engine or not Engine.FindEntitiesWithScript or not Engine.GetEntityPosition then
            return nil, math.huge
        end
        
        local playerX = self._targetPos.x
        local playerZ = self._targetPos.z
        local closestDist = math.huge
        local closestEnemy = nil
        
        -- C++ returns cached results (updated by UpdateCacheTiming)
        for _, scriptName in ipairs(self.enemyScriptNames) do
            local entities = Engine.FindEntitiesWithScript(scriptName)
            
            if entities and #entities > 0 then
                for i = 1, #entities do
                    local entityId = entities[i]
                    local x, y, z = Engine.GetEntityPosition(entityId)
                    
                    if x and y and z then
                        local dx = x - playerX
                        local dz = z - playerZ
                        local dist = math.sqrt(dx * dx + dz * dz)
                        
                        if dist < closestDist then
                            closestDist = dist
                            closestEnemy = entityId
                        end
                    end
                end
            end
        end
        
        return closestEnemy, closestDist
    end,
    
    -- NEW: Get distance to a specific enemy by ID
    _getDistanceToEnemy = function(self, enemyId)
        if not enemyId or not Engine or not Engine.GetEntityPosition then
            return math.huge
        end
        
        local x, y, z = Engine.GetEntityPosition(enemyId)
        if not x or not y or not z then
            return math.huge
        end
        
        local playerX = self._targetPos.x
        local playerZ = self._targetPos.z
        
        local dx = x - playerX
        local dz = z - playerZ
        
        return math.sqrt(dx * dx + dz * dz)
    end,
    
    -- FIXED: Properly handles action mode distance checking
    _updateEnemyProximity = function(self, dt)
        if not self.enableEnemyDetection then return end
        
        -- ==========================================
        -- CASE 1: Currently in action mode (triggered by enemy)
        -- ==========================================
        if self._actionModeActive and self._enemyTriggeredActionMode then
            -- Check distance to the ORIGINAL triggering enemy
            if self._triggeringEnemyId then
                local distToTrigger = self:_getDistanceToEnemy(self._triggeringEnemyId)
                
                if self.debugEnemyDetection then
                    print(string.format("[CameraFollow] Action mode active - distance to trigger enemy: %.1f", distToTrigger))
                end
                
                -- Enemy is beyond disengage range
                if distToTrigger > self.enemyDisengageRange then
                    self._enemyDisengageTimer = self._enemyDisengageTimer + dt
                    
                    if self.debugEnemyDetection and self._enemyDisengageTimer > 0.1 then
                        print(string.format("[CameraFollow] Enemy beyond disengage range - timer: %.1fs / %.1fs", 
                                          self._enemyDisengageTimer, self.enemyDisengageDelay))
                    end
                    
                    -- Timer expired - exit action mode
                    if self._enemyDisengageTimer >= self.enemyDisengageDelay then
                        self._actionModeActive = false
                        self._enemyTriggeredActionMode = false
                        self._enemyDisengageTimer = 0.0
                        self._triggeringEnemyId = nil
                        
                        if self.debugEnemyDetection then
                            print("[CameraFollow]  Action Mode DISABLED - enemy left range ")
                        end
                    end
                else
                    -- Enemy came back within disengage range - reset timer
                    if self._enemyDisengageTimer > 0 and self.debugEnemyDetection then
                        print("[CameraFollow] Enemy returned within range - timer reset")
                    end
                    self._enemyDisengageTimer = 0.0
                end
            else
                -- Lost track of triggering enemy - search for any enemy
                local closestEnemy, closestDist = self:_findClosestEnemy()
                
                if closestEnemy and closestDist <= self.enemyDisengageRange then
                    -- Found a close enemy - track it
                    self._triggeringEnemyId = closestEnemy
                    self._enemyDisengageTimer = 0.0
                    
                    if self.debugEnemyDetection then
                        print(string.format("[CameraFollow] Locked onto new enemy: %d at %.1fu", closestEnemy, closestDist))
                    end
                else
                    -- No enemies in range - exit action mode
                    self._actionModeActive = false
                    self._enemyTriggeredActionMode = false
                    self._triggeringEnemyId = nil
                    
                    if self.debugEnemyDetection then
                        print("[CameraFollow] Action Mode DISABLED - no enemies found")
                    end
                end
            end
            
            return  -- EXIT - don't search for new enemies while in action mode
        end
        
        -- ==========================================
        -- CASE 2: Not in action mode - search for enemies
        -- ==========================================
        local closestEnemy, closestDist = self:_findClosestEnemy()
        
        local wasInRange = self._enemyInRange
        self._enemyInRange = closestEnemy ~= nil and closestDist <= self.enemyDetectionRange
        
        if self.debugEnemyDetection and self._enemyInRange ~= wasInRange then
            print(string.format("[CameraFollow] Enemy proximity changed: %s (%.1fu)", 
                              tostring(self._enemyInRange), closestDist))
        end
        
        -- Enemy entered detection range - trigger action mode
        if self._enemyInRange then
            self._enemyDisengageTimer = 0.0
            
            if not self._actionModeActive then
                self._actionModeActive = true
                self._enemyTriggeredActionMode = true
                self._triggeringEnemyId = closestEnemy  -- NEW: Remember which enemy triggered it
                
                if self.debugEnemyDetection then
                    print(string.format("[CameraFollow] Action Mode ENABLED by enemy %d at %.1fu", 
                                      closestEnemy, closestDist))
                end
            end
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

        local shouldLockRotation = self.lockCameraRotation or (self._actionModeActive and self.actionModeLockRotation)
        
        if not shouldLockRotation then
            -- Normal behavior - update yaw and pitch
            self._yaw   = self._yaw   - xoffset  -- Subtract for correct left/right direction
            self._pitch = clamp(self._pitch + yoffset, self.minPitch or -80.0, self.maxPitch or 80.0)  -- Add for correct up/down direction
            
            -- Track normal pitch when not in action mode
            if not self._actionModeActive then
                self._normalPitch = self._pitch
            end
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

        -- ==========================================
        -- CRITICAL: Update C++ cache timing FIRST
        -- This accumulates deltaTime for all caches
        -- ==========================================
        if Engine and Engine.UpdateCacheTiming then
            Engine.UpdateCacheTiming(dt)
        end

        -- Update enemy proximity
        self:_updateEnemyProximity(dt)

        -- Cooldown timer
        if self._toggleCooldown > 0 then
            self._toggleCooldown = self._toggleCooldown - dt
        end

        -- Action mode toggle
        if self.actionModeEnabled and Input and Input.IsActionJustPressed and Input.IsActionJustPressed(self.actionModeKey) then
            if self._toggleCooldown <= 0 then
                self._actionModeActive = not self._actionModeActive
                self._enemyTriggeredActionMode = false  -- Disable enemy control when manually toggled
                self._triggeringEnemyId = nil           -- ADD THIS LINE - clear tracked enemy
                self._toggleCooldown = 0.25
                print("[CameraFollow] Action Mode " .. (self._actionModeActive and "ENABLED" or "DISABLED"))
            end
        end

        -- Toggle cursor lock with Escape (unified input system)
        if Input and Input.IsActionJustPressed and Input.IsActionJustPressed("Pause") then
            if Screen and Screen.SetCursorLocked and Screen.IsCursorLocked then
                local isLocked = Screen.IsCursorLocked()
                Screen.SetCursorLocked(not isLocked)
                self._firstMouse = true
            end
        end

        -- Re-lock cursor when clicking Attack action
        if Input and Input.IsActionJustPressed and Input.IsActionJustPressed("Attack") then
            if Screen and Screen.SetCursorLocked and Screen.IsCursorLocked then
                if not Screen.IsCursorLocked() then
                    Screen.SetCursorLocked(true)
                    self._firstMouse = true
                end
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
            print("Chain aiming active")
                
                -- FIRST FRAME: Snap yaw/pitch to aim target rotation BEFORE doing anything else
                if not self._chainAimPos then
                    print("First frame of chain aim")
                    
                    local aimTarget = Engine.FindTransformByName(self.chainAimTargetName)
                    print("aimTarget:", aimTarget)
                    print("Engine:", Engine)
                    print("Engine.GetTransformWorldRotation:", Engine and Engine.GetTransformWorldRotation)
                    
                    if aimTarget then
                        print("aimTarget found!")
                        if Engine and Engine.GetTransformWorldRotation then
                            print("GetTransformWorldRotation exists, calling...")
                            local qw, qx, qy, qz = Engine.GetTransformWorldRotation(aimTarget)
                            print("Quaternion: qw=" .. qw .. " qx=" .. qx .. " qy=" .. qy .. " qz=" .. qz)
                            
                            if qw then
                                print("Got quaternion!")
                                -- Extract forward vector from quaternion
                                local fx = 2 * (qx*qz + qw*qy)
                                local fy = 2 * (qy*qz - qw*qx)
                                local fz = 1 - 2 * (qx*qx + qy*qy)
                                
                                print("Forward vector: fx=" .. fx .. " fy=" .. fy .. " fz=" .. fz)
                                
                                -- IMMEDIATELY update yaw/pitch
                                local horizontalLen = math.sqrt(fx*fx + fz*fz)
                                if horizontalLen > 0.0001 then
                                    self._yaw = math.deg(atan2(fx, fz))
                                    self._pitch = -math.deg(math.atan(fy / horizontalLen))
                                    print("[CameraFollow] SNAP rotation: yaw=" .. self._yaw .. " pitch=" .. self._pitch)
                                else
                                    print("horizontalLen too small:", horizontalLen)
                                end
                            else
                                print("pcall failed or qw is nil")
                            end
                        else
                            print("Engine.GetTransformWorldRotation does NOT exist")
                        end
                    else
                        print("aimTarget NOT found with name:", self.chainAimTargetName)
                    end
                else
                    print("Not first frame, _chainAimPos already exists")
                end
            
            useOrbitFollow = false
            useFreeRotation = true
            
            -- NOW get position (every frame)
            local aimTarget = Engine.FindTransformByName(self.chainAimTargetName)
            if aimTarget then
                local ax, ay, az = 0, 0, 0
                local ok, a, b, c = pcall(function()
                    if Engine and Engine.GetTransformWorldPosition then
                        return Engine.GetTransformWorldPosition(aimTarget)
                    end
                    return nil
                end)
                if ok and a ~= nil then
                    if type(a) == "table" then
                        ax, ay, az = a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
                    else
                        ax, ay, az = a, b, c
                    end
                end
                self._chainAimPos = {x = ax, y = ay, z = az}
            end
            
            if self._chainAimPos then
                desiredX = self._chainAimPos.x
                desiredY = self._chainAimPos.y
                desiredZ = self._chainAimPos.z
                
                -- Use the NOW-CORRECT yaw/pitch
                local pitchRad = math.rad(self._pitch)
                local yawRad = math.rad(self._yaw)
                
                local forwardDist = 10.0
                local fx = math.sin(yawRad) * math.cos(pitchRad)
                local fy = -math.sin(pitchRad)
                local fz = math.cos(yawRad) * math.cos(pitchRad)
                
                cameraTarget.x = desiredX + fx * forwardDist
                cameraTarget.y = desiredY + fy * forwardDist
                cameraTarget.z = desiredZ + fz * forwardDist
            else
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