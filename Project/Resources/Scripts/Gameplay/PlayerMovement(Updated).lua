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
    - Respond to combat_state_changed: lock/unlock movement (velocity carries naturally)
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
    combat_state_changed           → update movement lock (velocity handles carry-over)
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

-- =====================================================================
-- Quaternion helpers

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

-- =====================================================================

return Component {
    mixins = { TransformMixin },

    fields = {
        -- MOVEMENT
        -- =====================================================================

        -- Top running speed (world units per second).
        -- Raise this to make the player feel faster overall.
        -- Suggested range: 2.5 (slow/deliberate) – 6.0 (fast/arcade)
        Speed = 4.0,

        -- =====================================================================
        -- Momentum rates
        -- These control how velocity changes, not raw speed.

        -- How quickly velocity climbs toward Speed when input is held.
        -- High = snappy start. Low = heavy, takes time to reach full speed.
        -- Suggested range: 10 (tank) – 30 (responsive)
        Acceleration = 22.0,

        -- How quickly velocity bleeds to zero when input is released.
        -- High = stops fast. Low = slides to a halt.
        -- Suggested range: 8 (slidey) – 20 (firm stop)
        Deceleration = 16.0,

        -- Extra deceleration rate when input direction opposes current velocity.
        -- Controls how quickly the player pivots into a new direction.
        -- High = sharp pivot. Low = wide drifty arc.
        -- Suggested range: 15 (loose) – 50 (instant pivot)
        TurnDecel = 32.0,

        -- Velocity bleed rate while a combo lock is active (player is attacking).
        -- Intentionally lower than Deceleration so momentum carries into hits.
        -- High = stops dead on attack. Low = slides further into the hit.
        -- Suggested range: 3 (long carry) – 12 (short carry)
        AttackDecay = 6.0,

        -- DASH
        -- =====================================================================

        -- Speed of the dash impulse (world units per second).
        -- This is independent of Speed — tune it relative to how far you
        -- want the player to travel during DashDuration.
        -- Suggested range: 6.0 (short hop) – 15.0 (long burst)
        DashSpeed = 5.0,

        -- How long the dash lasts in seconds.
        -- Match this to your dash animation length.
        -- Suggested range: 0.15 (snappy) – 0.4 (long slide)
        DashDuration = 0.7,

        -- Seconds before a consumed dash use regenerates.
        -- Suggested range: 0.8 (aggressive) – 2.0 (punishing)
        DashCooldown = 2.5,

        -- Maximum number of consecutive dashes available.
        -- Each dash consumes one use. Uses regenerate one at a time,
        -- each taking DashCooldown seconds after the previous use was spent.
        -- Example: DashMaxUses=2, DashCooldown=1.2 → two quick dashes,
        -- then first use returns after 1.2s, second after another 1.2s.
        DashMaxUses = 2,

        -- Fraction of the current dash that must elapse before a new dash
        -- input is accepted and fires immediately (cutting the animation short).
        -- Progress = (DashDuration - dashTimer) / DashDuration, checked each frame.
        --   0.0 = next dash can start instantly (no wait)
        --   0.9 = next dash fires at 90% through the current dash
        --   1.0 = must wait for the full dash to finish (no early cancel)
        -- Inputs received before the window opens are held, not dropped, so
        -- pressing dash slightly early still chains at the exact threshold.
        -- Suggested range: 0.7 (snappy chains) – 0.95 (tight window)
        DashEarlyCancelRatio = 0.5,

        -- Speed multiplier applied to DashSpeed when dashing in the air.
        -- > 1.0 = air dash travels further than ground dash.
        AirDashSpeedMultiplier = 1.3,

        -- How quickly the player can steer the dash direction (degrees per second).
        -- Input rotates the dash direction toward the stick at this rate.
        -- 0 = no steering (locked direction). 180 = very responsive steering.
        -- Suggested range: 90 (tight) – 270 (loose)
        DashSteerSpeed = 135.0,

        -- Upward velocity added during an air dash (gives a slight lift).
        -- Set to 0 to make air dashes purely horizontal.
        AirDashLift = 1.5,

        -- JUMP
        -- =====================================================================

        -- Vertical impulse height. Engine interprets this as jump force.
        -- Suggested range: 0.8 (low hop) – 2.5 (high jump)
        JumpHeight = 1.2,

        -- Seconds the landing animation plays before movement resumes.
        -- Suggested range: 0.2 (snappy) – 0.8 (weighty)
        LandingDuration = 0.4,

        -- Fall distance (world units) above which the player rolls on landing
        -- instead of playing the soft land animation.
        -- Suggested range: 1.5 (rolls often) – 4.0 (only big drops)
        RollHeightThreshold = 2.5,

        -- How strongly the chain resists movement toward the endpoint when taut.
        --   0.0 = chain has no effect on movement direction
        --   1.0 = full resistance (cannot push through the chain at all)
        -- Suggested range: 0.5 – 0.95
        TensionScale = 0.85,

        -- Scales air control: applies to both the turn rate and the
        -- speed-build rate when jumping from standstill.
        --   0.0 = ballistic (no steering at all)
        --   0.3 = nudge (default — noticeable but not arcade)
        --   1.0 = full ground-level control
        -- Suggested range: 0.1 – 0.5
        AirControlMultiplier = 0.3,

        -- How quickly the player can steer their velocity direction while airborne
        -- (degrees per second). Mirrors DashSteerSpeed — input rotates the velocity
        -- vector toward the stick at this rate, but only if the input has a forward
        -- component (dot > 0) relative to the current velocity. Opposing input is
        -- ignored so the player arcs rather than reversing mid-air. Speed magnitude
        -- is always preserved, identical to how dash steering works.
        -- 0   = no steering (ballistic arc)

        -- COMBAT
        -- =====================================================================

        -- Fallback attack lunge speed (world units per second).
        -- The real per-attack value comes from ComboManager's lunge table.
        -- Only used if attack_performed carries no lunge data.
        AttackLungeSpeed = 4.0,

        -- Fallback attack lunge duration in seconds. Same fallback rule as above.
        AttackLungeDuration = 0.12,

        -- FEEL / TIMING
        -- =====================================================================

        -- Seconds the damage stun lasts before the player regains control.
        DamageStunDuration = 0.5,

        -- Seconds after a cinematic freeze event before movement fully locks.
        -- Gives the character time to settle into position before cutting.
        CinematicSettleTime = 0.8,

        -- Seconds between footstep SFX triggers while running.
        -- Match this to your footstep animation cycle length.
        -- Suggested range: 0.25 (fast run) – 0.5 (slow walk)
        footstepInterval = 0.30,

        -- AUDIO (populate with clip GUIDs in the editor)
        -- =====================================================================
        playerFootstepSFX = {},
        playerHurtSFX     = {},
        playerJumpSFX     = {},
        playerLandSFX     = {},
        playerDeadSFX     = {},
        playerDashSFX     = {},
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

        -- =====================================================================
        -- Movement lock from combat
        self._playerCanMove = true

        -- =====================================================================
        -- Persistent velocity (momentum-based movement)
        -- This is the single source of truth for how fast the player is moving.
        -- Acceleration / deceleration / direction-change rates operate on this
        -- each frame rather than writing speed directly to the CharacterController.
        -- On attack entry the velocity is NOT zeroed — it decays at AttackDecay
        -- so momentum carries naturally into the first frames of a combo.
        self._velX = 0
        self._velZ = 0

        -- =====================================================================
        -- Lunge state (per-attack values from attack_performed event)
        self._lungeTimer  = 0
        self._lungeDirX   = 0
        self._lungeDirZ   = 0
        self._lungeSpeed  = self.AttackLungeSpeed

        -- =====================================================================
        -- Knockback
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

            -- =====================================================================
            -- Attack lunge
            -- ComboManager publishes lunge = { speed, duration } per state.
            -- PlayerMovement executes the impulse; it does not decide the values.
            self._attackLungeSub = event_bus.subscribe("attack_performed", function(data)
                local cameraYaw = _G.CAMERA_YAW or self._cameraYaw or 180.0
                local yr   = math.rad(cameraYaw)
                local sinYaw = math.sin(yr)
                local cosYaw = math.cos(yr)

                -- Camera forward and right axes (world space)
                local fwdX = -sinYaw
                local fwdZ = -cosYaw
                local rgtX =  cosYaw
                local rgtZ = -sinYaw

                -- Default lunge direction is camera forward.
                -- If input is held, take only its sideways component relative to
                -- the camera and offset the lunge by it. This produces a sidestep
                -- lunge when strafing but never redirects the attack backwards.
                -- A pure backwards input is ignored — lunge stays camera forward.
                local dirX, dirZ = fwdX, fwdZ
                local interp = _G.InputInterpreter
                local axis   = interp and interp:GetMovementAxis()
                local rawX   = axis and -axis.x or 0
                local rawZ   = axis and  axis.y or 0
                if rawX ~= 0 or rawZ ~= 0 then
                    local wX = rawZ * (-sinYaw) - rawX * cosYaw
                    local wZ = rawZ * (-cosYaw) + rawX * sinYaw
                    local wLen = math.sqrt(wX * wX + wZ * wZ)
                    if wLen > 0.001 then
                        wX, wZ = wX / wLen, wZ / wLen
                        -- Forward component of input relative to camera
                        local fwdDot  = wX * fwdX + wZ * fwdZ
                        -- Sideways component only — never let a backward input pull the lunge back
                        local sideDot = wX * rgtX + wZ * rgtZ
                        if fwdDot >= 0 then
                            -- Input has a forward lean: blend input direction with camera forward
                            dirX = fwdX + wX
                            dirZ = fwdZ + wZ
                        else
                            -- Input is sideways or backward: offset by side component only
                            dirX = fwdX + rgtX * sideDot
                            dirZ = fwdZ + rgtZ * sideDot
                        end
                        local dLen = math.sqrt(dirX * dirX + dirZ * dirZ)
                        if dLen > 0.001 then dirX = dirX / dLen; dirZ = dirZ / dLen end
                    end
                end

                self._lungeDirX = dirX
                self._lungeDirZ = dirZ

                -- Use per-attack lunge data from ComboManager; fall back to inspector defaults.
                local lunge = data and data.lunge
                self._lungeTimer = (lunge and lunge.duration) or self.AttackLungeDuration or 0.12
                self._lungeSpeed = (lunge and lunge.speed)    or self.AttackLungeSpeed    or 3.0

                -- Snap rotation to face the lunge direction
                local targetW, targetX, targetY, targetZ = directionToQuaternion(dirX, dirZ)
                self._currentRotW = targetW
                self._currentRotX = targetX
                self._currentRotY = targetY
                self._currentRotZ = targetZ
                self._facingX     = dirX
                self._facingZ     = dirZ
                pcall(self.SetRotation, self, targetW, targetX, targetY, targetZ)
            end)

            -- =====================================================================
            -- Combat state → movement lock
            -- canMove from ComboManager tells us whether this state allows
            -- movement. Velocity (_velX/_velZ) is NOT zeroed here — it
            -- decays naturally at AttackDecay so momentum carries into hits.
            self._combatStateSub = event_bus.subscribe("combat_state_changed", function(data)
                if not data then return end
                self._playerCanMove = data.canMove ~= false
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

            -- =====================================================================
            -- Dash
            -- ComboManager signals that a dash should happen.
            -- DashDuration is owned entirely by this script — ComboManager
            -- has no opinion on how long the dash lasts.
            self._dashRequested = false
            self._dashPerformedSub = event_bus.subscribe("dash_performed", function()
                self._dashRequested = true
                print("[PlayerMovement] dash_performed received")
            end)

            -- =====================================================================
            -- Chain movement constraint
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
        self._dashDirX           = 0
        self._dashDirZ           = 0
        self._wasDashingInAir    = false
        _G.player_is_dashing     = false

        -- Dash charge system:
        -- _dashUses        : current available uses (starts full)
        -- _dashRegenTimer  : counts down DashCooldown for the next regen
        --                    only ticks when _dashUses < DashMaxUses
        self._dashUses       = self.DashMaxUses
        self._dashRegenTimer = 0

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
        self._playerCanMove  = true
        self._velX, self._velZ = 0, 0

        -- Reset dash charges on respawn
        self._dashUses       = self.DashMaxUses
        self._dashRegenTimer = 0

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

        -- =====================================================================
        -- Respawn
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

        -- =====================================================================
        -- Cinematic freeze settle timer
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

        -- =====================================================================
        -- Knockback
        if self._kbPending then
            self._kbPending = false
            CharacterController.Move(self._controller, self._kbX or 0, 0, self._kbZ or 0)
            self._kbX, self._kbZ = 0, 0
        end

        -- =====================================================================
        -- Damage stun timer
        if self._isDamageStun then
            self._damageStunDuration = self._damageStunDuration - dt
            if self._damageStunDuration <= 0 then
                self._damageStunDuration = self.DamageStunDuration
                self._isDamageStun       = false
            end
        end

        -- =====================================================================
        -- Landing recovery timer
        if self._isLanding then
            self._landingDuration = self._landingDuration - dt
            if self._landingDuration <= 0 then
                self._landingDuration = self.LandingDuration
                self._isLanding       = false
                self._isRolling       = false
                self._animator:SetBool("IsRolling", false)
            end
        end

        -- During damage stun: keep position in sync, skip all other logic.
        -- Ground-only: if airborne, fall through so movement stays active.
        local isGroundedStun = CharacterController.IsGrounded(self._controller)
        if self._isDamageStun and isGroundedStun then
            self._animator:SetBool("IsGrounded", isGroundedStun)
            self._animator:SetBool("IsRunning", self._isRunning)
            local position = CharacterController.GetPosition(self._controller)
            if position then
                self:SetPosition(position.x, position.y, position.z)
                if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
            end
            return
        end

        -- =====================================================================
        -- Dash charge regen
        -- Only ticks when uses are depleted. Regenerates one use per
        -- DashCooldown duration, then immediately starts timing the next.
        if self._dashUses < self.DashMaxUses then
            self._dashRegenTimer = self._dashRegenTimer - dt
            if self._dashRegenTimer <= 0 then
                self._dashUses = self._dashUses + 1
                if self._dashUses < self.DashMaxUses then
                    -- Still missing uses — start timing the next regen immediately
                    self._dashRegenTimer = self.DashCooldown
                else
                    self._dashRegenTimer = 0
                end
            end
        end

        -- =====================================================================
        -- Attack lunge impulse
        -- Speed and duration are set by the attack_performed subscriber.
        -- This block just applies the cached values each frame.
        if self._lungeTimer and self._lungeTimer > 0 then
            self._lungeTimer = self._lungeTimer - dt
            CharacterController.Move(self._controller,
                self._lungeDirX * self._lungeSpeed,
                0,
                self._lungeDirZ * self._lungeSpeed)
        end

        -- =====================================================================
        -- Skill cast lock (ground-only)
        -- Airborne: fall through so movement stays active during aerial skills.
        local isGroundedSkill = CharacterController.IsGrounded(self._controller)
        if _G.player_is_casting_skill and isGroundedSkill then
            self._animator:SetBool("IsRunning", false)
            self._isRunning = false
            local position = CharacterController.GetPosition(self._controller)
            if position then
                self:SetPosition(position.x, position.y, position.z)
                if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
            end
            return
        end

        -- =====================================================================
        -- Combat movement lock
        -- While attacking (and this state doesn't allow movement), bleed the
        -- persistent velocity using AttackDecay — much slower than Deceleration
        -- so the player visibly carries momentum into the first hit, then stops.
        --
        -- self._playerCanMove is kept in sync via the combat_state_changed
        -- subscriber (set by ComboManager before the event fires, so no lag).
        -- Using the local copy rather than _G.player_can_move directly makes
        -- the data flow explicit and removes a hidden global dependency.
        -- IMPORTANT: The lock is ground-only. If the player becomes airborne
        -- mid-combo (explicit jump or walking off a ledge), we bypass the lock
        -- entirely so air control remains fully responsive. Without this, the
        -- early return below blocks ALL movement for the entire airtime whenever
        -- an attack was active at the moment of becoming airborne.
        local isGroundedForLock = CharacterController.IsGrounded(self._controller)
        if _G.player_is_attacking and not self._playerCanMove and isGroundedForLock then
            local decay = 1.0 - math.min(self.AttackDecay * dt, 1.0)
            self._velX = self._velX * decay
            self._velZ = self._velZ * decay

            -- Only push the decayed velocity if no lunge is active this frame.
            -- The lunge already called CharacterController.Move above; a second
            -- call here would overwrite it and kill the impulse entirely.
            if not (self._lungeTimer and self._lungeTimer > 0) then
                local velMag = math.sqrt(self._velX * self._velX + self._velZ * self._velZ)
                if velMag > 0.01 then
                    CharacterController.Move(self._controller, self._velX, 0, self._velZ)
                else
                    self._velX, self._velZ = 0, 0
                end
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

        -- =====================================================================
        -- Chain movement constraint
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
                    tensionScale   = math.max(0.0, 1.0 - self._chainConstraintRatio^3 * (self.TensionScale or 0.85))
                end
            end
        end

        -- =====================================================================
        -- Read movement axis from InputInterpreter
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

        -- =====================================================================
        -- Camera-relative movement
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

        -- Track peak Y while airborne so fall distance = peakY - landY
        if not isGrounded then
            local airPos = CharacterController.GetPosition(self._controller)
            if airPos and airPos.y > (self._peakAirY or airPos.y) then
                self._peakAirY = airPos.y
            end
        end


        -- =====================================================================
        -- Death pending
        if self._playerDeadPending and isGrounded then
            if self._animator then
                self._animator:SetBool("IsDead", true)
                dbg("[PlayerMovement] Player grounded, playing Death animation")
            end
            self._playerDead        = true
            self._playerDeadPending = false
            return
        end

        -- =====================================================================
        -- Dash execution
        -- ComboManager decides a dash happens; this block executes the physics.
        --
        -- Early-cancel: while a dash is active a new input is held (not dropped).
        -- It fires the moment progress >= DashEarlyCancelRatio, resetting the
        -- timer and consuming a use. Inputs before the window stay buffered so
        -- pressing dash a frame early still chains at the exact threshold.
        local earlyCancel = self._isDashing and
            (self.DashDuration - self._dashTimer) / self.DashDuration
                >= (self.DashEarlyCancelRatio or 0.9)

        if (not self._isDashing or earlyCancel)
            and self._dashRequested
            and self._dashUses > 0
            and not self._isDamageStun
            and not self._isLanding
            and not self._freezePending
        then
            self._dashRequested  = false
            self._isDashing      = true
            self._dashTimer      = self.DashDuration
            _G.player_is_dashing = true
            self._wasDashingInAir = not isGrounded

            if earlyCancel then
                print("[PlayerMovement] Dash early-cancelled and re-triggered")
            else
                print("[PlayerMovement] Dash started (duration=" .. tostring(self._dashTimer) .. ")")
            end

            -- Consume one use and immediately start the regen countdown.
            -- Always reset the timer so each use begins its own cooldown right away,
            -- regardless of whether a previous regen was already in progress.
            self._dashUses       = self._dashUses - 1
            self._dashRegenTimer = self.DashCooldown

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
            -- Only set IsDashing bool on a fresh dash — the animator state machine
            -- needs it to transition in from idle/run. On an early-cancel the dash
            -- state is already active, so only the trigger is needed to restart it.
            if not earlyCancel then
                self._animator:SetBool("IsDashing", true)
            end
            self._animator:SetTrigger("Dash")

            playRandomSFX(self._audio, self.playerDashSFX)

        elseif self._dashRequested and self._isDashing then
            -- Inside the dash but before the early-cancel window — hold the
            -- request so it fires the moment the threshold is crossed.
            -- (do nothing: _dashRequested stays true)

        elseif self._dashRequested then
            -- All other blocking conditions (stun, landing, no uses) — discard.
            print("[PlayerMovement] Dash discarded (uses=" .. self._dashUses
                .. " stun=" .. tostring(self._isDamageStun)
                .. " landing=" .. tostring(self._isLanding) .. ")")
            self._dashRequested = false
        end

        if self._isDashing then
            self._dashTimer = self._dashTimer - dt

            if self._dashTimer <= 0 then
                -- Dash finished: carry dash velocity into normal movement so the
                -- momentum system can arc it naturally. TurnDecel will bleed the
                -- speed off in a smooth arc when the player steers the other way,
                -- rather than snapping from zero. Clamp to Speed so it doesn't
                -- overshoot normal top speed on exit.
                self._isDashing          = false
                _G.player_is_dashing     = false
                self._wasDashingInAir    = false
                self._animator:SetBool("IsDashing", false)
                self._velX = self._dashDirX * self.Speed
                self._velZ = self._dashDirZ * self.Speed

                if event_bus and event_bus.publish then
                    event_bus.publish("dash_ended", {
                        uses       = self._dashUses,
                        maxUses    = self.DashMaxUses,
                        regenTimer = self._dashRegenTimer,
                        cooldown   = self.DashCooldown,
                    })
                end
                print("[PlayerMovement] Dash ended (uses remaining=" .. self._dashUses .. ")")
            else
                -- Dash active: apply impulse and sync position.
                -- Player retains directional influence — input steers the dash
                -- direction at DashSteerSpeed (degrees/sec) but cannot reverse
                -- or brake. The dash momentum stays dominant; input only rotates
                -- the direction the dash is travelling.
                if isMoving then
                    local inputLen = math.sqrt(moveX * moveX + moveZ * moveZ)
                    local inputDirX = moveX / inputLen
                    local inputDirZ = moveZ / inputLen

                    -- Only steer if input has a forward component (dot > 0).
                    -- This prevents the player from braking by pushing backward.
                    local dot = inputDirX * self._dashDirX + inputDirZ * self._dashDirZ
                    if dot > 0 then
                        local maxRotRad = math.rad(self.DashSteerSpeed or 180.0) * dt
                        local t = math.min(maxRotRad, 1.0)
                        local newX = self._dashDirX + (inputDirX - self._dashDirX) * t
                        local newZ = self._dashDirZ + (inputDirZ - self._dashDirZ) * t
                        local len  = math.sqrt(newX * newX + newZ * newZ)
                        if len > 0.001 then
                            self._dashDirX = newX / len
                            self._dashDirZ = newZ / len
                        end
                    end
                end

                local speed = self.DashSpeed
                local liftY = 0
                if not isGrounded or self._wasDashingInAir then
                    speed = self.DashSpeed * self.AirDashSpeedMultiplier
                    liftY = self.AirDashLift
                end
                CharacterController.Move(self._controller,
                    self._dashDirX * speed, liftY, self._dashDirZ * speed)

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

                -- Dash still active: skip movement + animation this frame.
            end
        end

        if not self._isDashing then

        -- =====================================================================
        -- Jump
        local interp = _G.InputInterpreter
        if not self._isLanding and not self._freezePending
            and interp and interp:IsJumpJustPressed() and isGrounded
        then
            CharacterController.Jump(self._controller, self.JumpHeight)
            isJumping = true
            self._animator:SetBool("IsJumping", true)
            playRandomSFX(self._audio, self.playerJumpSFX)
            -- Reset peak height tracker at jump start
            local launchPos = CharacterController.GetPosition(self._controller)
            self._peakAirY = launchPos and launchPos.y or 0
        end

        -- =====================================================================
        -- Momentum-based velocity update
        if not isJumping then
            if isMoving then
                local targetX = moveX * self.Speed
                local targetZ = moveZ * self.Speed
                local dot = self._velX * targetX + self._velZ * targetZ

                if not isGrounded then
                    -- Air control: same acceleration/pivot as ground but scaled
                    -- down by AirControlMultiplier.
                    --   0.0 = ballistic (no steering)
                    --   1.0 = full ground-level control
                    local rate = (dot < 0) and self.TurnDecel or self.Acceleration
                    rate = rate * (self.AirControlMultiplier or 0.3)
                    local t = math.min(rate * dt, 1.0)
                    self._velX = self._velX + (targetX - self._velX) * t
                    self._velZ = self._velZ + (targetZ - self._velZ) * t
                else
                    -- Grounded: normal accelerate or pivot.
                    local rate = (dot < 0) and self.TurnDecel or self.Acceleration
                    local t = math.min(rate * dt, 1.0)
                    self._velX = self._velX + (targetX - self._velX) * t
                    self._velZ = self._velZ + (targetZ - self._velZ) * t
                end
            else
                if isGrounded then
                    -- Grounded: bleed to stop when stick is released.
                    local decay = 1.0 - math.min(self.Deceleration * dt, 1.0)
                    self._velX = self._velX * decay
                    self._velZ = self._velZ * decay
                    if math.sqrt(self._velX * self._velX + self._velZ * self._velZ) < 0.001 then
                        self._velX, self._velZ = 0, 0
                    end
                end
                -- In air with no input: preserve momentum entirely.
                -- C++ gravity handles the arc; Lua doesn't bleed XZ.
            end

            -- Apply chain tension: resist outward component, leave lateral/inward free
            local mx, mz = self._velX, self._velZ
            local tensionDot = mx * tensionRadialX + mz * tensionRadialZ
            if tensionDot > 0 then
                mx = mx - tensionRadialX * tensionDot * (1.0 - tensionScale)
                mz = mz - tensionRadialZ * tensionDot * (1.0 - tensionScale)
            end

            local velMag = math.sqrt(mx * mx + mz * mz)
            if velMag > 0.001 then
                CharacterController.Move(self._controller, mx, 0, mz)
            end
        end

        -- =====================================================================
        -- Animation
        -- isEffectivelyMoving covers two cases:
        --   • isMoving  : stick is held (normal run)
        --   • velMag    : velocity is still high from a dash-exit burst or
        --                 post-attack carry-over with no stick input.
        -- Without the velMag check the character would slide visibly but
        -- play no run animation during those coast-off periods.
        local coastVelMag = math.sqrt(self._velX * self._velX + self._velZ * self._velZ)
        local isEffectivelyMoving = isMoving or coastVelMag > 0.1

        if not isGrounded then
            if not self._isJumping then
                -- Player became airborne without pressing Jump (e.g. walked off a ledge).
                -- Previously only the internal flags were set; the animator was never told,
                -- causing the run clip to keep playing mid-air and the jump/fall bool to
                -- never flip. Now we mirror what the explicit jump-press path does.
                self._isJumping = true
                self._isRunning = false
                self._animator:SetBool("IsRunning", false)
                self._animator:SetBool("IsJumping", true)
                -- Reset peak height tracker on walk-off
                local walkOffPos = CharacterController.GetPosition(self._controller)
                self._peakAirY = walkOffPos and walkOffPos.y or 0
            end
        else
            if self._isJumping then
                self._isJumping = false
                self._animator:SetBool("IsJumping", false)
                self._isLanding = true
                playRandomSFX(self._audio, self.playerLandSFX)

                -- Measure fall distance from peak height to landing position
                local landPos  = CharacterController.GetPosition(self._controller)
                local landY    = landPos and landPos.y or 0
                local fallDist = (self._peakAirY or landY) - landY
                local hardLand = fallDist >= (self.RollHeightThreshold or 2.5)
                print(string.format("[Landing] peakY=%.2f landY=%.2f fallDist=%.2f threshold=%.2f roll=%s",
                    self._peakAirY or 0, landY, fallDist, self.RollHeightThreshold or 2.5, tostring(hardLand)))

                if hardLand then
                    -- Fell far enough: roll landing
                    self._isRolling = true
                    self._animator:SetBool("IsRolling", true)
                    self._animator:SetBool("IsRunning", false)
                    self._isRunning = false
                else
                    -- Soft land
                    self._isRolling = false
                    self._animator:SetBool("IsRolling", false)
                    if isEffectivelyMoving then
                        self._animator:SetBool("IsRunning", true)
                        self._isRunning = true
                    else
                        self._animator:SetBool("IsRunning", false)
                        self._isRunning = false
                    end
                end
            elseif isEffectivelyMoving and not self._isRunning then
                self._animator:SetBool("IsRunning", true)
                self._isRunning = true
            elseif not isMoving and self._isRunning then
                -- Only cut the run anim when velocity has actually bled off,
                -- not the instant the stick is released.
                local velMag = math.sqrt(self._velX * self._velX + self._velZ * self._velZ)
                if velMag < 0.05 then
                    self._animator:SetBool("IsRunning", false)
                    self._isRunning = false
                end
            end
        end

        -- =====================================================================
        -- Footsteps
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

        -- =====================================================================
        -- Rotation
        -- Rotate toward input direction while input is held.
        -- In air, rotation speed is scaled by AirControlMultiplier so the
        -- character doesn't snap to a new direction faster than it can move there.
        -- While coasting (no input but still moving), hold the last facing.
        if isMoving then
            local mag = math.sqrt(moveX * moveX + moveZ * moveZ)
            self._facingX = moveX / mag
            self._facingZ = moveZ / mag

            local targetW, targetX, targetY, targetZ = directionToQuaternion(moveX, moveZ)
            local rotRate = self.rotationSpeed
            if not isGrounded then
                rotRate = rotRate * (self.AirControlMultiplier or 0.1)
            end
            local t = math.min(rotRate * dt, 1.0)
            local newW, newX, newY, newZ = lerpQuaternion(
                self._currentRotW, self._currentRotX,
                self._currentRotY, self._currentRotZ,
                targetW, targetX, targetY, targetZ, t)

            self._currentRotW, self._currentRotX, self._currentRotY, self._currentRotZ =
                newW, newX, newY, newZ
            pcall(self.SetRotation, self, newW, newX, newY, newZ)
        end

        end -- if not self._isDashing

        -- =====================================================================
        -- Position sync
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
        self._velX                    = 0
        self._velZ                    = 0
        self._dashUses                = self.DashMaxUses
        self._dashRegenTimer          = 0
        self._chainConstraintRatio    = 0
        self._chainConstraintExceeded = false
        self._chainDrag               = false
    end,
}