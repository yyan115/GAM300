-- camera_follow.lua
-- Third-person orbit camera — orchestrates focused sub-modules.
--
--  Camera/camera_utils.lua           → math helpers (clamp, atan2, eulerToQuat)
--  Camera/camera_enemy_detection.lua → action-mode triggering via enemy proximity
--  Camera/camera_input.lua           → mouse look + scroll zoom
--  Camera/camera_cinematic_mode.lua  → scripted cinematic cuts
--  Camera/camera_chain_aim.lua       → chain-aim position/rotation blending
--  Camera/camera_collision.lua       → raycast wall-avoidance

require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local utils     = require("Camera.camera_utils")
local EnemyDet  = require("Camera.camera_enemy_detection")
local CamInput  = require("Camera.camera_input")
local Cinematic = require("Camera.camera_cinematic_mode")
local ChainAim  = require("Camera.camera_chain_aim")
local Collision  = require("Camera.camera_collision")
local SlamTilt   = require("Camera.camera_slam_tilt")
local LockOn     = require("Camera.camera_lockon")

local clamp = utils.clamp

local event_bus          = _G.event_bus
local POST_HOLD_MULTIPLIER = 3.0

-- ─────────────────────────────────────────────────────────────────────────────

return Component {
    mixins = { TransformMixin },

    fields = {
        -- === Follow ===
        heightOffset     = 1.0,
        followLerp       = 10.0,
        mouseSensitivity = 0.15,
        minPitch         = -30.0,
        maxPitch         = 60.0,
        minZoom          = 2.0,
        maxZoom          = 15.0,
        zoomSpeed        = 1.0,
        zoomLerpSpeed    = 6.0,

        -- === Collision ===
        collisionEnabled             = true,
        collisionOffset              = 0.2,
        collisionLerpIn              = 20.0,
        collisionLerpOut             = 5.0,
        maxCameraHeightAbovePlayer   = 4.0,

        -- === Action Mode ===
        actionModeEnabled      = false,
        actionModeDuration     = 3.0,
        actionModePitch        = 25.0,
        actionModeDistance     = 3.5,
        actionModeTransition   = 8.0,
        actionModeLockRotation = false,

        -- === Chain Aim ===
        chainAimPosName         = "ChainAimPointLeft",
        chainAimTargetName      = "ChainAimPointLeftEnd",
        chainAimTransitionSpeed = 5.0,
        chainAimZoomDistance    = 0.8,
        chainAimHeightOffset    = 1.5,
        chainAimSideOffset      = 0.3,

        -- === Chain Aim Assist ===
        chainAimAssistComponents   = {"EnemyAI", "FlyingEnemyLogic"},
        chainAimAssistAngle        = 30.0,
        chainAimAssistStrength     = 15.0,
        chainAimAssistRange        = 12.0,
        chainAimAssistHeightOffset = 0.5,

        -- === Slam Tilt ===
        TriggerSlam        = false,
        SlamGroundAngle    = 60.0,
        SlamReturnAngle    = 0.0,
        SlamDownDuration   = 0.15,
        SlamReturnDuration = 0.25,

        -- === Screen Shake ===
        TriggerShake       = false,
        ShakeIntensity     = 0.3,
        ShakeDuration      = 0.4,
        ShakeFrequency     = 25.0,

        -- === Lock-On ===
        lockOnBreakDistance   = 15.0,
        lockOnRotSpeed        = 20.0,
        lockOnSnapFraction    = 0.85,
        lockOnMouseThreshold  = 2.0,
        lockOnLOSHeight       = 1.0,
        lockOnLOSGrace        = 0.5,

        -- === Motion Blur ===
        MotionBlurEnabled   = true,   -- master toggle; set false to disable without removing the system
        MotionBlurThreshold = 2.0,    -- camera speed (world units/sec) below which no blur is applied
        MotionBlurMaxSpeed  = 14.0,   -- camera speed at which blur reaches full MotionBlurMaxIntensity

        -- === Rotation ===
        lockCameraRotation = false,

        -- === Enemy Detection ===
        enableEnemyDetection = true,
        enemyDetectionRange  = 8.0,
        enemyDisengageRange  = 10.0,
        enemyDisengageDelay  = 2.0,
        enemyComponents      = {"EnemyAI"},
        cacheUpdateInterval  = 1.0,
        debugEnemyDetection  = false,
    },

    -- ─────────────────────────────────────────────────────────────────────────
    Awake = function(self)
        self._yaw   = 180.0
        self._pitch = 15.0

        self._targetPos  = { x = 0.0, y = 0.0, z = 0.0 }
        self._hasTarget  = false
        self._firstMouse = true
        self._currentCollisionDist = nil

        -- Action mode
        self._actionModeActive        = false
        self._normalPitch             = 15.0
        self._normalDistance          = self.minZoom or 2.0
        self.followDistance           = self._normalDistance
        self._toggleCooldown          = 0.0
        self._actionModeTimer         = 0.0

        -- Chain aim
        self._chainAiming         = false
        self._chainAimBlend       = 0.0
        self._chainAimYaw         = nil
        self._chainAimPitch       = nil
        self._chainAimInitialized = false

        -- Enemy detection
        self._enemyInRange             = false
        self._enemyDisengageTimer      = 0.0
        self._enemyTriggeredActionMode = false
        self._triggeringEnemyId        = nil

        -- Cinematic
        self._cinematicActive    = false
        self._cinematicTarget    = nil
        self._cinematicStartPosX = nil
        self._cinematicStartRotW = nil

        -- Respawn teleport flag
        self._teleportToPlayer = false

        -- Motion blur
        self._lastCamX = nil
        self._lastCamY = nil
        self._lastCamZ = nil

        -- Screen shake
        self._shakeTimer       = 0.0
        self._shakeDuration    = 0.0
        self._shakeIntensity   = 0.0
        self._shakeFrequency   = 0.0
        self._shakePitchOffset = 0.0
        self._shakeYawOffset   = 0.0

        -- Slam tilt
        SlamTilt.init(self)

        -- Lock-on
        LockOn.init(self)

        -- Configure C++ entity cache intervals
        if Engine and Engine.SetCacheUpdateInterval then
            -- Deduplicate: register both enemy-detection names and chain-aim-assist names
            local registered = {}
            local allNames = {}
            for _, n in ipairs(self.enemyComponents or {}) do
                if not registered[n] then registered[n] = true; allNames[#allNames+1] = n end
            end
            for _, n in ipairs(self.chainAimAssistComponents or {}) do
                if not registered[n] then registered[n] = true; allNames[#allNames+1] = n end
            end
            for _, scriptName in ipairs(allNames) do
                print("[CameraFollow] Setting cache interval for: " .. tostring(scriptName))
                Engine.SetCacheUpdateInterval(scriptName, self.cacheUpdateInterval)
            end
        end

        -- Event subscriptions
        if event_bus and event_bus.subscribe then
            self._posSub = event_bus.subscribe("player_position", function(payload)
                if not payload then return end
                local x = payload.x or payload[1] or 0.0
                local y = payload.y or payload[2] or 0.0
                local z = payload.z or payload[3] or 0.0
                self._targetPos.x, self._targetPos.y, self._targetPos.z = x, y, z
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
                if self._chainAiming and not wasAiming then
                    self._chainAimInitialized = false
                    self._preChainAimPitch = self._normalPitch
                    -- Tell the player to face the current camera look direction.
                    -- self._yaw accumulates without wrapping, so collapse through
                    -- sin/cos + atan2 to get a canonical angle in (-180, 180].
                    if event_bus and event_bus.publish then
                        local yr = math.rad(self._yaw)
                        local lookYaw = math.deg(math.atan2(-math.sin(yr), -math.cos(yr)))
                        event_bus.publish("chain_aim_started", { yaw = lookYaw })
                    end
                elseif not self._chainAiming and wasAiming then
                    -- Chain aim released: inherit the current aim yaw/pitch into the
                    -- orbit camera so it zooms back out from the same angle instead
                    -- of panning back to the old orbit yaw.
                    if self._chainAimYaw then
                        -- _chainAimYaw is a look-direction; orbit _yaw is a camera-
                        -- position offset (180° apart), so we add 180 to convert.
                        self._yaw = self._chainAimYaw + 180.0
                    end
                    if self._chainAimPitch then
                        self._pitch = self._chainAimPitch
                    end
                    -- Restore the pre-chain-aim normal pitch so the camera
                    -- doesn't drop to 0 after chain aim ends.
                    self._normalPitch = self._preChainAimPitch or self._normalPitch
                end
            end)

            self._cinematicActiveSub = event_bus.subscribe("cinematic.active", function(active)
                self._cinematicActive = active
                if active then
                    print("[CameraFollow] Cinematic mode ACTIVE")
                else
                    print("[CameraFollow] Cinematic mode INACTIVE")
                    self._cinematicTarget    = nil
                    self._cinematicStartPosX = nil
                    self._cinematicStartRotW = nil
                end
            end)

            self._cinematicTargetSub = event_bus.subscribe("cinematic.target", function(payload)
                if payload then self._cinematicTarget = payload end
            end)

            self._playerRespawnedSub = event_bus.subscribe("playerRespawned", function(payload)
                if not payload then return end
                self._targetPos.x = payload.x or payload[1] or 0.0
                self._targetPos.y = payload.y or payload[2] or 0.0
                self._targetPos.z = payload.z or payload[3] or 0.0
                self._teleportToPlayer = true
            end)

            self._cameraShakeSub = event_bus.subscribe("camera_shake", function(p)
                if not p then return end
                self._shakeTimer     = 0.0
                self._shakeDuration  = p.duration  or self.ShakeDuration  or 0.4
                self._shakeIntensity = p.intensity or self.ShakeIntensity or 0.3
                self._shakeFrequency = p.frequency or self.ShakeFrequency or 25.0
            end)
        end
    end,

    -- ─────────────────────────────────────────────────────────────────────────
    OnDisable = function(self)
        SlamTilt.cleanup(self)
        LockOn.cleanup(self)
        if event_bus and event_bus.unsubscribe then
            for _, sub in ipairs({
                "_posSub", "_chainAimSub",
                "_cinematicActiveSub", "_cinematicTargetSub",
                "_playerRespawnedSub", "_cameraShakeSub",
            }) do
                if self[sub] then
                    event_bus.unsubscribe(self[sub])
                    self[sub] = nil
                end
            end
        end
        self._cinematicActive = false
        self._cinematicTarget = nil
        if Screen and Screen.SetCursorLocked then
            Screen.SetCursorLocked(false)
        end
    end,

    -- ─────────────────────────────────────────────────────────────────────────
    Update = function(self, dt)
        if not (self.GetPosition and self.SetPosition and self.SetRotation) then return end
        if not self._hasTarget then return end

        -- Teleport to respawn point without lerp
        if self._teleportToPlayer then
            print(string.format("[CameraFollow] Teleporting to %.2f %.2f %.2f",
                self._targetPos.x, self._targetPos.y, self._targetPos.z))
            self:SetPosition(self._targetPos.x, self._targetPos.y, self._targetPos.z)
            self._teleportToPlayer = false
            return
        end

        -- Tick C++ entity cache
        if Engine and Engine.UpdateCacheTiming then
            Engine.UpdateCacheTiming(dt)
        end

        -- ── Enemy detection / action mode ───────────────────────────────────
        EnemyDet.updateEnemyProximity(self, dt)

        if self._toggleCooldown > 0 then
            self._toggleCooldown = self._toggleCooldown - dt
        end

        -- Re-lock cursor when attacking (keeps it locked if the player clicks out)
        if Input then
            if (Input.IsActionJustPressed and Input.IsActionJustPressed("Attack"))
            or (Input.IsActionPressed    and Input.IsActionPressed("Attack")) then
                local isPaused = Time and Time.IsPaused and Time.IsPaused()
                if not isPaused
                and Screen and Screen.SetCursorLocked and Screen.IsCursorLocked
                and not Screen.IsCursorLocked() then
                    Screen.SetCursorLocked(true)
                    self._firstMouse = true
                end
            end
        end

        -- Action mode timer countdown (for attack-driven mode)
        if self._actionModeTimer > 0 then
            self._actionModeTimer = self._actionModeTimer - dt
            if self._actionModeTimer <= 0 then
                self._actionModeTimer  = 0
                self._actionModeActive = false
            end
        end

        -- ── Input (mouse look + scroll zoom) ────────────────────────────────
        if not self._cinematicActive then
            local isAndroid = Platform and Platform.IsAndroid and Platform.IsAndroid()
            if not self._loggedPlatform then
                print("[CameraFollow] isAndroid=" .. tostring(isAndroid))
                self._loggedPlatform = true
            end

            local cursorOk = isAndroid
                or (Screen and Screen.IsCursorLocked and Screen.IsCursorLocked())
            local shouldLock = self.lockCameraRotation
                or (self._actionModeActive and self.actionModeLockRotation)

            if cursorOk and not shouldLock then
                local lockedOn = LockOn.update(self, dt)
                if not lockedOn then
                    CamInput.updateMouseLook(self, dt)
                end
            end
        end
        -- Always called so the scroll buffer is drained every frame;
        -- updateScrollZoom discards the delta when cinematic or chain aim is active.
        CamInput.updateScrollZoom(self)

        -- ── Action mode pitch / distance transition ──────────────────────────
        if not self._cinematicActive then
            local targetPitch    = self._actionModeActive and self.actionModePitch    or self._normalPitch
            local targetDistance = self._actionModeActive and self.actionModeDistance or self._normalDistance
            local pitchT = 1.0 - math.exp(-(self.actionModeTransition or 8.0) * dt)
            local zoomT  = 1.0 - math.exp(-(self.zoomLerpSpeed or 6.0) * dt)
            self._pitch         = self._pitch         + (targetPitch    - self._pitch)         * pitchT
            self.followDistance = self.followDistance + (targetDistance - self.followDistance) * zoomT
        end

        -- ── Cinematic override ───────────────────────────────────────────────
        if Cinematic.updateCinematic(self, dt) then return end

        -- ── Chain aim blend ──────────────────────────────────────────────────
        local chainActive, chainX, chainY, chainZ = ChainAim.updateChainAim(self, dt)

        -- ── Orbit follow position ────────────────────────────────────────────
        local radius     = self.followDistance or 5.0
        if self.TriggerShake then
            self.TriggerShake    = false
            self._shakeTimer     = 0.0
            self._shakeDuration  = self.ShakeDuration  or 0.4
            self._shakeIntensity = self.ShakeIntensity or 0.3
            self._shakeFrequency = self.ShakeFrequency or 25.0
        end

        local slamAbsPitch = SlamTilt.update(self, dt)
        local pitchRad
        if slamAbsPitch then
            -- Keep _pitch and _normalPitch in sync so action-mode lerp
            -- doesn't fight the slam and the camera settles correctly on release.
            self._pitch       = slamAbsPitch
            self._normalPitch = slamAbsPitch
            pitchRad = math.rad(slamAbsPitch)
        else
            pitchRad = math.rad(self._pitch)
        end
        local yawRad     = math.rad(self._yaw)

        -- Export camera angles globally for skills to read
        _G.CAMERA_YAW = self._yaw
        _G.CAMERA_PITCH = self._pitch

        local minZoom    = self.minZoom or 2.0
        local maxZoom    = self.maxZoom or 15.0
        local zoomFactor = clamp((radius - minZoom) / (maxZoom - minZoom), 0.0, 1.0)

        -- Look-at pivot (slightly above player feet, scales with zoom).
        -- heightOffset is applied here so the pivot rises with the camera,
        -- keeping the pitch angle stable regardless of how high the offset is.
        local lookAtHeight = 0.5 + zoomFactor * 0.2
        local cameraTarget = {
            x = self._targetPos.x,
            y = self._targetPos.y + lookAtHeight + (self.heightOffset or 1.0),
            z = self._targetPos.z,
        }

        -- Publish horizontal forward for player movement
        if event_bus and event_bus.publish then
            event_bus.publish("camera_basis", {
                forward = { x = math.sin(yawRad), y = 0.0, z = math.cos(yawRad) },
            })
        end

        -- Ideal camera position (spherical offset from pivot)
        local horizRadius     = radius * math.cos(pitchRad)
        local desiredX = cameraTarget.x + horizRadius * math.sin(yawRad)
        local desiredY = cameraTarget.y + radius * math.sin(pitchRad)
        local desiredZ = cameraTarget.z + horizRadius * math.cos(yawRad)

        -- Hard Y cap: prevents the camera from rising above indoor ceilings even
        -- when the ceiling geometry has no physics collider. Set
        -- maxCameraHeightAbovePlayer = 0 in the editor to disable for open areas.
        local _maxH = self.maxCameraHeightAbovePlayer or 0
        if _maxH > 0 then
            desiredY = math.min(desiredY, self._targetPos.y + _maxH)
        end

        -- ── Wall collision ───────────────────────────────────────────────────
        desiredX, desiredY, desiredZ = Collision.applyCollision(
            self, cameraTarget.x, cameraTarget.y, cameraTarget.z,
            desiredX, desiredY, desiredZ, dt
        )

        -- ── Blend position with chain aim when active ─────────────────────
        -- Lerp between orbit and chain-aim positions based on blend factor
        -- so zoom-out and rotation change happen simultaneously on release.
        local blend = self._chainAimBlend
        if chainActive and chainX then
            desiredX = desiredX + (chainX - desiredX) * blend
            desiredY = desiredY + (chainY - desiredY) * blend
            desiredZ = desiredZ + (chainZ - desiredZ) * blend
        end

        -- ── Smooth position follow ───────────────────────────────────────────
        local cx, cy, cz
        local px, py, pz = self:GetPosition()
        if type(px) == "table" then
            cx, cy, cz = px.x or 0.0, px.y or 0.0, px.z or 0.0
        else
            cx, cy, cz = px or 0.0, py or 0.0, pz or 0.0
        end

        local lerpT = 1.0 - math.exp(-(self.followLerp or 10.0) * dt)
        local newX  = cx + (desiredX - cx) * lerpT
        local newY  = cy + (desiredY - cy) * lerpT
        local newZ  = cz + (desiredZ - cz) * lerpT

        -- Export exact camera position and mathematically perfect forward vector!
        _G.CAMERA_POS_X = newX
        _G.CAMERA_POS_Y = newY
        _G.CAMERA_POS_Z = newZ
        
        local fwdX = cameraTarget.x - newX
        local fwdY = cameraTarget.y - newY
        local fwdZ = cameraTarget.z - newZ
        local fLen = math.sqrt(fwdX*fwdX + fwdY*fwdY + fwdZ*fwdZ)
        
        if fLen > 0.001 then
            _G.CAMERA_FWD_X = fwdX / fLen
            _G.CAMERA_FWD_Y = fwdY / fLen
            _G.CAMERA_FWD_Z = fwdZ / fLen
        end

        self:SetPosition(newX, newY, newZ)

        -- ── Motion blur ──────────────────────────────────────────────────────
        -- Measure how far the camera actually moved this frame; publish normalised
        -- intensity so camera_effects can drive blur without knowing positions.
        if self.MotionBlurEnabled and self._lastCamX then
            local dx    = newX - self._lastCamX
            local dy    = newY - self._lastCamY
            local dz    = newZ - self._lastCamZ
            local speed = math.sqrt(dx*dx + dy*dy + dz*dz) / (dt > 0 and dt or 0.016)
            local lo    = self.MotionBlurThreshold or 2.0
            local hi    = self.MotionBlurMaxSpeed  or 14.0
            local intensity = math.max(0.0, math.min(1.0, (speed - lo) / (hi - lo)))
            if event_bus and event_bus.publish then
                event_bus.publish("fx_motion_blur", { intensity = intensity })
            end
        end
        self._lastCamX = newX
        self._lastCamY = newY
        self._lastCamZ = newZ

        -- ── Screen shake (angular, applied to rotation) ──────────────────────
        if self._shakeTimer < self._shakeDuration then
            self._shakeTimer = self._shakeTimer + dt
            local decay     = 1.0 - (self._shakeTimer / self._shakeDuration)
            local intensity = self._shakeIntensity * decay
            local t         = self._shakeTimer * self._shakeFrequency
            self._shakePitchOffset = math.sin(t * 1.7) * intensity
            self._shakeYawOffset   = math.sin(t * 1.3) * intensity
        else
            self._shakePitchOffset = 0.0
            self._shakeYawOffset   = 0.0
        end

        -- ── Rotation ─────────────────────────────────────────────────────────
        local utils_eu = require("Camera.camera_utils")
        local shakeQ = utils_eu.eulerToQuat(
            self._shakePitchOffset,
            self._shakeYawOffset,
            0.0
        )
        ChainAim.applyRotation(self, newX, newY, newZ, cameraTarget, chainActive, blend)
        -- Multiply shake onto the rotation after base rotation is set
        local rw, rx, ry, rz = self:GetRotation()
        if type(rw) == "table" then rw, rx, ry, rz = rw.w, rw.x, rw.y, rw.z end
        local sw, sx, sy, sz = shakeQ.w, shakeQ.x, shakeQ.y, shakeQ.z
        self:SetRotation(
            rw*sw - rx*sx - ry*sy - rz*sz,
            rw*sx + rx*sw + ry*sz - rz*sy,
            rw*sy - rx*sz + ry*sw + rz*sx,
            rw*sz + rx*sy - ry*sx + rz*sw
        )

        self.isDirty = true
    end,
}