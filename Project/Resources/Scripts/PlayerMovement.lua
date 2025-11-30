-- PlayerMovement.lua
-- Player movement using CharacterController for proper wall collision
-- Includes walking/jump animation, camera-relative rotation, and SFX

print("[PlayerMovement] SCRIPT LOADING...")

require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus
local Input = _G.Input
local CharacterController = _G.CharacterController

--------------------------------------------------------------------------------
-- Helper Functions
--------------------------------------------------------------------------------

-- Direction to quaternion (Y-axis rotation) - for facing movement direction
local function directionToQuaternion(dx, dz)
    local angle
    if dx == 0 and dz == 0 then
        angle = 0
    elseif dz > 0 then
        angle = math.atan(dx / dz)
    elseif dz < 0 then
        angle = math.atan(dx / dz) + (dx >= 0 and math.pi or -math.pi)
    else
        angle = dx > 0 and (math.pi * 0.5) or (-math.pi * 0.5)
    end

    local halfAngle = angle * 0.5
    return math.cos(halfAngle), 0, math.sin(halfAngle), 0
end

-- Quaternion lerp with normalization (for smooth rotation)
local function lerpQuaternion(w1, x1, y1, z1, w2, x2, y2, z2, t)
    -- Ensure shortest path
    if w1*w2 + x1*x2 + y1*y2 + z1*z2 < 0 then
        w2, x2, y2, z2 = -w2, -x2, -y2, -z2
    end

    -- Lerp components
    local w = w1 + (w2 - w1) * t
    local x = x1 + (x2 - x1) * t
    local y = y1 + (y2 - y1) * t
    local z = z1 + (z2 - z1) * t

    -- Normalize
    local invLen = 1.0 / math.sqrt(w*w + x*x + y*y + z*z + 0.0001)
    return w * invLen, x * invLen, y * invLen, z * invLen
end

-- Play random SFX from array
local function playRandomSFX(audio, clips)
    local count = clips and #clips or 0
    if count > 0 and audio then
        audio:PlayOneShot(clips[math.random(1, count)])
    end
end

--------------------------------------------------------------------------------
-- Component Definition
--------------------------------------------------------------------------------

return Component {
    mixins = { TransformMixin },

    ----------------------------------------------------------------------
    -- Inspector Fields
    ----------------------------------------------------------------------
    fields = {
        name = "PlayerMovement",

        -- Movement settings
        moveSpeed     = 5.0,
        jumpHeight    = 1.5,
        rotationSpeed = 10.0,

        -- SFX clip arrays (populate in editor with audio GUIDs)
        footstepSFXClips = {},
        jumpSFXClips     = {},
        landingSFXClips  = {},

        -- SFX timing
        footstepInterval = 0.4,
        sfxVolume        = 0.5,
    },

    ----------------------------------------------------------------------
    -- Lifecycle: Awake
    ----------------------------------------------------------------------
    Awake = function(self)
        print("[PlayerMovement] Awake - Using CharacterController")

        -- Character controller instance
        self._charController = nil
        self._controllerReady = false

        -- Movement state
        self._isGrounded = true
        self._wasGrounded = true

        -- Animation state
        self._isWalking  = false
        self._isJumping  = false

        -- Animation clip indices
        self._idleAnimationClip = 0
        self._walkAnimationClip = 1
        self._jumpAnimationClip = 2

        -- Rotation state (quaternion: w, x, y, z)
        self._currentRotW = 1
        self._currentRotX = 0
        self._currentRotY = 0
        self._currentRotZ = 0

        -- Timers
        self._footstepTimer = 0

        -- Input tracking for key press detection
        self._prevInputW = false
        self._prevInputS = false
        self._prevInputA = false
        self._prevInputD = false

        -- Camera yaw for camera-relative movement
        self._cameraYaw = 180.0
        self._cameraYawSub = nil

        -- Subscribe to camera yaw updates
        if event_bus and event_bus.subscribe then
            self._cameraYawSub = event_bus.subscribe("camera_yaw", function(yaw)
                if yaw then
                    self._cameraYaw = yaw
                end
            end)
        end
    end,

    ----------------------------------------------------------------------
    -- Lifecycle: Start
    ----------------------------------------------------------------------
    Start = function(self)
        print("[PlayerMovement] ===== START CALLED =====")

        -- Cache component references
        self._animator = self:GetComponent("AnimationComponent")
        self._audio    = self:GetComponent("AudioComponent")
        self._collider = self:GetComponent("ColliderComponent")
        self._transform = self:GetComponent("Transform")

        -- Debug: Print what we got
        print("[PlayerMovement] animator:", self._animator)
        print("[PlayerMovement] collider:", self._collider)
        print("[PlayerMovement] transform:", self._transform)

        if self._animator then
            self._animator.enabled = true
            self._animator:PlayClip(self._idleAnimationClip or 0, true)
        end

        if self._audio then
            self._audio.enabled = true
            self._audio:SetVolume(self.sfxVolume or 0.5)
        end

        -- Debug: Check if ColliderComponent exists
        if not self._collider then
            print("[PlayerMovement] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
            print("[PlayerMovement] WARNING: No ColliderComponent found on Player!")
            print("[PlayerMovement] Add a ColliderComponent (Capsule type) to the Player entity and save the scene.")
            print("[PlayerMovement] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
        else
            print("[PlayerMovement] ColliderComponent FOUND!")
        end

        -- Create and initialize CharacterController
        if CharacterController and CharacterController.new then
            self._charController = CharacterController.new()

            if self._charController and self._collider and self._transform then
                -- Use capsule dimensions configured in the editor
                local success = CharacterController.Initialise(
                    self._charController,
                    self._collider,
                    self._transform
                )

                if success then
                    print("[PlayerMovement] CharacterController initialized successfully!")
                    self._controllerReady = true
                else
                    print("[PlayerMovement] ERROR: CharacterController.Initialise failed!")
                end
            else
                print("[PlayerMovement] ERROR: Missing components for CharacterController")
                print("  charController:", self._charController)
                print("  collider:", self._collider)
                print("  transform:", self._transform)
            end
        else
            print("[PlayerMovement] ERROR: CharacterController not available in Lua!")
        end

        -- Initial position broadcast for camera
        if event_bus and event_bus.publish then
            local pos = self:_getPosition()
            event_bus.publish("player_position", { x = pos.x, y = pos.y, z = pos.z })
        end
    end,

    ----------------------------------------------------------------------
    -- Helper: Get position from CharacterController or Transform
    ----------------------------------------------------------------------
    _getPosition = function(self)
        if self._controllerReady and self._charController then
            local pos = CharacterController.GetPosition(self._charController)
            return { x = pos.x or 0, y = pos.y or 0, z = pos.z or 0 }
        else
            -- Fallback to transform
            local ok, a, b, c = pcall(self.GetPosition, self)
            if ok then
                if type(a) == "table" then
                    return { x = a.x or 0, y = a.y or 0, z = a.z or 0 }
                else
                    return { x = a or 0, y = b or 0, z = c or 0 }
                end
            end
            return { x = 0, y = 0, z = 0 }
        end
    end,

    ----------------------------------------------------------------------
    -- Helper: Sync transform from CharacterController position
    ----------------------------------------------------------------------
    _syncTransformFromController = function(self)
        if self._controllerReady and self._charController then
            local pos = CharacterController.GetPosition(self._charController)
            pcall(self.SetPosition, self, pos.x, pos.y, pos.z)
        end
    end,

    ----------------------------------------------------------------------
    -- Lifecycle: Update
    ----------------------------------------------------------------------
    Update = function(self, dt)
        local animator = self._animator
        local audio    = self._audio

        -- Read global attack state
        local isAttacking = (_G.player_is_attacking == true)

        --------------------------------------
        -- 1) Read WASD Input
        --------------------------------------
        local inputW = Input and Input.GetKey and Input.GetKey(Input.Key.W) or false
        local inputS = Input and Input.GetKey and Input.GetKey(Input.Key.S) or false
        local inputA = Input and Input.GetKey and Input.GetKey(Input.Key.A) or false
        local inputD = Input and Input.GetKey and Input.GetKey(Input.Key.D) or false

        -- Raw input direction
        local rawMoveX, rawMoveZ = 0.0, 0.0
        if inputW then rawMoveZ = rawMoveZ + 1.0 end
        if inputS then rawMoveZ = rawMoveZ - 1.0 end
        if inputA then rawMoveX = rawMoveX + 1.0 end
        if inputD then rawMoveX = rawMoveX - 1.0 end

        -- Normalize diagonal movement
        local len = math.sqrt(rawMoveX * rawMoveX + rawMoveZ * rawMoveZ)
        if len > 0.0001 then
            rawMoveX = rawMoveX / len
            rawMoveZ = rawMoveZ / len
        end

        local hasMovementInput = (len > 0.0001)

        -- Transform to camera-relative direction
        local moveX, moveZ = 0.0, 0.0
        if hasMovementInput then
            local yawRad = math.rad(self._cameraYaw or 180.0)
            local sinYaw = math.sin(yawRad)
            local cosYaw = math.cos(yawRad)
            moveX = rawMoveZ * (-sinYaw) - rawMoveX * cosYaw
            moveZ = rawMoveZ * (-cosYaw) + rawMoveX * sinYaw
        end

        --------------------------------------
        -- 2) Detect Key Press for Footstep SFX
        --------------------------------------
        local keyJustPressed = (inputW and not self._prevInputW) or
                            (inputS and not self._prevInputS) or
                            (inputA and not self._prevInputA) or
                            (inputD and not self._prevInputD)

        self._prevInputW = inputW
        self._prevInputS = inputS
        self._prevInputA = inputA
        self._prevInputD = inputD

        --------------------------------------
        -- 3) CharacterController Movement
        --------------------------------------
        if self._controllerReady and self._charController then
            -- Check grounded state
            self._wasGrounded = self._isGrounded
            self._isGrounded = CharacterController.IsGrounded(self._charController)

            -- Debug: Print grounded state occasionally
            self._groundedDebugTimer = (self._groundedDebugTimer or 0) + dt
            if self._groundedDebugTimer > 2.0 then
                print("[PlayerMovement] isGrounded=" .. tostring(self._isGrounded) .. " hasInput=" .. tostring(hasMovementInput))
                self._groundedDebugTimer = 0
            end

            -- Get current velocity for Y preservation
            local currentVel = CharacterController.GetVelocity(self._charController)
            local velY = currentVel.y or 0

            -- Calculate horizontal velocity
            local speed = self.moveSpeed or 5.0
            local velX = moveX * speed
            local velZ = moveZ * speed

            -- Jump input - only when grounded and not attacking
            local jumpPressed = Input and Input.GetKeyDown and Input.GetKeyDown(Input.Key.Space)
            if self._isGrounded and jumpPressed and not isAttacking then
                CharacterController.Jump(self._charController, self.jumpHeight or 1.5)
                self._isJumping = true

                if animator then
                    animator:PlayClip(self._jumpAnimationClip, false)
                end
                playRandomSFX(audio, self.jumpSFXClips)
                print("[PlayerMovement] Jump!")
            end

            -- Set movement velocity (CharacterController handles gravity)
            CharacterController.Move(self._charController, velX, velY, velZ)

            -- Update CharacterController physics
            CharacterController.Update(self._charController, dt)

            -- Sync transform from CharacterController
            self:_syncTransformFromController()

            -- Landing detection
            if self._isGrounded and not self._wasGrounded then
                if self._isJumping then
                    playRandomSFX(audio, self.landingSFXClips)
                    self._isJumping = false
                    print("[PlayerMovement] Landed!")

                    if animator and not isAttacking then
                        if hasMovementInput then
                            animator:PlayClip(self._walkAnimationClip or 1, true)
                        else
                            animator:PlayClip(self._idleAnimationClip or 0, true)
                        end
                    end
                end
            end
        else
            -- Fallback: Direct transform movement (no collision)
            -- Assume grounded in fallback mode so animations work
            self._isGrounded = true

            local pos = self:_getPosition()
            local speed = self.moveSpeed or 5.0
            local newX = pos.x + moveX * speed * dt
            local newZ = pos.z + moveZ * speed * dt
            pcall(self.SetPosition, self, newX, pos.y, newZ)
        end

        --------------------------------------
        -- 4) Rotation: Face Movement Direction
        --------------------------------------
        if hasMovementInput and self.SetRotation and not isAttacking then
            local targetW, targetX, targetY, targetZ = directionToQuaternion(moveX, moveZ)

            local t = math.min((self.rotationSpeed or 10.0) * dt, 1.0)
            local newW, newX2, newY2, newZ2 = lerpQuaternion(
                self._currentRotW, self._currentRotX, self._currentRotY, self._currentRotZ,
                targetW, targetX, targetY, targetZ,
                t
            )

            self._currentRotW = newW
            self._currentRotX = newX2
            self._currentRotY = newY2
            self._currentRotZ = newZ2

            pcall(self.SetRotation, self, newW, newX2, newY2, newZ2)
        end

        --------------------------------------
        -- 5) Walking Animation & Footstep SFX
        --------------------------------------
        local isWalkingNow = hasMovementInput and self._isGrounded and not isAttacking

        if isWalkingNow ~= self._isWalking and not self._isJumping then
            self._isWalking = isWalkingNow

            if animator and not isAttacking then
                if isWalkingNow then
                    animator:PlayClip(self._walkAnimationClip or 1, true)
                else
                    animator:PlayClip(self._idleAnimationClip or 0, true)
                end
            end
        end

        -- Footstep SFX
        if isWalkingNow then
            if keyJustPressed then
                playRandomSFX(audio, self.footstepSFXClips)
                self._footstepTimer = 0
            end

            self._footstepTimer = self._footstepTimer + dt
            if self._footstepTimer >= (self.footstepInterval or 0.4) then
                playRandomSFX(audio, self.footstepSFXClips)
                self._footstepTimer = 0
            end
        else
            self._footstepTimer = 0
        end

        --------------------------------------
        -- 6) Broadcast Position for Camera
        --------------------------------------
        if event_bus and event_bus.publish then
            local pos = self:_getPosition()
            event_bus.publish("player_position", { x = pos.x, y = pos.y, z = pos.z })
        end
    end,

    ----------------------------------------------------------------------
    -- Lifecycle: OnDisable
    ----------------------------------------------------------------------
    OnDisable = function(self)
        print("[PlayerMovement] OnDisable")

        -- Destroy CharacterController
        if self._charController and CharacterController and CharacterController.Destroy then
            CharacterController.Destroy(self._charController)
            self._charController = nil
            self._controllerReady = false
        end

        -- Unsubscribe from camera yaw updates
        if event_bus and event_bus.unsubscribe and self._cameraYawSub then
            event_bus.unsubscribe(self._cameraYawSub)
            self._cameraYawSub = nil
        end
    end,
}
