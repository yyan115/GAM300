--[[
================================================================================
PLAYER MOVEMENT
================================================================================
PURPOSE:
    Executes physical movement for the player character. Reads movement intent
    from InputInterpreter and responds to combat decisions published by
    ComboManager via the event bus.

SINGLE RESPONSIBILITY: Execute movement and physics. Nothing else.

RESPONSIBILITIES:
    - Read movement axis and jump from InputInterpreter (never raw Input)
    - Execute CharacterController movement, dash physics, jump, and lunge
    - Respond to combat_state_changed: lock/unlock movement, bleed momentum
    - Respond to attack_performed: apply per-attack lunge impulse
    - Respond to dash_performed: execute the dash (duration from ComboManager)
    - Respond to chain events: apply movement constraints
    - Drive movement-specific animator parameters (IsRunning, IsJumping, etc.)
    - Manage respawn, damage stun, landing, cinematic freeze

NOT RESPONSIBLE FOR:
    - Reading raw engine input directly (owned by InputInterpreter)
    - Deciding which attack or combo to execute (owned by ComboManager)
    - Publishing chain or combat events (owned by ComboManager)

MOVEMENT LOCK RULES:
    _G.player_is_attacking && !_G.player_can_move → full movement lock
    _G.player_is_attacking &&  _G.player_can_move → movement allowed (canMove state)
    Momentum bleed: on attack entry, carry a fraction of running velocity into
    the first frames of the attack so the transition feels grounded.

EVENTS CONSUMED:
    camera_yaw                     → update camera-relative movement yaw
    playerDead                     → trigger death state
    playerHurtTriggered            → apply damage stun
    player_knockback               → apply knockback impulse
    respawnPlayer                  → respawn to checkpoint or origin
    activatedCheckpoint            → remember last checkpoint entity
    freeze_player                  → cinematic freeze with settle timer
    request_player_forward         → reply with current facing direction
    attack_performed               → execute per-attack lunge impulse
    force_player_rotation_to_camera → snap rotation to camera forward
    force_player_rotation_to_direction → snap rotation to arbitrary direction
    combat_state_changed           → update movement lock and momentum bleed
    dash_performed                 → begin dash (duration supplied by event)
    chain.movement_constraint      → apply chain length / drag constraint

AUTHOR: Soh Wei Jie
VERSION: 2.0
================================================================================
--]]

require("extension.engine_bootstrap")
_G.CHAIN_DEBUG = _G.CHAIN_DEBUG ~= nil and _G.CHAIN_DEBUG or false
local function dbg(...) if _G.CHAIN_DEBUG then print(...) end end
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

-- Animation state indices
local IDLE = 0
local RUN  = 1
local JUMP = 2

-- Chain tension (updated from chain.movement_constraint event)
local tensionRadialX = 0
local tensionRadialZ = 0
local tensionScale   = 7.0

-- ── Quaternion helpers ────────────────────────────────────────────────────────

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

local function lerpQuaternion(w1, x1, y1, z1, w2, x2, y2, z2, t)
    if w1*w2 + x1*x2 + y1*y2 + z1*z2 < 0 then
        w2, x2, y2, z2 = -w2, -x2, -y2, -z2
    end
    local w = w1 + (w2 - w1) * t
    local x = x1 + (x2 - x1) * t
    local y = y1 + (y2 - y1) * t
    local z = z1 + (z2 - z1) * t
    local len = math.sqrt(w*w + x*x + y*y + z*z + 0.0001)
    return w/len, x/len, y/len, z/len
end

local function playRandomSFX(audio, clips)
    local count = clips and #clips or 0
    if count == 0 or not audio then return end
    if count == 1 then audio:PlayOneShot(clips[1]); return end
    local idx
    repeat idx = math.random(1, count) until idx ~= clips._lastIdx
    clips._lastIdx = idx
    audio:PlayOneShot(clips[idx])
end

-- ─────────────────────────────────────────────────────────────────────────────

return Component {
    mixins = { TransformMixin },

    fields = {
        Speed                 = 1.5,
        JumpHeight            = 1.2,
        DamageStunDuration    = 1.0,
        LandingDuration       = 0.5,
        footstepInterval      = 0.35,
        DashSpeed             = 5.0,
        DashDuration          = 1.0,   -- fallback; ComboManager publishes the real value
        DashCooldown          = 1.5,
        CinematicSettleTime   = 0.8,
        AirDashSpeedMultiplier = 1.5,
        AirDashLift           = 2.0,
        AttackLungeSpeed      = 3.0,   -- fallback; ComboManager publishes per-attack values
        AttackLungeDuration   = 0.12,  -- fallback
        playerFootstepSFX     = {},
        playerHurtSFX         = {},
        playerJumpSFX         = {},
        playerLandSFX         = {},
        playerDeadSFX         = {},
        playerDashSFX         = {},
    },

    Awake = function(self)
        self._currentRotW = 1
        self._currentRotX = 0
        self._currentRotY = 0
        self._currentRotZ = 0

        -- Camera yaw (drives camera-relative movement)
        self._cameraYaw    = 180.0
        self._cameraYawSub = nil

        -- Chain constraint state (from chain.movement_constraint event)
        self._chainConstraintRatio    = 0
        self._chainConstraintExceeded = false
        self._chainDrag               = false
        self._chainDragTargetX        = 0
        self._chainDragTargetY        = 0
        self._chainDragTargetZ        = 0

        -- ── Movement lock from combat ─────────────────────────────────────
        -- Updated by combat_state_changed event; true by default so movement
        -- works before any combat event has been received.
        self._playerCanMove = true

        -- ── Momentum bleed ────────────────────────────────────────────────
        -- On attack entry, carry a fraction of running velocity for a short
        -- window so the transition feels grounded rather than a dead stop.
        self._momentumX        = 0
        self._momentumZ        = 0
        self._momentumTimer    = 0
        self._MOMENTUM_DURATION = 0.12  -- seconds
        self._MOMENTUM_SCALE    = 0.6   -- fraction of running speed to carry

        -- ── Lunge state (per-attack values from attack_performed event) ────
        self._lungeTimer  = 0
        self._lungeDirX   = 0
        self._lungeDirZ   = 0
        self._lungeSpeed  = self.AttackLungeSpeed

        -- ── Knockback ─────────────────────────────────────────────────────
        self._kbPending = false
        self._kbX       = 0
        self._kbZ       = 0

        if event_bus and event_bus.subscribe then

            -- Camera yaw for camera-relative movement
            dbg("[PlayerMovement] Subscribing to camera_yaw")
            self._cameraYawSub = event_bus.subscribe("camera_yaw", function(yaw)
                if yaw then self._cameraYaw = yaw end
            end)

            -- Death
            dbg("[PlayerMovement] Subscribing to playerDead")
            self._playerDeadSub = event_bus.subscribe("playerDead", function(playerDead)
                if playerDead then
                    dbg("[PlayerMovement] Received playerDead")
                    self._playerDeadPending = playerDead
                    playRandomSFX(self._audio, self.playerDeadSFX)
                end
            end)

            -- Damage stun
            dbg("[PlayerMovement] Subscribing to playerHurtTriggered")
            self._playerHurtTriggeredSub = event_bus.subscribe("playerHurtTriggered", function(hit)
                if hit then
                    self._isDamageStun = true
                    if self._animator then self._animator:SetBool("IsJumping", false) end
                    playRandomSFX(self._audio, self.playerHurtSFX)
                end
            end)

            -- Knockback impulse
            dbg("[PlayerMovement] Subscribing to player_knockback")
            self._knockSub = event_bus.subscribe("player_knockback", function(p)
                if not p then return end
                self._kbX      = (p.x or 0) * (p.strength or 0)
                self._kbZ      = (p.z or 0) * (p.strength or 0)
                self._kbPending = true
            end)

            -- Respawn
            dbg("[PlayerMovement] Subscribing to respawnPlayer")
            self._respawnPlayerSub = event_bus.subscribe("respawnPlayer", function(respawn)
                if respawn then self._respawnPlayer = true end
            end)

            -- Checkpoint
            self._activatedCheckpointSub = event_bus.subscribe("activatedCheckpoint", function(entityId)
                if entityId then self._activatedCheckpoint = entityId end
            end)

            -- Cinematic freeze (settle timer → hard freeze)
            dbg("[PlayerMovement] Subscribing to freeze_player")
            self._frozenBycinematic  = false
            self._freezePending      = false
            self._freezeSettleTimer  = 0.0
            self._freezePlayerSub    = event_bus.subscribe("freeze_player", function(frozen)
                if frozen then
                    self._freezePending     = true
                    self._freezeSettleTimer = self.CinematicSettleTime or 0.8
                else
                    self._frozenBycinematic = false
                    self._freezePending     = false
                end
                dbg("[PlayerMovement] Frozen = " .. tostring(frozen))
            end)

            -- Forward direction request (chain weapon / other systems)
            dbg("[PlayerMovement] Subscribing to request_player_forward")
            self._requestPlayerForwardSub = event_bus.subscribe("request_player_forward", function(_)
                if not self._facingX or not self._facingZ then return end
                if event_bus and event_bus.publish then
                    event_bus.publish("player_forward_response", {
                        x = self._facingX,
                        y = 0,
                        z = self._facingZ,
                    })
                end
            end)

            -- ── Attack lunge ──────────────────────────────────────────────
            -- ComboManager publishes lunge = { speed, duration } per state.
            -- PlayerMovement executes the impulse; it does not decide the values.
            self._attackLungeSub = event_bus.subscribe("attack_performed", function(data)
                local cameraYaw = _G.CAMERA_YAW or self._cameraYaw or 180.0
                local yr   = math.rad(cameraYaw)
                local fwdX = -math.sin(yr)
                local fwdZ = -math.cos(yr)
                local len  = math.sqrt(fwdX * fwdX + fwdZ * fwdZ)
                if len > 0.001 then fwdX = fwdX / len; fwdZ = fwdZ / len end

                self._lungeDirX = fwdX
                self._lungeDirZ = fwdZ

                -- Use per-attack lunge data from ComboManager; fall back to inspector defaults.
                local lunge = data and data.lunge
                self._lungeTimer = (lunge and lunge.duration) or self.AttackLungeDuration or 0.12
                self._lungeSpeed = (lunge and lunge.speed)    or self.AttackLungeSpeed    or 3.0

                -- Snap rotation to face camera direction on attack
                local targetW, targetX, targetY, targetZ = directionToQuaternion(fwdX, fwdZ)
                self._currentRotW = targetW
                self._currentRotX = targetX
                self._currentRotY = targetY
                self._currentRotZ = targetZ
                self._facingX     = fwdX
                self._facingZ     = fwdZ
                pcall(self.SetRotation, self, targetW, targetX, targetY, targetZ)
            end)

            -- ── Combat state → movement lock + momentum bleed ─────────────
            -- When an attack begins, capture running velocity and bleed it
            -- out over _MOMENTUM_DURATION so the transition feels grounded.
            self._combatStateSub = event_bus.subscribe("combat_state_changed", function(data)
                if not data then return end
                self._playerCanMove = data.canMove ~= false

                if data.state ~= "idle" and data.state ~= "dash" then
                    -- Capture current facing velocity as bleed momentum
                    local speed = self.Speed or 1.5
                    self._momentumX     = (self._facingX or 0) * speed * self._MOMENTUM_SCALE
                    self._momentumZ     = (self._facingZ or 0) * speed * self._MOMENTUM_SCALE
                    self._momentumTimer = self._MOMENTUM_DURATION
                else
                    -- Returning to idle or dashing: clear bleed
                    self._momentumTimer = 0
                end
            end)

            -- Snap rotation to camera forward (skill / cutscene trigger)
            self._forceRotSub = event_bus.subscribe("force_player_rotation_to_camera", function()
                local cameraYaw = _G.CAMERA_YAW or self._cameraYaw or 180.0
                local yr   = math.rad(cameraYaw)
                local fwdX = -math.sin(yr)
                local fwdZ = -math.cos(yr)
                local len  = math.sqrt(fwdX * fwdX + fwdZ * fwdZ)
                if len > 0.001 then fwdX = fwdX / len; fwdZ = fwdZ / len end

                local targetW, targetX, targetY, targetZ = directionToQuaternion(fwdX, fwdZ)
                self._currentRotW = targetW
                self._currentRotX = targetX
                self._currentRotY = targetY
                self._currentRotZ = targetZ
                self._facingX     = fwdX
                self._facingZ     = fwdZ
                pcall(self.SetRotation, self, targetW, targetX, targetY, targetZ)
            end)

            -- Snap rotation to an arbitrary world direction
            self._chainFiredRotSub = event_bus.subscribe("force_player_rotation_to_direction", function(payload)
                if not payload then return end
                local dx, dz = payload.x, payload.z
                if not dx or not dz then return end
                local len = math.sqrt(dx * dx + dz * dz)
                if len < 0.001 then return end
                dx, dz = dx / len, dz / len
                local targetW, targetX, targetY, targetZ = directionToQuaternion(dx, dz)
                self._currentRotW = targetW
                self._currentRotX = targetX
                self._currentRotY = targetY
                self._currentRotZ = targetZ
                self._facingX     = dx
                self._facingZ     = dz
                pcall(self.SetRotation, self, targetW, targetX, targetY, targetZ)
            end)

            -- ── Dash ──────────────────────────────────────────────────────
            -- ComboManager publishes dash_performed with duration when it
            -- decides a dash should happen. PlayerMovement just executes it.
            -- Duration comes from the event so there is a single source of truth.
            self._dashRequested = false
            self._dashDuration  = self.DashDuration  -- will be overwritten by event
            self._dashPerformedSub = event_bus.subscribe("dash_performed", function(data)
                self._dashRequested = true
                self._dashDuration  = (data and data.duration) or self.DashDuration
                print("[PlayerMovement] dash_performed: duration=" .. tostring(self._dashDuration))
            end)

            -- ── Chain movement constraint ─────────────────────────────────
            self._chainConstraintSub = event_bus.subscribe("chain.movement_constraint", function(payload)
                if not payload then return end
                self._chainConstraintRatio    = payload.ratio    or 0
                self._chainConstraintExceeded = payload.exceeded or false
                self._chainDrag               = payload.drag     or false
                self._chainEndX               = payload.endX
                self._chainEndY               = payload.endY
                self._chainEndZ               = payload.endZ
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

        dbg("transform y: ", self._transform.localPosition.y)
        self._controller = CharacterController.Create(self.entityId, self._collider, self._transform)

        if self._animator then
            dbg("[PlayerMovement] Animator found, playing IDLE clip")
            self._animator:PlayClip(IDLE, true)
        else
            dbg("[PlayerMovement] ERROR: Animator is nil!")
        end

        self._isRunning  = false
        self._isJumping  = false
        self.rotationSpeed = 10.0

        self._damageStunDuration = self.DamageStunDuration
        self._isDamageStun       = false
        self._landingDuration    = self.LandingDuration

        self._isDashing          = false
        self._dashTimer          = 0
        self._dashCooldownTimer  = 0
        self._dashDirX           = 0
        self._dashDirZ           = 0
        self._wasDashingInAir    = false
        _G.player_is_dashing     = false

        self._footstepTimer = 0
        self._wasRunning    = false

        local pos = self._transform.worldPosition
        self._initialSpawnPoint = { x = pos.x, y = pos.y, z = pos.z }
    end,

    RespawnPlayer = function(self)
        local respawnPos = self._initialSpawnPoint

        if self._activatedCheckpoint then
            local checkpointTransform = GetComponent(self._activatedCheckpoint, "Transform")
            local checkpointPos       = checkpointTransform.worldPosition
            self:SetPosition(checkpointPos.x, checkpointPos.y, checkpointPos.z)
            respawnPos = checkpointPos
        elseif self._initialSpawnPoint then
            self:SetPosition(self._initialSpawnPoint.x, self._initialSpawnPoint.y, self._initialSpawnPoint.z)
        end

        CharacterController.SetPosition(self._controller, self._transform)
        self._respawnPlayer       = false
        self._playerDead          = false
        self._playerDeadPending   = false
        self._animator:SetBool("IsDead", false)
        self._justRespawnedPlayer = true

        -- Reset movement lock and momentum on respawn
        self._playerCanMove = true
        self._momentumTimer = 0

        -- Reset chain constraint
        self._chainConstraintRatio    = 0
        self._chainConstraintExceeded = false

        if event_bus and event_bus.publish then
            event_bus.publish("playerRespawned", respawnPos)
            dbg(string.format("[PlayerMovement] Respawned to %f %f %f",
                respawnPos.x, respawnPos.y, respawnPos.z))
        end
    end,

    Update = function(self, dt)
        -- Sync global state flags for other systems (e.g. FeatherSkillManager)
        _G.player_is_jumping = self._isJumping  or false
        _G.player_is_rolling = self._isRolling  or false
        _G.player_is_landing = self._isLanding  or false
        _G.player_is_hurt    = self._isDamageStun or false
        _G.player_is_dead    = self._playerDead or false
        _G.player_is_frozen  = self._frozenBycinematic or self._freezePending or false

        -- ── Respawn ───────────────────────────────────────────────────────
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

        -- ── Cinematic freeze settle timer ─────────────────────────────────
        if self._freezePending then
            self._freezeSettleTimer = self._freezeSettleTimer - dt
            if self._freezeSettleTimer <= 0 then
                self._freezePending     = false
                self._frozenBycinematic = true
            end
        end

        if self._frozenBycinematic then
            local position = CharacterController.GetPosition(self._controller)
            if position then
                self:SetPosition(position.x, position.y, position.z)
                if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
            end
            return
        end

        -- ── Knockback ─────────────────────────────────────────────────────
        if self._kbPending then
            self._kbPending = false
            CharacterController.Move(self._controller, self._kbX or 0, 0, self._kbZ or 0)
            self._kbX, self._kbZ = 0, 0
        end

        -- ── Damage stun timer ─────────────────────────────────────────────
        if self._isDamageStun then
            self._damageStunDuration = self._damageStunDuration - dt
            if self._damageStunDuration <= 0 then
                self._damageStunDuration = self.DamageStunDuration
                self._isDamageStun       = false
            end
        end

        -- ── Landing recovery timer ─────────────────────────────────────────
        if self._isLanding then
            self._landingDuration = self._landingDuration - dt
            if self._landingDuration <= 0 then
                self._landingDuration = self.LandingDuration
                self._isLanding       = false
                self._isRolling       = false
            end
        end

        -- During damage stun: keep position in sync, skip all other logic
        if self._isDamageStun then
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

        -- ── Dash cooldown ─────────────────────────────────────────────────
        if self._dashCooldownTimer > 0 then
            self._dashCooldownTimer = self._dashCooldownTimer - dt
        end

        -- ── Attack lunge impulse ──────────────────────────────────────────
        -- Speed and duration are set by the attack_performed subscriber.
        -- This block just applies the cached values each frame.
        if self._lungeTimer and self._lungeTimer > 0 then
            self._lungeTimer = self._lungeTimer - dt
            CharacterController.Move(self._controller,
                self._lungeDirX * self._lungeSpeed,
                0,
                self._lungeDirZ * self._lungeSpeed)
        end

        -- ── Skill cast lock ───────────────────────────────────────────────
        if _G.player_is_casting_skill then
            self._animator:SetBool("IsRunning", false)
            self._isRunning = false
            local position = CharacterController.GetPosition(self._controller)
            if position then
                self:SetPosition(position.x, position.y, position.z)
                if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
            end
            return
        end

        -- ── Combat movement lock ──────────────────────────────────────────
        -- _G.player_is_attacking is set by ComboManager.
        -- _G.player_can_move refines it: even during an attack, canMove=true
        -- states (future design space) allow the player to keep moving.
        -- Momentum bleed always runs to carry entry velocity into the lock.
        if _G.player_is_attacking and not _G.player_can_move then
            -- Momentum bleed: decay into the attack so entry doesn't feel abrupt
            if self._momentumTimer and self._momentumTimer > 0 then
                self._momentumTimer = self._momentumTimer - dt
                local t = math.max(0, self._momentumTimer / self._MOMENTUM_DURATION)
                CharacterController.Move(self._controller,
                    self._momentumX * t, 0, self._momentumZ * t)
            end

            self._animator:SetBool("IsRunning", false)
            self._isRunning = false
            local position = CharacterController.GetPosition(self._controller)
            if position then
                self:SetPosition(position.x, position.y, position.z)
                if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
            end
            return
        end

        -- ── Chain movement constraint ─────────────────────────────────────
        tensionRadialX = 0
        tensionRadialZ = 0

        if self._chainConstraintExceeded then
            self._chainConstraintRatio    = 0
            self._chainConstraintExceeded = false
            self._chainDrag               = false
        elseif self._chainDrag then
            self:SetPosition(self._chainDragTargetX, self._chainDragTargetY, self._chainDragTargetZ)
            CharacterController.SetPosition(self._controller, self._transform)
        elseif self._chainConstraintRatio and self._chainConstraintRatio > 0 then
            local pos = CharacterController.GetPosition(self._controller)
            if pos and self._chainEndX and self._chainEndZ then
                local radX = pos.x - self._chainEndX
                local radZ = pos.z - self._chainEndZ
                local radLen = math.sqrt(radX * radX + radZ * radZ)
                if radLen > 1e-4 then
                    tensionRadialX = radX / radLen
                    tensionRadialZ = radZ / radLen
                    tensionScale   = math.max(0.0, 1.0 - self._chainConstraintRatio^3 * 0.95)
                end
            end
        end

        -- ── Read movement axis from InputInterpreter ──────────────────────
        -- PlayerMovement never touches _G.Input directly. InputInterpreter
        -- is the single owner of raw input polling.
        local axis
        if self._freezePending then
            axis = { x = 0, y = 0 }
        else
            local interp = _G.InputInterpreter
            axis = (interp and interp:GetMovementAxis()) or { x = 0, y = 0 }
        end

        local rawX = -axis.x
        local rawZ =  axis.y

        -- ── Camera-relative movement ──────────────────────────────────────
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

        local isMoving  = (moveX ~= 0 or moveZ ~= 0)
        local isGrounded = CharacterController.IsGrounded(self._controller)
        local isJumping  = false
        self._animator:SetBool("IsGrounded", isGrounded)

        -- ── Death pending ─────────────────────────────────────────────────
        if self._playerDeadPending and isGrounded then
            if self._animator then
                self._animator:SetBool("IsDead", true)
                dbg("[PlayerMovement] Player grounded, playing Death animation")
            end
            self._playerDead        = true
            self._playerDeadPending = false
            return
        end

        -- ── Dash execution ────────────────────────────────────────────────
        -- ComboManager decides a dash happens; this block executes the physics.
        -- self._dashDuration is set by the dash_performed event so ComboManager's
        -- combo tree is the single source of truth for dash timing.
        if not self._isDashing
            and self._dashRequested
            and self._dashCooldownTimer <= 0
            and not self._isDamageStun
            and not self._isLanding
            and not self._freezePending
        then
            self._dashRequested = false
            self._isDashing     = true
            self._dashTimer     = self._dashDuration  -- from dash_performed event
            _G.player_is_dashing = true
            print("[PlayerMovement] Dash started (duration=" .. tostring(self._dashTimer) .. ")")
            self._wasDashingInAir = not isGrounded

            -- Dash direction: prefer input, fall back to current facing
            if isMoving then
                local len = math.sqrt(moveX * moveX + moveZ * moveZ)
                self._dashDirX = moveX / len
                self._dashDirZ = moveZ / len
            else
                local halfAngle = math.acos(math.max(-1, math.min(1, self._currentRotW)))
                local sinHalf   = math.sin(halfAngle)
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
            playRandomSFX(self._audio, self.playerDashSFX)

        elseif self._dashRequested then
            -- Conditions not met — discard request
            print("[PlayerMovement] Dash discarded (cooldown=" .. self._dashCooldownTimer
                .. " stun=" .. tostring(self._isDamageStun)
                .. " landing=" .. tostring(self._isLanding) .. ")")
            self._dashRequested = false
        end

        if self._isDashing then
            self._dashTimer = self._dashTimer - dt

            if self._dashTimer <= 0 then
                self._isDashing          = false
                _G.player_is_dashing     = false
                self._wasDashingInAir    = false
                self._animator:SetBool("IsDashing", false)
                self._dashCooldownTimer  = self.DashCooldown

                if event_bus and event_bus.publish then
                    event_bus.publish("dash_ended", { cooldown = self.DashCooldown })
                end
                print("[PlayerMovement] Dash ended")
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
                self._currentRotW, self._currentRotX, self._currentRotY, self._currentRotZ =
                    targetW, targetX, targetY, targetZ
                pcall(self.SetRotation, self, targetW, targetX, targetY, targetZ)
            end

            local position = CharacterController.GetPosition(self._controller)
            if position then
                self:SetPosition(position.x, position.y, position.z)
                if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
            end
            return
        end

        -- ── Jump ──────────────────────────────────────────────────────────
        -- Read from InputInterpreter — no direct Input access.
        local interp = _G.InputInterpreter
        if not self._isLanding and not self._freezePending
            and interp and interp:IsJumpJustPressed() and isGrounded
        then
            CharacterController.Jump(self._controller, self.JumpHeight)
            isJumping = true
            self._animator:SetBool("IsJumping", true)
            playRandomSFX(self._audio, self.playerJumpSFX)
        end

        -- ── Apply movement ─────────────────────────────────────────────────
        if not isJumping and isMoving then
            local mx, mz = moveX * self.Speed, moveZ * self.Speed

            -- Resist the outward component against chain tension;
            -- lateral / inward movement is unaffected.
            local dot = mx * tensionRadialX + mz * tensionRadialZ
            if dot > 0 then
                mx = mx - tensionRadialX * dot * (1.0 - tensionScale)
                mz = mz - tensionRadialZ * dot * (1.0 - tensionScale)
            end
            CharacterController.Move(self._controller, mx, 0, mz)
        end

        -- Rolling momentum (carry previous direction on landing without input)
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

        -- ── Animation ─────────────────────────────────────────────────────
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

        -- ── Footsteps ─────────────────────────────────────────────────────
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

        -- ── Rotation ──────────────────────────────────────────────────────
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

            self._currentRotW, self._currentRotX, self._currentRotY, self._currentRotZ =
                newW, newX, newY, newZ
            pcall(self.SetRotation, self, newW, newX, newY, newZ)
        end

        -- ── Position sync ─────────────────────────────────────────────────
        local position = CharacterController.GetPosition(self._controller)
        if position then
            self:SetPosition(position.x, position.y, position.z)
            if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe then
            if self._cameraYawSub             then event_bus.unsubscribe(self._cameraYawSub)             end
            if self._playerDeadSub            then event_bus.unsubscribe(self._playerDeadSub)            end
            if self._playerHurtTriggeredSub   then event_bus.unsubscribe(self._playerHurtTriggeredSub)   end
            if self._knockSub                 then event_bus.unsubscribe(self._knockSub)                 end
            if self._respawnPlayerSub         then event_bus.unsubscribe(self._respawnPlayerSub)         end
            if self._activatedCheckpointSub   then event_bus.unsubscribe(self._activatedCheckpointSub)  end
            if self._freezePlayerSub          then event_bus.unsubscribe(self._freezePlayerSub)          end
            if self._requestPlayerForwardSub  then event_bus.unsubscribe(self._requestPlayerForwardSub) end
            if self._attackLungeSub           then event_bus.unsubscribe(self._attackLungeSub)           end
            if self._combatStateSub           then event_bus.unsubscribe(self._combatStateSub)           end
            if self._forceRotSub              then event_bus.unsubscribe(self._forceRotSub)              end
            if self._chainFiredRotSub         then event_bus.unsubscribe(self._chainFiredRotSub)         end
            if self._dashPerformedSub         then event_bus.unsubscribe(self._dashPerformedSub)         end
            if self._chainConstraintSub       then event_bus.unsubscribe(self._chainConstraintSub)       end
        end

        self._frozenBycinematic       = false
        self._playerCanMove           = true
        self._momentumTimer           = 0
        self._chainConstraintRatio    = 0
        self._chainConstraintExceeded = false
        self._chainDrag               = false
    end,
}