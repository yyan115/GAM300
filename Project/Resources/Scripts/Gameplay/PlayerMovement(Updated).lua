require("extension.engine_bootstrap")
_G.CHAIN_DEBUG = _G.CHAIN_DEBUG ~= nil and _G.CHAIN_DEBUG or false
local function dbg(...) if _G.CHAIN_DEBUG then print(...) end end
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus
local Input = _G.Input

-- Animation States
local IDLE = 0
local RUN  = 1
local JUMP = 2

-- Chain Tension Response
local chainSpeedMult = 1.0
local tensionRadialX = 0
local tensionRadialZ = 0
local tensionScale   = 7.0

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
        footstepInterval = 0.35,
        DashSpeed = 5.0,
        DashDuration = 0.3,
        DashCooldown = 2.0,
        CinematicSettleTime = 0.8,
        AirDashSpeedMultiplier = 1.5,
        AirDashLift = 2.0,
        AttackLungeSpeed = 3.0,
        AttackLungeDuration = 0.12,
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

        -- Chain movement constraint state
        self._chainConstraintRatio = 0   -- 0=free, 1=at hard limit
        self._chainConstraintExceeded = false
        self._chainDrag = false          -- true when being dragged by a DragTag entity
        self._chainDragTargetX = 0
        self._chainDragTargetY = 0
        self._chainDragTargetZ = 0

        if event_bus and event_bus.subscribe then
            dbg("[PlayerMovement] Subscribing to camera_yaw")
            self._cameraYawSub = event_bus.subscribe("camera_yaw", function(yaw)
                if yaw then self._cameraYaw = yaw end
            end)

            dbg("[PlayerMovement] Subscribing to playerDead")
            self._playerDeadSub = event_bus.subscribe("playerDead", function(playerDead)
                if playerDead then
                    dbg("[PlayerMovement] Received playerDead")
                    self._playerDeadPending = playerDead
                    playRandomSFX(self._audio, self.playerDeadSFX)
                end
            end)

            dbg("[PlayerMovement] Subscribing to playerHurtTriggered")
            self._playerHurtTriggeredSub = event_bus.subscribe("playerHurtTriggered", function(hit)
                if hit then
                    self._isDamageStun = true
                    if self._animator then self._animator:SetBool("IsJumping", false) end
                    playRandomSFX(self._audio, self.playerHurtSFX)
                end
            end)

            dbg("[PlayerMovement] Subscribing to player_knockback")
            self._kbPending = false
            self._kbX, self._kbZ = 0, 0

            self._knockSub = event_bus.subscribe("player_knockback", function(p)
                if not p then return end
                self._kbX = (p.x or 0) * (p.strength or 0)
                self._kbZ = (p.z or 0) * (p.strength or 0)
                self._kbPending = true
            end)

            dbg("[PlayerMovement] Subscribing to respawnPlayer")
            self._respawnPlayerSub = event_bus.subscribe("respawnPlayer", function(respawn)
                if respawn then self._respawnPlayer = true end
            end)

            self._lungeTimer = 0
            self._lungeDirX  = 0
            self._lungeDirZ  = 0
            self._attackLungeSub = event_bus.subscribe("attack_performed", function(data)
                local w = self._currentRotW
                local y = self._currentRotY
                local fwdX = 2.0 * w * y
                local fwdZ = w * w - y * y
                local len = math.sqrt(fwdX * fwdX + fwdZ * fwdZ)
                if len > 0.001 then
                    self._lungeDirX = fwdX / len
                    self._lungeDirZ = fwdZ / len
                end
                self._lungeTimer = self.AttackLungeDuration or 0.12
            end)

            dbg("[PlayerMovement] Subscribing to activatedCheckpoint")
            self._activatedCheckpointSub = event_bus.subscribe("activatedCheckpoint", function(entityId)
                if entityId then self._activatedCheckpoint = entityId end
            end)

            dbg("[PlayerMovement] Subscribing to freeze_player")
            self._frozenBycinematic = false
            self._freezePending = false
            self._freezeSettleTimer = 0.0
            self._freezePlayerSub = event_bus.subscribe("freeze_player", function(frozen)
                if frozen then
                    self._freezePending = true
                    self._freezeSettleTimer = self.CinematicSettleTime or 0.8
                else
                    self._frozenBycinematic = false
                    self._freezePending = false
                end
                dbg("[PlayerMovement] Frozen = " .. tostring(frozen))
            end)

            dbg("[PlayerMovement] Subscribing to request_player_forward")
            self._requestPlayerForwardSub = event_bus.subscribe("request_player_forward", function(_)
                if not self._facingX or not self._facingZ then return end
                if event_bus and event_bus.publish then
                    event_bus.publish("player_forward_response", {
                        x = self._facingX,
                        y = 0,
                        z = self._facingZ
                    })
                end
            end)

            -- Chain movement constraint: reduce speed as player approaches slack limit,
            -- exceeded means the chain just flopped so constraint is cleared.
            self._chainConstraintSub = event_bus.subscribe("chain.movement_constraint", function(payload)
                if not payload then return end
                self._chainConstraintRatio    = payload.ratio or 0
                self._chainConstraintExceeded = payload.exceeded or false
                self._chainDrag               = payload.drag or false
                self._chainEndX = payload.endX
                self._chainEndY = payload.endY
                self._chainEndZ = payload.endZ
                if self._chainDrag then
                    self._chainDragTargetX = payload.targetX or 0
                    self._chainDragTargetY = payload.targetY or 0
                    self._chainDragTargetZ = payload.targetZ or 0
                end
            end)
        else
            dbg("[PlayerMovement] ERROR: event_bus not available!")
        end
    end,

    Start = function(self)
        self._collider  = self:GetComponent("ColliderComponent")
        self._animator  = self:GetComponent("AnimationComponent")
        self._transform = self:GetComponent("Transform")
        self._audio     = self:GetComponent("AudioComponent")
        self._rigidbody = self:GetComponent("RigidBodyComponent")

        dbg("transform y here is ", self._transform.localPosition.y)
        self._controller = CharacterController.Create(self.entityId, self._collider, self._transform)

        if self._animator then
            dbg("[PlayerMovement] Animator found, playing IDLE clip")
            self._animator:PlayClip(IDLE, true)
        else
            dbg("[PlayerMovement] ERROR: Animator is nil!")
        end

        self._isRunning = false
        self._isJumping = false
        self.rotationSpeed = 10.0

        self._damageStunDuration = self.DamageStunDuration
        self._isDamageStun = false

        self._landingDuration = self.LandingDuration

        self._isDashing = false
        self._dashTimer = 0
        self._dashCooldownTimer = 0
        self._dashDirX = 0
        self._dashDirZ = 0
        self._wasDashingInAir = false
        _G.player_is_dashing = false

        self._footstepTimer = 0
        self._wasRunning = false

        local pos = self._transform.worldPosition
        self._initialSpawnPoint = { x = pos.x, y = pos.y, z = pos.z }
    end,

    RespawnPlayer = function(self)
        local respawnPos = self._initialSpawnPoint
        if self._activatedCheckpoint then
            local checkpointTransform = GetComponent(self._activatedCheckpoint, "Transform")
            local checkpointPos = checkpointTransform.worldPosition
            self:SetPosition(checkpointPos.x, checkpointPos.y, checkpointPos.z)
            respawnPos = checkpointPos
        elseif self._initialSpawnPoint then
            self:SetPosition(self._initialSpawnPoint.x, self._initialSpawnPoint.y, self._initialSpawnPoint.z)
        end

        CharacterController.SetPosition(self._controller, self._transform)
        self._respawnPlayer = false
        self._playerDead = false
        self._playerDeadPending = false
        self._animator:SetBool("IsDead", false)
        self._justRespawnedPlayer = true
        -- Clear constraint on respawn
        self._chainConstraintRatio = 0
        self._chainConstraintExceeded = false

        if event_bus and event_bus.publish then
            event_bus.publish("playerRespawned", respawnPos)
            dbg(string.format("[PlayerMovement] Respawned player to %f %f %f", respawnPos.x, respawnPos.y, respawnPos.z))
        end
    end,

    Update = function(self, dt)
        if self._respawnPlayer then
            self:RespawnPlayer()
            return
        end

        if self._justRespawnedPlayer then
            self._animator:Stop(self.entityId)
            self._animator:Play(self.entityId)
            self._justRespawnedPlayer = false
        end

        if not self._collider or not self._transform or not self._controller or self._playerDead then
            return
        end

        if self._freezePending then
            self._freezeSettleTimer = self._freezeSettleTimer - dt
            if self._freezeSettleTimer <= 0 then
                self._freezePending = false
                self._frozenBycinematic = true
            end
        end

        if self._frozenBycinematic then
            local position = CharacterController.GetPosition(self._controller)
            if position then
                self:SetPosition(position.x, position.y, position.z)
                if event_bus and event_bus.publish then
                    event_bus.publish("player_position", position)
                end
            end
            return
        end

        if self._kbPending then
            self._kbPending = false
            CharacterController.Move(self._controller, (self._kbX or 0), 0, (self._kbZ or 0))
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
            local position = CharacterController.GetPosition(self._controller)
            if position then
                self:SetPosition(position.x, position.y, position.z)
                if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
            end
            return
        end

        if self._dashCooldownTimer > 0 then
            self._dashCooldownTimer = self._dashCooldownTimer - dt
        end

        if self._lungeTimer and self._lungeTimer > 0 then
            self._lungeTimer = self._lungeTimer - dt
            CharacterController.Move(self._controller,
                self._lungeDirX * (self.AttackLungeSpeed or 3.0), 0,
                self._lungeDirZ * (self.AttackLungeSpeed or 3.0))
        end

        if _G.player_is_attacking then
            self._animator:SetBool("IsRunning", false)
            self._isRunning = false
            local position = CharacterController.GetPosition(self._controller)
            if position then
                self:SetPosition(position.x, position.y, position.z)
                if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
            end
            return
        end

        -- CHAIN MOVEMENT CONSTRAINT
        tensionRadialX = 0
        tensionRadialZ = 0
        if self._chainConstraintExceeded then
            self._chainConstraintRatio = 0
            self._chainConstraintExceeded = false
            self._chainDrag = false
        elseif self._chainDrag then
            self:SetPosition(self._chainDragTargetX, self._chainDragTargetY, self._chainDragTargetZ)
            CharacterController.SetPosition(self._controller, self._transform)
        elseif self._chainConstraintRatio and self._chainConstraintRatio > 0 then
            local r = self._chainConstraintRatio
            if self._chainEndX and self._chainEndZ then
                local pos = CharacterController.GetPosition(self._controller)
                if pos then
                    local radX = pos.x - self._chainEndX
                    local radZ = pos.z - self._chainEndZ
                    local radLen = math.sqrt(radX * radX + radZ * radZ)
                    if radLen > 1e-4 then
                        tensionRadialX = radX / radLen
                        tensionRadialZ = radZ / radLen
                        tensionScale   = math.max(0.0, 1.0 - r * r * r * 0.95)
                    end
                end
            end
        end

        -- RAW INPUT
        local axis
        if self._freezePending then
            axis = { x = 0, y = 0 }
        else
            axis = Input and Input.GetAxis and Input.GetAxis("Movement") or { x = 0, y = 0 }
        end
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
        local isGrounded = CharacterController.IsGrounded(self._controller)
        local isJumping = false
        self._animator:SetBool("IsGrounded", isGrounded)

        if self._playerDeadPending and isGrounded then
            if self._animator then
                self._animator:SetBool("IsDead", true)
                dbg("[PlayerMovement] Player grounded, playing Death animation")
            end
            self._playerDead = true
            self._playerDeadPending = false
            return
        end

        -- DASH
        if not self._isDashing
            and self._dashCooldownTimer <= 0
            and not self._isDamageStun
            and not self._isLanding
            and not self._freezePending
            and Input and Input.IsActionJustPressed and Input.IsActionJustPressed("Dash")
        then
            self._isDashing = true
            self._dashTimer = self.DashDuration
            _G.player_is_dashing = true
            self._wasDashingInAir = not isGrounded

            if isMoving then
                local len = math.sqrt(moveX * moveX + moveZ * moveZ)
                self._dashDirX = moveX / len
                self._dashDirZ = moveZ / len
            else
                local halfAngle = math.acos(math.max(-1, math.min(1, self._currentRotW)))
                local sinHalf = math.sin(halfAngle)
                if sinHalf > 0.001 then
                    local yAxis = self._currentRotY / sinHalf
                    local angle = 2 * halfAngle * (yAxis >= 0 and 1 or -1)
                    self._dashDirX = math.sin(angle)
                    self._dashDirZ = math.cos(angle)
                else
                    self._dashDirX = 0
                    self._dashDirZ = 1
                end
            end

            self._animator:SetBool("IsJumping", false)
            self._isJumping = false
            self._animator:SetBool("IsRunning", false)
            self._isRunning = false
            self._animator:SetBool("IsDashing", true)
            dbg("[PlayerMovement] Dash started")
        end

        if self._isDashing then
            self._dashTimer = self._dashTimer - dt

            if Input and Input.IsActionJustPressed and Input.IsActionJustPressed("Jump") and isGrounded then
                self._isDashing = false
                _G.player_is_dashing = false
                self._animator:SetBool("IsDashing", false)
                self._dashCooldownTimer = self.DashCooldown
                CharacterController.Jump(self._controller, self.JumpHeight)
                self._animator:SetBool("IsJumping", true)
                playRandomSFX(self._audio, self.playerJumpSFX)
                dbg("[PlayerMovement] Dash jump-cancelled")
            elseif self._dashTimer <= 0 then
                self._isDashing = false
                _G.player_is_dashing = false
                self._animator:SetBool("IsDashing", false)
                self._dashCooldownTimer = self.DashCooldown
                self._isLanding = true
                self._isRolling = true
                self._wasDashingInAir = false
                dbg("[PlayerMovement] Dash ended")
            else
                local speed = self.DashSpeed
                local liftY = 0
                if not isGrounded or self._wasDashingInAir then
                    speed = self.DashSpeed * self.AirDashSpeedMultiplier
                    liftY = self.AirDashLift
                end
                CharacterController.Move(self._controller,
                    self._dashDirX * speed, liftY, self._dashDirZ * speed)
            end

            if not isGrounded then self._wasDashingInAir = true end

            if self.SetRotation then
                local targetW, targetX, targetY, targetZ = directionToQuaternion(self._dashDirX, self._dashDirZ)
                self._currentRotW, self._currentRotX, self._currentRotY, self._currentRotZ = targetW, targetX, targetY, targetZ
                pcall(self.SetRotation, self, targetW, targetX, targetY, targetZ)
            end

            local position = CharacterController.GetPosition(self._controller)
            if position then
                self:SetPosition(position.x, position.y, position.z)
                if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
            end
            return
        end

        -- JUMP
        if not self._isLanding and not self._freezePending
            and Input and Input.IsActionJustPressed and Input.IsActionJustPressed("Jump") and isGrounded
        then
            CharacterController.Jump(self._controller, self.JumpHeight)
            isJumping = true
            self._animator:SetBool("IsJumping", true)
            playRandomSFX(self._audio, self.playerJumpSFX)
        end

        -- APPLY MOVEMENT
        if not isJumping and isMoving then
            local mx, mz = moveX * self.Speed, moveZ * self.Speed
            -- only resist the outward component; lateral/inward movement is unaffected
            local dot = mx * tensionRadialX + mz * tensionRadialZ
            if dot > 0 then
                mx = mx - tensionRadialX * dot * (1.0 - tensionScale)
                mz = mz - tensionRadialZ * dot * (1.0 - tensionScale)
            end
            CharacterController.Move(self._controller, mx, 0, mz)
        end

        if self._isRolling and not isMoving then
            local yawRad = math.rad(cameraYaw)
            local sinYaw = math.sin(yawRad)
            local cosYaw = math.cos(yawRad)
            moveX = self._prevRawZ * (-sinYaw) - self._prevRawX * cosYaw
            moveZ = self._prevRawZ * (-cosYaw) + self._prevRawX * sinYaw
            local mx, mz = moveX * self.Speed, moveZ * self.Speed
            local dot = mx * tensionRadialX + mz * tensionRadialZ
            if dot > 0 then
                mx = mx - tensionRadialX * dot * (1.0 - tensionScale)
                mz = mz - tensionRadialZ * dot * (1.0 - tensionScale)
            end
            CharacterController.Move(self._controller, mx, 0, mz)
        end

        -- ANIMATION
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
                playRandomSFX(self._audio, self.playerLandSFX)
                if isMoving then
                    self._animator:SetBool("IsRunning", true)
                    self._isRunning = true
                    self._isRolling = true
                else
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

        -- FOOTSTEPS
        if self._isRunning and isGrounded and not self._isLanding then
            if not self._wasRunning then
                playRandomSFX(self._audio, self.playerFootstepSFX)
                self._footstepTimer = 0
            end
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
        if isMoving then
            local mag = math.sqrt(moveX * moveX + moveZ * moveZ)
            self._facingX = moveX / mag
            self._facingZ = moveZ / mag

            local targetW, targetX, targetY, targetZ = directionToQuaternion(moveX, moveZ)
            local t = math.min(self.rotationSpeed * dt, 1.0)
            local newW, newX, newY, newZ = lerpQuaternion(
                self._currentRotW, self._currentRotX,
                self._currentRotY, self._currentRotZ,
                targetW, targetX, targetY, targetZ, t)

            self._currentRotW, self._currentRotX, self._currentRotY, self._currentRotZ = newW, newX, newY, newZ
            pcall(self.SetRotation, self, newW, newX, newY, newZ)
        end

        -- POSITION SYNC
        local position = CharacterController.GetPosition(self._controller)
        if position then
            self:SetPosition(position.x, position.y, position.z)
            if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe then
            if self._cameraYawSub          then event_bus.unsubscribe(self._cameraYawSub)          end
            if self._freezePlayerSub        then event_bus.unsubscribe(self._freezePlayerSub)       end
            if self._requestPlayerForwardSub then event_bus.unsubscribe(self._requestPlayerForwardSub) end
            if self._attackLungeSub         then event_bus.unsubscribe(self._attackLungeSub)        end
            if self._chainConstraintSub     then event_bus.unsubscribe(self._chainConstraintSub)    end
        end
        self._frozenBycinematic = false
        self._chainConstraintRatio = 0
        self._chainConstraintExceeded = false
        self._chainDrag = false
    end,
}