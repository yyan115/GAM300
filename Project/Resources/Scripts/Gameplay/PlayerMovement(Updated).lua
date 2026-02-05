require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus
local Input = _G.Input

-- Animation States
local IDLE = 0
local RUN  = 1
local JUMP = 2

-- Helper: convert 2D movement vector to Y-axis quaternion
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

-- Helper: play random SFX from array
local function playRandomSFX(audio, clips)
    local count = clips and #clips or 0
    if count > 0 and audio then
        audio:PlayOneShot(clips[math.random(1, count)])
    end
end

-- Helper: lerp quaternion for smooth rotation
local function lerpQuaternion(w1, x1, y1, z1, w2, x2, y2, z2, t)
    if w1*w2 + x1*x2 + y1*y2 + z1*z2 < 0 then
        w2, x2, y2, z2 = -w2, -x2, -y2, -z2
    end
    local w = w1 + (w2 - w1) * t
    local x = x1 + (x2 - x1) * t
    local y = y1 + (y2 - y1) * t
    local z = z1 + (z2 - z1) * t
    local invLen = 1.0 / math.sqrt(w*w + x*x + y*y + z*z + 0.0001)
    return w * invLen, x * invLen, y * invLen, z * invLen
end

return Component {
    mixins = { TransformMixin },

    fields = {
        Speed = 1.5,
        JumpHeight = 1.2,
        DamageStunDuration = 1.0,
        LandingDuration = 0.5,
        footstepInterval = 0.35,  -- Time between footstep sounds while running
        playerFootstepSFX = {},
        playerHurtSFX = {},
        playerJumpSFX = {},
        playerLandSFX = {},
        playerDeadSFX = {},
    },

    Awake = function(self)
        self._currentRotW = 1
        self._currentRotX = 0
        self._currentRotY = 0
        self._currentRotZ = 0

        -- Camera state
        self._cameraYaw = 180.0
        self._cameraYawSub = nil

        if event_bus and event_bus.subscribe then
            print("[PlayerMovement] Subscribing to camera_yaw")
            self._cameraYawSub = event_bus.subscribe("camera_yaw", function(yaw)
                if yaw then
                    self._cameraYaw = yaw
                end
            end)

            print("[PlayerMovement] Subscribing to playerDead")
            self._playerDeadSub = event_bus.subscribe("playerDead", function(playerDead)
                if playerDead then
                    print("[PlayerMovement] Received playerDead")
                    self._playerDeadPending = playerDead
                    -- Play death SFX
                    playRandomSFX(self._audio, self.playerDeadSFX)
                end
            end)

            print("[PlayerMovement] Subscribing to playerHurtTriggered")
            self._playerHurtTriggeredSub = event_bus.subscribe("playerHurtTriggered", function(hit)
                if hit then
                    print("[PlayerMovement] playerHurtTriggered received")
                    self._isDamageStun = true
                    if self._animator then self._animator:SetBool("IsJumping", false) end
                    -- Play hurt SFX
                    playRandomSFX(self._audio, self.playerHurtSFX)
                end
            end)

            print("[PlayerMovement] Subscribing to player_knockback")
            self._kbPending = false
            self._kbX, self._kbZ = 0, 0

            self._knockSub = event_bus.subscribe("player_knockback", function(p)
                if not p then return end
                self._kbX = (p.x or 0) * (p.strength or 0)
                self._kbZ = (p.z or 0) * (p.strength or 0)
                self._kbPending = true
            end)
            print("[PlayerMovement] Subscribing to respawnPlayer")
            self._respawnPlayerSub = event_bus.subscribe("respawnPlayer", function(respawn)
                if respawn then
                    self._respawnPlayer = true
                end
            end)

            print("[PlayerMovement] Subscribing to activatedCheckpoint")
            self._activatedCheckpointSub = event_bus.subscribe("activatedCheckpoint", function(entityId)
                if entityId then
                    self._activatedCheckpoint = entityId
                end
            end)
            print("[PlayerMovement] Subscription token: " .. tostring(self._activatedCheckpointSub))
        else
            print("[PlayerMovement] ERROR: event_bus not available!")
        end
    end,

    Start = function(self)
        self._collider  = self:GetComponent("ColliderComponent")
        self._animator  = self:GetComponent("AnimationComponent")
        self._transform = self:GetComponent("Transform")
        self._audio     = self:GetComponent("AudioComponent")

        print("transform y here is ", self._transform.localPosition.y)
        self._controller = CharacterController.Create(self.entityId, self._collider, self._transform)

        if self._animator then
            print("[PlayerMovement] Animator found, playing IDLE clip")
            self._animator:PlayClip(IDLE, true)
        else
            print("[PlayerMovement] ERROR: Animator is nil!")
        end

        self._isRunning = false
        self._isJumping = false
        self.rotationSpeed = 10.0

        self._damageStunDuration = self.DamageStunDuration
        self._isDamageStun = false

        self._landingDuration = self.LandingDuration

        -- SFX state
        self._footstepTimer = 0
        self._wasRunning = false

        self._initialSpawnPoint = self._transform.worldPosition
    end,

    RespawnPlayer = function(self)
        if not self._controller then
            --print("[PlayerMovement] RespawnPlayer: self._controller is null")
        end

        if not self._transform then
            --print("[PlayerMovement] RespawnPlayer: self._transform is null")
        end
        
        if not self._activatedCheckpoint then
            --print("[PlayerMovement] RespawnPlayer: self._activatedCheckpoint is null")
        end

        if self._activatedCheckpoint then
            local checkpointTransform = GetComponent(self._activatedCheckpoint, "Transform")
            local checkpointPos = checkpointTransform.worldPosition
            self:SetPosition(checkpointPos.x, checkpointPos.y, checkpointPos.z)
        elseif self._initialSpawnPoint then
            self:SetPosition(self._initialSpawnPoint.x, self._initialSpawnPoint.y, self._initialSpawnPoint.z)
        end

        CharacterController.SetPosition(self._controller, self._transform)
        self._respawnPlayer = false
        self._playerDead = false
        self._playerDeadPending = false
        print("self._animator:SetBool(IsDead, false)")
        self._animator:SetBool("IsDead", false)

        print(string.format("[PlayerMovement] Respawned player to %f %f %f", checkpointPos.x, checkpointPos.y, checkpointPos.z))
    end,

    Update = function(self, dt)
        if self._respawnPlayer then
            self.RespawnPlayer(self)
            return
        end

        if not self._collider or not self._transform or not self._controller or self._playerDead then
            return
        end

        -- KNOCKBACK (ONE-SHOT, applied immediately even during damage stun)
        if self._kbPending then
            self._kbPending = false
            CharacterController.Move(
                self._controller,
                (self._kbX or 0),
                0,
                (self._kbZ or 0)
            )
            -- prevent weird follow-through
            self._kbX, self._kbZ = 0, 0
        end

        if self._isDamageStun == true then
            self._damageStunDuration = self._damageStunDuration - dt
            if self._damageStunDuration <= 0 then
                self._damageStunDuration = self.DamageStunDuration
                self._isDamageStun = false
            end
        end

        if self._isLanding == true then
            self._landingDuration = self._landingDuration - dt
            if self._landingDuration <= 0 then
                self._landingDuration = self.LandingDuration
                self._isLanding = false
                self._isRolling = false
            end
        end

        if self._isDamageStun == true then
            local isGrounded = CharacterController.IsGrounded(self._controller)
            self._animator:SetBool("IsGrounded", isGrounded)
            
            self._animator:SetBool("IsRunning", self._isRunning)
            --self._animator:SetBool("IsJumping", self._isJumping)

            local position = CharacterController.GetPosition(self._controller)
            if position then
                self:SetPosition(position.x, position.y, position.z)
                if event_bus and event_bus.publish then
                    event_bus.publish("player_position", position)
                end
            end
            return
        end

        -- RAW INPUT (LOCAL SPACE)
        local axis = Input and Input.GetAxis and Input.GetAxis("Movement") or { x = 0, y = 0 }
        local rawX = -axis.x
        local rawZ = axis.y

        -- CAMERA-RELATIVE MOVEMENT
        local cameraYaw = _G.CAMERA_YAW or self._cameraYaw or 180.0
        local moveX, moveZ = 0, 0
        if rawX ~= 0 or rawZ ~= 0 then
            self._prevRawX = rawX
            self._prevRawZ = rawZ

            local yawRad = math.rad(cameraYaw)
            local sinYaw = math.sin(yawRad)
            local cosYaw = math.cos(yawRad)

            moveX = rawZ * (-sinYaw) - rawX * cosYaw
            moveZ = rawZ * (-cosYaw) + rawX * sinYaw
        end

        local isMoving = (moveX ~= 0 or moveZ ~= 0)

        -- JUMP
        local isGrounded = CharacterController.IsGrounded(self._controller)
        local isJumping = false
        self._animator:SetBool("IsGrounded", isGrounded)

        -- If death is pending AND we are on the floor, die now.
        if self._playerDeadPending and isGrounded then
            if self._animator then
                self._animator:SetBool("IsDead", true)
                print("[PlayerMovement] Player grounded, playing Death animation")
            end
            self._playerDead = true
            self._playerDeadPending = false
            return -- Exit immediately so we don't process movement on the death frame
        end

        if not self._isLanding and Input and Input.IsActionJustPressed and Input.IsActionJustPressed("Jump") and isGrounded then
            CharacterController.Jump(self._controller, self.JumpHeight)
            isJumping = true
            self._animator:SetBool("IsJumping", true)
            -- Play jump SFX
            playRandomSFX(self._audio, self.playerJumpSFX)
        end

        -- APPLY MOVEMENT
        if not isJumping and isMoving then
            CharacterController.Move(
                self._controller,
                moveX * self.Speed,
                0,
                moveZ * self.Speed
            )
        end

        -- FORCE MOVEMENT IF PLAYER IS ROLLING
        if self._isRolling and not isMoving then
            local yawRad = math.rad(cameraYaw)
            local sinYaw = math.sin(yawRad)
            local cosYaw = math.cos(yawRad)

            moveX = self._prevRawZ * (-sinYaw) - self._prevRawX * cosYaw
            moveZ = self._prevRawZ * (-cosYaw) + self._prevRawX * sinYaw

            CharacterController.Move(
                self._controller,
                moveX * self.Speed,
                0,
                moveZ * self.Speed
            )
        end

        -- ANIMATION LOGIC
        if not isGrounded then
            if not self._isJumping then
                self._isJumping = true
                self._isRunning = false
            end
        else
            if self._isJumping then
                self._isJumping = false
                self._animator:SetBool("IsJumping", false)
                self._isLanding = true
                -- Play landing SFX
                playRandomSFX(self._audio, self.playerLandSFX)
                -- Resume proper state based on movement
                if isMoving then
                    print("[PlayerMovement] SetBool(IsRunning, true)")
                    self._animator:SetBool("IsRunning", true)
                    self._isRunning = true
                    self._isRolling = true
                else
                    print("[PlayerMovement] SetBool(IsRunning, false)")
                    self._animator:SetBool("IsRunning", false)
                    self._isRunning = false
                end
            elseif isMoving and not self._isRunning then
                self._animator:SetBool("IsRunning", true)
                self._isRunning = true
            elseif not isMoving and self._isRunning then
                self._animator:SetBool("IsRunning", false)
                self._isRunning = false
            end
        end

        -- Footstep SFX while running (timer-based)
        if self._isRunning and isGrounded and not self._isLanding then
            -- Play immediately when running starts
            if not self._wasRunning then
                playRandomSFX(self._audio, self.playerFootstepSFX)
                self._footstepTimer = 0
            end
            -- Timer-based footsteps
            self._footstepTimer = self._footstepTimer + dt
            if self._footstepTimer >= (self.footstepInterval or 0.35) then
                playRandomSFX(self._audio, self.playerFootstepSFX)
                self._footstepTimer = 0
            end
        else
            self._footstepTimer = 0
        end
        self._wasRunning = self._isRunning

        -- ROTATION
        if isMoving and self.SetRotation then
            local targetW, targetX, targetY, targetZ = directionToQuaternion(moveX, moveZ)
            local t = math.min(self.rotationSpeed * dt, 1.0)
            local newW, newX, newY, newZ = lerpQuaternion(
                self._currentRotW, self._currentRotX,
                self._currentRotY, self._currentRotZ,
                targetW, targetX, targetY, targetZ,
                t
            )

            self._currentRotW, self._currentRotX, self._currentRotY, self._currentRotZ = newW, newX, newY, newZ
            pcall(self.SetRotation, self, newW, newX, newY, newZ)
        end

        -- POSITION SYNC
        local position = CharacterController.GetPosition(self._controller)
        if position then
            self:SetPosition(position.x, position.y, position.z)
            if event_bus and event_bus.publish then
                event_bus.publish("player_position", position)
            end
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe and self._cameraYawSub then
            event_bus.unsubscribe(self._cameraYawSub)
            self._cameraYawSub = nil
        end
    end,
}