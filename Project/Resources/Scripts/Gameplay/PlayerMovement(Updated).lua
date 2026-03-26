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
    - Respond to combat_state_changed: lock/unlock movement
    - Respond to attack_performed: apply per-attack lunge impulse
    - Respond to dash_performed: execute the dash
    - Respond to chain events: apply movement constraints
    - Drive movement-specific animator parameters (IsRunning, IsJumping, etc.)
    - Manage respawn, damage stun, landing, cinematic freeze
    - Apply squash & stretch scale effects on movement events

NOT RESPONSIBLE FOR:
    - Reading raw engine input (owned by InputInterpreter)
    - Deciding which attack/combo to execute (owned by ComboManager)
    - Publishing chain or combat events (owned by ComboManager)

MOVEMENT LOCK RULES:
    _G.player_is_attacking && !_G.player_can_move → full movement lock
    _G.player_is_attacking &&  _G.player_can_move → movement allowed
    On attack entry velocity is NOT zeroed — decays at AttackDecay so
    momentum carries naturally into the first frames of a combo.

EVENTS CONSUMED:
    camera_yaw                          → camera-relative movement yaw
    playerDead                          → trigger death state
    playerHurtTriggered                 → apply damage stun
    player_knockback                    → apply knockback impulse
    respawnPlayer                       → respawn to checkpoint or origin
    activatedCheckpoint                 → remember last checkpoint entity
    freeze_player                       → cinematic freeze with settle timer
    request_player_forward              → reply with current facing direction
    attack_performed                    → execute per-attack lunge impulse
    force_player_rotation_to_camera     → snap rotation to camera forward
    force_player_rotation_to_direction  → snap rotation to arbitrary direction
    combat_state_changed                → update movement lock
    dash_performed                      → begin dash
    chain.movement_constraint           → apply chain length / drag constraint

EVENTS PUBLISHED:
    dash_executed                       → confirms dash actually ran (i-frame gate in PlayerHealth)
    dash_ended                          → dash finished; carries uses/regen state
    player_forward_response             → reply to request_player_forward
    player_position                     → world position each frame
    playerRespawned                     → respawn complete; carries spawn position
    vault_jump                          → confirmed vault jump executed

-- TO ADD new events: subscribe in Awake under the appropriate section header,
-- unsubscribe in OnDisable, and document the event name + payload above.

AUTHOR: Soh Wei Jie
VERSION: 3.0
================================================================================
--]]

require("extension.engine_bootstrap")
_G.CHAIN_DEBUG = _G.CHAIN_DEBUG ~= nil and _G.CHAIN_DEBUG or false
local function dbg(...) if _G.CHAIN_DEBUG then print(...) end end
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local AudioHelper    = require("extension.audio_helper")

local event_bus = _G.event_bus

-- Animation clip indices
local IDLE = 0
local RUN  = 1
local JUMP = 2

-- Chain tension direction (updated from chain.movement_constraint each frame)
local tensionRadialX = 0
local tensionRadialZ = 0
local tensionScale   = 1.0

-- ============================================================================
-- Quaternion helpers
-- ============================================================================

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

-- ============================================================================

return Component {
    mixins = { TransformMixin },

    -- ==========================================================================
    -- INSPECTOR FIELDS
    -- ==========================================================================
    -- Grouped by system. Add new tunables in the relevant section.
    -- Don't scatter magic numbers through Update — put them here.
    -- TO ADD a new field: add it in the right section and initialise any
    -- runtime state that depends on it in Start.
    -- ==========================================================================
    fields = {

        -- === Ground movement ===
        Speed        = 4.0,   -- Top running speed (world units/sec).
        Acceleration = 22.0,  -- Rate velocity climbs to Speed when input held. High = snappy.
        Deceleration = 16.0,  -- Rate velocity falls to zero when input released. High = firm stop.
        TurnDecel    = 32.0,  -- Extra decel when input opposes velocity (180-turn pivot rate).
        AttackDecay  = 6.0,   -- Velocity bleed during combo lock. Low = more momentum carry into hits.
        -- TO ADD new ground feel: add field here.

        -- === Jump & air control ===
        JumpHeight           = 1.2,   -- Vertical impulse magnitude.
        AirControlMultiplier = 0.3,   -- Scales both turn rate and accel in air. 0=ballistic, 1=full control.

        -- === Vault jump ===
        -- Two forward rays at VaultMinHeight and VaultMaxHeight probe for vaultable obstacles.
        -- Object qualifies when the min ray hits and the max ray does not.
        VaultMinHeight       = 0.2,   -- Lower ray origin height above feet. Object must be at least this tall.
        VaultMaxHeight       = 2.0,   -- Upper ray origin height above feet. Object must be shorter than this.
        VaultDetectRange     = 2.5,   -- Forward ray length (world units). How far ahead to probe.
        VaultMinApproachDist = 0.4,   -- Min distance to obstacle for vault to register. Prevents trigger while standing right at the base.
        VaultJumpHeight      = 2.0,   -- Fixed jump height used for all vault jumps. Tune this in the editor.
        VaultEnemyRadius     = 8.0,   -- Max distance an enemy can be for vaulting to be allowed (world units).

        -- === Dash ===
        DashSpeed              = 5.0,   -- Dash impulse speed — independent of Speed.
        DashDuration           = 0.7,   -- Seconds dash lasts. Match to dash animation.
        DashCooldown           = 2.5,   -- Seconds before a consumed use regenerates.
        DashMaxUses            = 2,     -- Max consecutive dashes. Uses regenerate one at a time.
        DashEarlyCancelRatio   = 0.5,   -- Fraction of dash elapsed before early-cancel fires (0=instant, 1=no cancel).
        AirDashSpeedMultiplier = 1.3,   -- Speed multiplier for air dashes.
        AirDashLift            = 1.5,   -- Upward velocity added during air dash. 0 = purely horizontal.
        DashSteerSpeed         = 135.0, -- Degrees/sec the player can steer mid-dash. Forward only — no braking.
        PostDashRecoveryTime   = 0.25,  -- Seconds after dash ends before full reverse steering is restored. 0 = no restriction.
        BackDashDotThreshold   = -0.5,  -- Dot product of input vs facing below which dash is classified as a back dash.
                                        -- -1.0 = exact opposite only.  0.0 = any sideways-or-back input qualifies.
        -- TO ADD dash variants (dodge roll, air dive): add fields here.

        -- === Combat / lunge ===
        -- Fallback values — real values come from ComboManager's lunge table.
        -- These only fire if attack_performed carries no lunge payload.
        AttackLungeSpeed    = 4.0,
        AttackLungeDuration = 0.12,
        ChainKickbackSpeed  = 2.5,   -- Kickback impulse on chain attack (opposite to throw direction). 0 = none.
        -- TO ADD per-attack feel: edit lunge table in ComboManager, not here.

        -- === Chain weapon ===
        TensionScale = 1.3,  -- Resistance to outward movement when chain is taut. 0=none, 1=full block.
        -- TO ADD chain swing / pull-toward modes: add fields here.

        -- === Aerial combat ===
        -- LiftAttackHeight  : jump height used exclusively for lift_attack launcher.
        -- AirLiftTargetVelY : Y velocity the player is restored TO when falling mid-combo.
        -- AirLiftCooldown   : seconds between lift applications.
        -- PostLiftAirLock   : seconds after lift_attack fires during which horizontal
        --                     air control is zeroed. Player is carried by jump arc.
        --                     Makes lift feel committed and heavy, not floaty+steerable.
        -- AirSlamInitialSpeed / Acceleration / MaxSpeed: slam descent curve.
        -- SlamShakeIntensity / Duration / Frequency: camera_shake payload on slam land.
        LiftAttackHeight    =  1.35,
        AirLiftTargetVelY   =  2.0,
        AirLiftCooldown     =  0.35,
        PostLiftAirLock     =  0.30,
        AirSlamInitialSpeed =  2.0,
        AirSlamAcceleration = 80.0,
        AirSlamMaxSpeed     = 50.0,
        SlamShakeIntensity  =  0.6,
        SlamShakeDuration   =  0.7,
        SlamShakeFrequency  = 25.0,
        LandingDuration     = 0.65,   -- Seconds of recovery before movement restores.
        RollHeightThreshold = 2.0,   -- Fall distance (world units) that triggers roll instead of soft land.
        SlamSlowMoScale     = 0.15,  -- Time scale during slam hit-stop (0=frozen, 1=normal).
        SlamSlowMoDuration  = 0.18,  -- Seconds the slow-mo lasts before snapping back.
        SlamHoverDuration   = 0.1,   -- Seconds player freezes in air before slam descent begins.
        -- TO ADD landing SFX variation or ledge-grab: add fields here.

        -- === Squash & stretch ===
        -- SquashStrength : how dramatic the effect is. Start here.
        --                  0.0 = disabled   0.3 = subtle   1.0 = cartoony
        --                  Scales all events — landing, dash, chain.
        -- SquashDuration : seconds to hit peak squash. Match to impact frame.
        -- StretchDuration: seconds to spring back to normal.
        SquashStrength  = 0.9,
        SquashDuration  = 0.15,
        StretchDuration = 0.22,

        -- === Feel / timing ===
        DamageStunDuration  = 0.5,    -- Seconds of stun after being hit.
        CinematicSettleTime = 0.8,    -- Seconds to settle before cinematic hard-freeze locks movement.
        footstepInterval    = 0.30,   -- Seconds between footstep SFX triggers while running.
        -- TO ADD new feel tuning: add field here.

        -- === Audio ===
        -- Populate with clip GUIDs in the editor. Each accepts a list for random selection.
        -- TO ADD new SFX: add a field here and call PlayRandomSFX in the relevant Update section.
        playerFootstepSFX = {},
        playerHurtSFX     = {},
        playerJumpSFX     = {},
        playerLandSFX     = {},
        playerDeadSFX     = {},
        playerDashSFX     = {},
    },

    -- ==========================================================================
    -- AWAKE
    -- Event subscriptions and field-independent initial state.
    -- Field-dependent state (e.g. LandingDuration) goes in Start, not here,
    -- because fields aren't populated until after Awake.
    -- ==========================================================================
    Awake = function(self)

        -- ── Rotation ──────────────────────────────────────────────────────────
        self._currentRotW = 1; self._currentRotX = 0
        self._currentRotY = 0; self._currentRotZ = 0
        self._facingX     = 0; self._facingZ     = 1

        -- ── Camera ────────────────────────────────────────────────────────────
        self._cameraYaw = 180.0

        -- ── Persistent velocity ───────────────────────────────────────────────
        -- Single source of truth for XZ movement. Never write to
        -- CharacterController.Move without going through _velX/_velZ.
        self._velX = 0
        self._velZ = 0

        -- ── Lunge ─────────────────────────────────────────────────────────────
        self._lungeTimer = 0; self._lungeDirX = 0
        self._lungeDirZ  = 0; self._lungeSpeed = 0

        -- ── Knockback ─────────────────────────────────────────────────────────
        self._kbPending = false; self._kbX = 0; self._kbZ = 0

        -- ── Chain constraint ──────────────────────────────────────────────────
        self._chainConstraintRatio    = 0
        self._chainConstraintExceeded = false
        self._chainDrag               = false
        self._chainDragTargetX        = 0
        self._chainDragTargetY        = 0
        self._chainDragTargetZ        = 0

        -- ── Combat lock ───────────────────────────────────────────────────────
        self._playerCanMove = true

        -- ── Cinematic freeze ──────────────────────────────────────────────────
        self._frozenBycinematic = false
        self._freezePending     = false
        self._freezeSettleTimer = 0.0

        if not (event_bus and event_bus.subscribe) then
            dbg("[PlayerMovement] ERROR: event_bus not available in Awake")
            return
        end

        -- Guard: unsubscribe any existing handle before (re-)subscribing.
        -- The engine may call Awake more than once; this makes every subscription
        -- idempotent regardless of call order or timing.
        local function sub(self, key, event, fn)
            if self[key] and event_bus.unsubscribe then
                event_bus.unsubscribe(self[key])
            end
            self[key] = event_bus.subscribe(event, fn)
        end

        sub(self, "_cameraYawSub", "camera_yaw", function(yaw)
            if yaw then self._cameraYaw = yaw end
        end)

        -- ── Player state ──────────────────────────────────────────────────────────────────────────
        sub(self, "_playerDeadSub", "playerDead", function(playerDead)
            if playerDead then
                self._playerDeadPending = playerDead
                AudioHelper.PlayRandomSFX(self._audio, self.playerDeadSFX)
            end
        end)

        sub(self, "_playerHurtTriggeredSub", "playerHurtTriggered", function(hit)
            if hit then
                self._isDamageStun = true
                if self._animator then self._animator:SetBool("IsJumping", false) end
                AudioHelper.PlayRandomSFX(self._audio, self.playerHurtSFX)
                -- Hit reaction: horizontal squash (pushed-sideways feel)
                self:_squashTrigger("horizontal", 0.5)
            end
        end)

        sub(self, "_knockSub", "player_knockback", function(p)
            if not p then return end
            self._kbX = (p.x or 0) * (p.strength or 0)
            self._kbZ = (p.z or 0) * (p.strength or 0)
            self._kbPending = true
        end)

        sub(self, "_respawnPlayerSub", "respawnPlayer", function(respawn)
            if respawn then self._respawnPlayer = true end
        end)

        sub(self, "_activatedCheckpointSub", "activatedCheckpoint", function(data)
            if data and data.child then self._activatedCheckpoint = data.child end
        end)
        sub(self, "_freezePlayerSub", "freeze_player", function(frozen)
            if frozen then
                self._freezePending     = true
                self._freezeSettleTimer = self.CinematicSettleTime or 0.8
            else
                self._frozenBycinematic = false
                self._freezePending     = false
            end
        end)

        -- ── Chain aim state ───────────────────────────────────────────────────
        self._chainAimActive = false
        self._chainAimStateSub = event_bus.subscribe("chain.aim_camera", function(payload)
            self._chainAimActive = payload and payload.active or false
        end)

        -- ── Rotation overrides ────────────────────────────────────────────────
        sub(self, "_requestPlayerForwardSub", "request_player_forward", function(_)
            if not self._facingX or not self._facingZ then return end
            event_bus.publish("player_forward_response", {
                x = self._facingX, y = 0, z = self._facingZ,
            })
        end)

        sub(self, "_forceRotSub", "force_player_rotation_to_camera", function()
            local yr   = math.rad(_G.CAMERA_YAW or self._cameraYaw or 180.0)
            local fwdX = -math.sin(yr)
            local fwdZ = -math.cos(yr)
            local len  = math.sqrt(fwdX*fwdX + fwdZ*fwdZ)
            if len > 0.001 then fwdX = fwdX/len; fwdZ = fwdZ/len end
            local w, x, y, z = directionToQuaternion(fwdX, fwdZ)
            self._currentRotW, self._currentRotX, self._currentRotY, self._currentRotZ = w, x, y, z
            self._facingX = fwdX; self._facingZ = fwdZ
            pcall(self.SetRotation, self, w, x, y, z)
        end)

        sub(self, "_chainFiredRotSub", "force_player_rotation_to_direction", function(payload)
            if not payload then return end
            local dx, dz = payload.x, payload.z
            if not dx or not dz then return end
            local len = math.sqrt(dx*dx + dz*dz)
            if len < 0.001 then return end
            dx, dz = dx/len, dz/len
            local w, x, y, z = directionToQuaternion(dx, dz)
            self._currentRotW, self._currentRotX, self._currentRotY, self._currentRotZ = w, x, y, z
            self._facingX = dx; self._facingZ = dz
            pcall(self.SetRotation, self, w, x, y, z)
        end)

        -- ── Combat: lunge + lock ──────────────────────────────────────────────
        -- attack_performed carries { state, lunge={speed,duration} }.
        -- Direction is computed from current input + camera yaw.
        -- Chain attack inverts the direction (kickback).
        sub(self, "_attackLungeSub", "attack_performed", function(data)
            local yr     = math.rad(_G.CAMERA_YAW or self._cameraYaw or 180.0)
            local sinYaw = math.sin(yr); local cosYaw = math.cos(yr)
            local fwdX   = -sinYaw;     local fwdZ   = -cosYaw
            local rgtX   =  cosYaw;     local rgtZ   = -sinYaw

            -- Default: camera forward. Offset by sideways input; backward input ignored.
            local dirX, dirZ = fwdX, fwdZ
            local interp = _G.InputInterpreter
            local axis   = interp and interp:GetMovementAxis()
            local rawX   = axis and -axis.x or 0
            local rawZ   = axis and  axis.y or 0
            if rawX ~= 0 or rawZ ~= 0 then
                local wX  = rawZ * (-sinYaw) - rawX * cosYaw
                local wZ  = rawZ * (-cosYaw) + rawX * sinYaw
                local wLen = math.sqrt(wX*wX + wZ*wZ)
                if wLen > 0.001 then
                    wX, wZ = wX/wLen, wZ/wLen
                    local fwdDot  = wX*fwdX + wZ*fwdZ
                    local sideDot = wX*rgtX + wZ*rgtZ
                    if fwdDot >= 0 then
                        dirX = fwdX + wX; dirZ = fwdZ + wZ
                    else
                        dirX = fwdX + rgtX*sideDot; dirZ = fwdZ + rgtZ*sideDot
                    end
                    local dLen = math.sqrt(dirX*dirX + dirZ*dirZ)
                    if dLen > 0.001 then dirX = dirX/dLen; dirZ = dirZ/dLen end
                end
            end

            if data and data.state == "chain_attack" then
                -- Chain kickback: reverse impulse + squash
                self._lungeDirX  = -dirX
                self._lungeDirZ  = -dirZ
                self._lungeSpeed = self.ChainKickbackSpeed or 2.5
                self:_squashTrigger("horizontal", 0.5)
            elseif data and data.state == "lift_attack" then
                -- Flag section 21 to use LiftAttackHeight this frame.
                -- Also zero horizontal input for PostLiftAirLock secs so the
                -- player is carried by the jump arc — feels committed, not floaty.
                if self._animator then self._animator:SetBool("IsLifting", true) end
                self._liftAttackJump  = true
                self._postLiftAirLock = self.PostLiftAirLock or 0.30
                self._lungeDirX = dirX
                self._lungeDirZ = dirZ
            elseif data and data.isSlam then
                self._isAirSlam   = true
                self._isJumping   = true
                self._slamHover   = true
                self._slamHoverTimer = self.SlamHoverDuration or 0.1
                self._slamVelY    = self.AirSlamInitialSpeed or 2.0
                self._velX        = 0
                self._velZ        = 0
                self._lungeTimer  = 0
                if self._animator then self._animator:SetBool("IsSlamming", true) end
            elseif data and data.isAerial then
                -- Air time extension: restore Y velocity to AirLiftTargetVelY only when
                -- (a) already airborne, (b) currently falling (curVelY < 0),
                -- (c) the lift cooldown has fully expired.
                -- This means "stop falling briefly" — not "relaunch".
                -- The cooldown prevents consecutive hits stacking velocity
                -- before the previous lift has had time to decay under gravity.
                if not CharacterController.IsGrounded(self._controller)
                    and (self._airLiftCooldownTimer or 0) <= 0
                then
                    local curVel  = CharacterController.GetVelocity(self._controller)
                    local curVelY = curVel and curVel.y or 0
                    if curVelY < 0 then
                        local vx = curVel and curVel.x or self._velX
                        local vz = curVel and curVel.z or self._velZ
                        CharacterController.SetVelocity(self._controller,
                            vx, self.AirLiftTargetVelY or 2.0, vz)
                        self._airLiftCooldownTimer = self.AirLiftCooldown or 0.35
                    end
                end
                self._lungeDirX = dirX
                self._lungeDirZ = dirZ
            else
                self._lungeDirX = dirX
                self._lungeDirZ = dirZ
            end

            local lunge = data and data.lunge
            self._lungeTimer = (lunge and lunge.duration) or self.AttackLungeDuration or 0.12
            self._lungeSpeed = (lunge and lunge.speed)    or self.AttackLungeSpeed    or 3.0

            -- Snap facing to lunge direction
            local w, x, y, z = directionToQuaternion(dirX, dirZ)
            self._currentRotW, self._currentRotX, self._currentRotY, self._currentRotZ = w, x, y, z
            self._facingX = dirX; self._facingZ = dirZ
            pcall(self.SetRotation, self, w, x, y, z)
        end)

        -- combat_state_changed: update movement lock. Velocity NOT zeroed here.
        -- When ComboManager leaves an attacking state, lunge is cancelled.
        sub(self, "_combatStateSub", "combat_state_changed", function(data)
            if not data then return end
            self._playerCanMove = data.canMove ~= false
            if not _G.player_is_attacking then
                self._lungeTimer = 0
                -- Do NOT cancel an active air slam here — the descent loop owns
                -- its own termination once IsGrounded fires. Only clear if already
                -- on the ground (e.g. combo cancelled before leaving the floor).
                if not self._isAirSlam or CharacterController.IsGrounded(self._controller) then
                    self._isAirSlam = false
                    self._slamVelY  = 0
                    if self._animator then self._animator:SetBool("IsSlamming", false) end
                end
            end
        end)

        -- ── Dash ──────────────────────────────────────────────────────────────
        -- ComboManager signals intent. All dash physics is owned here.
        self._dashRequested    = false
        sub(self, "_dashPerformedSub", "dash_performed", function()
            self._dashRequested = true
        end)

        -- ── Chain movement constraint ─────────────────────────────────────────
        -- Payload: { ratio, exceeded, drag, endX/Y/Z, targetX/Y/Z }
        sub(self, "_chainConstraintSub", "chain.movement_constraint", function(payload)
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

        -- ── Enemy proximity (vault gate) ──────────────────────────────────────
        self._nearbyEnemyPositions = {}
        sub(self, "_enemyPosSub", "enemy_position", function(payload)
            if not payload or not payload.entityId then return end
            self._nearbyEnemyPositions[payload.entityId] = {
                x = payload.x, y = payload.y, z = payload.z
            }
        end)

        -- TO ADD new subscriptions: use sub(self, "_handleKey", "event.name", fn).
        -- sub() automatically unsubscribes the previous handle if Awake re-fires.
    end,

    -- ==========================================================================
    -- START
    -- Field-dependent runtime state. Fields are not populated during Awake.
    -- ==========================================================================
    Start = function(self)
        self._collider  = self:GetComponent("ColliderComponent")
        self._animator  = self:GetComponent("AnimationComponent")
        self._transform = self:GetComponent("Transform")
        self._audio     = self:GetComponent("AudioComponent")
        self._rigidbody = self:GetComponent("RigidBodyComponent")

        dbg("transform y: ", self._transform.localPosition.y)
        self._controller = CharacterController.Create(self.entityId, self._collider, self._transform)

        -- ── Normal scale (squash & stretch baseline) ──────────────────────────
        -- Captured once from the transform set in the editor so that squash and
        -- stretch always springs back to the authored scale, not a hardcoded 1,1,1.
        local _initScale   = self._transform and self._transform.localScale
        self._normalScaleX = (_initScale and _initScale.x) or 1.0
        self._normalScaleY = (_initScale and _initScale.y) or 1.0
        self._normalScaleZ = (_initScale and _initScale.z) or 1.0

        --if self._animator then self._animator:PlayClip(IDLE, true) end

        self.rotationSpeed = 10.0

        -- ── State flags ───────────────────────────────────────────────────────
        self._isRunning         = false
        self._isJumping         = false
        self._isLanding         = false
        self._isRolling         = false
        self._rollDirX          = 0
        self._rollDirZ          = 0
        self._isDamageStun      = false
        self._playerDead        = false
        self._playerDeadPending = false

        -- ── Timers ────────────────────────────────────────────────────────────
        self._damageStunDuration = self.DamageStunDuration
        self._landingDuration    = self.LandingDuration
        self._footstepTimer      = 0
        self._wasRunning         = false

        -- ── Dash ──────────────────────────────────────────────────────────────
        self._isDashing          = false
        self._dashTimer          = 0
        self._postDashTimer      = 0
        self._dashDirX           = 0
        self._dashDirZ           = 0
        self._wasDashingInAir    = false
        self._isBackDash         = false   -- true while a back-dash is active; drives IsBackDashing animator bool
        -- _dashUses: available uses (starts full). Resets on respawn.
        -- _dashRegenTimer: counts down DashCooldown between regen ticks.
        self._dashUses           = self.DashMaxUses
        self._dashRegenTimer     = 0
        _G.player_is_dashing     = false

        -- ── Squash & stretch ──────────────────────────────────────────────────
        -- _squashPhase: "squash" → peak → "stretch" → springs back → nil
        -- Call _squashTrigger(mode, intensity) to start an effect.
        self._squashPhase     = nil
        self._squashTimer     = 0
        self._squashIntensity = 1.0
        self._squashMode      = "vertical"

        -- ── Aerial combat ─────────────────────────────────────────────────────
        -- Air lift is one-shot: SetVelocity is called once per hit, CC gravity
        -- accumulates naturally, no per-frame tracking needed.
        -- Slam is per-frame: _slamVelY accelerates each frame via SetVelocity.
        -- _slamLanding is a one-frame flag for full-intensity landing squash.
        self._liftAttackJump       = false
        self._isAirSlam            = false
        self._slamHover            = false
        self._slamHoverTimer       = 0
        self._slamVelY             = 0
        self._slamLanding          = false
        self._lastGroundedY        = 0
        self._airLiftCooldownTimer = 0
        self._postLiftAirLock      = 0   -- zero XZ control for PostLiftAirLock secs after lift_attack fires

        -- ── Air tracking ──────────────────────────────────────────────────────
        -- Records highest Y this airborne period for fall distance calculation.
        self._peakAirY = 0

        -- ── Vault jump ────────────────────────────────────────────────────────
        -- _vaultDetected: true when this frame's ray probe found a vaultable obstacle.
        -- Cleared after jump consumes it, or when player stops approaching.
        self._vaultDetected        = false
        self._vaultJumpHeight      = 0
        self._vaultJumpActive      = false   -- true while airborne from a vault jump
        self._vaultLaunchY         = 0
        self._vaultPeakY           = 0
        self._vaultAirControlBlend = 0       -- 1.0 = full control, 0.0 = normal AirControlMultiplier
        self._vaultAscentLock      = false   -- suppresses landing detection while player is still rising
        self._vaultReadyTimer      = 0       -- holds _vaultDetected alive briefly for forgiving timing
        self._prevAirY             = 0       -- previous frame Y; used to detect peak and start of descent

        -- ── Spawn position ────────────────────────────────────────────────────
        local pos = self._transform.worldPosition
        self._initialSpawnPoint = { x = pos.x, y = pos.y, z = pos.z }
    end,

    -- ==========================================================================
    -- SQUASH TRIGGER HELPER
    -- Starts a squash/stretch effect. Call from any trigger site.
    --   mode      : "vertical" | "horizontal" | "dashstart"
    --   intensity : 0.0–1.0, scales effect magnitude
    -- TO ADD a new mode: add an elseif in the squash update block in Update.
    -- ==========================================================================
    _squashTrigger = function(self, mode, intensity)
        self._squashPhase     = "squash"
        self._squashTimer     = 0
        self._squashMode      = mode or "vertical"
        self._squashIntensity = intensity or 1.0
    end,

    -- ==========================================================================
    -- RESPAWN
    -- ==========================================================================
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
        self._justRespawnedPlayer = true
        self._playerCanMove       = true
        self._velX                = 0
        self._velZ                = 0
        self._dashUses            = self.DashMaxUses
        self._dashRegenTimer      = 0
        self._isBackDash          = false
        self._chainConstraintRatio    = 0
        self._chainConstraintExceeded = false

        self._animator:SetBool("IsDead", false)

        if event_bus and event_bus.publish then
            event_bus.publish("playerRespawned", respawnPos)
        end
    end,

    -- ==========================================================================
    -- UPDATE
    -- Sections run in priority order. Higher-priority early-returns prevent
    -- lower sections from running. Keep that ordering when adding new sections.
    --
    -- SECTION ORDER:
    --   1. Global flags
    --   2. Respawn
    --   3. Guard checks (dead / no components)
    --   4. Cinematic freeze
    --   5. Knockback
    --   6. Damage stun timer
    --   7. Landing recovery timer
    --   8. Damage stun early-return
    --   9. Dash charge regen
    --   10. Attack lunge impulse
    --   11. Skill cast lock
    --   12. Interactable lock
    --   13. Combat movement lock
    --   14. Chain constraint
    --   15. Input
    --   16. Squash & stretch
    --   17. Peak air height tracking
    --   18. Death
    --   19. Dash
    --   20. Jump
    --   21. Velocity
    --   22. Animation
    --   23. Footsteps
    --   24. Rotation
    --   25. Position sync
    -- ==========================================================================
    Update = function(self, dt)

        -- ── [KEYBOARD INPUT NUMBER TEST] ──────────────────────────────────────
        -- Raw Keyboard bindings are PC only.
        -- local oneHeld = Keyboard.IsDigitPressed(1)
        -- if oneHeld and not self._digitWasHeld then
        --     print("[KeyboardTest] 1 pressed")
        --     for i = 0, 9 do
        --         print("[KeyboardTest] Digit " .. i .. " held: ", Keyboard.IsDigitPressed(i))
        --     end
        --     print("[KeyboardTest] Num1 alias held:   ", Keyboard.IsKeyPressed(Keyboard.Key.Num1))
        --     print("[KeyboardTest] One alias held:    ", Keyboard.IsKeyPressed(Keyboard.Key.One))
        --     print("[KeyboardTest] Digit1 alias held: ", Keyboard.IsKeyPressed(Keyboard.Key.Digit1))
        --     print("[KeyboardTest] Space held:        ", Keyboard.IsKeyPressed(Keyboard.Key.Space))
        --     print("[KeyboardTest] Left mouse held:   ", Keyboard.IsMouseButtonPressed(Keyboard.Mouse.Left))
        --     print("[KeyboardTest] Mouse pos:         ", Keyboard.GetMouseX(), Keyboard.GetMouseY())
        -- elseif not oneHeld and self._digitWasHeld then
        --     print("[KeyboardTest] 1 released")
        -- end
        -- self._digitWasHeld = oneHeld
        -- ── [END KEYBOARD INPUT NUMBER TEST] ──────────────────────────────────

        if Keyboard.IsDigitPressed(5) then
            self:RespawnPlayer()
        end

        -- ── 1. Global flags ───────────────────────────────────────────────────
        -- Written first so every other system sees current values this frame.
        _G.player_is_jumping = self._isJumping    or false
        _G.player_is_rolling = self._isRolling    or false
        _G.player_is_landing = self._isLanding    or false
        _G.player_is_hurt    = self._isDamageStun or false
        _G.player_is_dead    = self._playerDead   or false
        _G.player_is_frozen  = self._frozenBycinematic or self._freezePending or false

        -- ── 2. Respawn ────────────────────────────────────────────────────────
        if self._respawnPlayer then self:RespawnPlayer(); return end

        if self._justRespawnedPlayer then
            self._animator:Stop(self.entityId)
            self._animator:Play(self.entityId)
            self._justRespawnedPlayer = false
        end

        -- ── 3. Guard checks ───────────────────────────────────────────────────
        if not self._collider or not self._transform or not self._controller or self._playerDead then
            return
        end

        -- ── 4. Cinematic freeze ───────────────────────────────────────────────
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

        -- ── 5. Knockback ──────────────────────────────────────────────────────
        -- Single-frame impulse; cleared immediately after applying.
        if self._kbPending then
            self._kbPending = false
            CharacterController.Move(self._controller, self._kbX or 0, 0, self._kbZ or 0)
            self._kbX, self._kbZ = 0, 0
        end

        -- ── 6. Damage stun timer ──────────────────────────────────────────────
        if self._isDamageStun then
            self._damageStunDuration = self._damageStunDuration - dt
            if self._damageStunDuration <= 0 then
                self._damageStunDuration = self.DamageStunDuration
                self._isDamageStun       = false
            end
        end

        -- ── 7. Landing recovery timer ─────────────────────────────────────────
        if self._isLanding then
            self._landingDuration = self._landingDuration - dt
            if self._landingDuration <= 0 then
                self._landingDuration = self.LandingDuration
                self._isLanding       = false
                self._isRolling       = false
                self._animator:SetBool("IsRolling", false)
            end
        end

        -- ── 8. Damage stun early-return (grounded only) ───────────────────────
        -- If airborne during stun, fall through so movement stays active.
        local isGroundedStun = CharacterController.IsGrounded(self._controller)
        if self._isDamageStun and isGroundedStun then
            self._animator:SetBool("IsGrounded", isGroundedStun)
            self._animator:SetBool("IsRunning",  self._isRunning)
            local position = CharacterController.GetPosition(self._controller)
            if position then
                self:SetPosition(position.x, position.y, position.z)
                if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
            end
            return
        end

        -- ── 9. Dash charge regen ──────────────────────────────────────────────
        -- Only ticks when uses are depleted. Regenerates one use per DashCooldown.
        if self._dashUses < self.DashMaxUses then
            self._dashRegenTimer = self._dashRegenTimer - dt
            if self._dashRegenTimer <= 0 then
                self._dashUses       = self._dashUses + 1
                self._dashRegenTimer = (self._dashUses < self.DashMaxUses) and self.DashCooldown or 0
            end
        end

        -- ── 10. Attack lunge impulse + air lift cooldown ──────────────────────
        -- Lunge: direction/speed cached by attack_performed subscriber.
        -- Air lift cooldown: ticks down each frame regardless of ground state so
        -- it's always ready when the next aerial hit fires.
        if self._airLiftCooldownTimer > 0 then
            self._airLiftCooldownTimer = self._airLiftCooldownTimer - dt
        end
        if self._postLiftAirLock > 0 then
            self._postLiftAirLock = self._postLiftAirLock - dt
        end
        if self._lungeTimer and self._lungeTimer > 0 then
            if _G.player_is_attacking then
                self._lungeTimer = self._lungeTimer - dt
                CharacterController.Move(self._controller,
                    self._lungeDirX * self._lungeSpeed, 0, self._lungeDirZ * self._lungeSpeed)
            else
                -- Attack ended or was cancelled — discard remaining lunge immediately.
                self._lungeTimer = 0
            end
        end

        -- ── 11. Skill cast lock (grounded only) ───────────────────────────────
        if _G.player_is_casting_skill and CharacterController.IsGrounded(self._controller) then
            self._animator:SetBool("IsRunning", false)
            self._isRunning = false
            local position = CharacterController.GetPosition(self._controller)
            if position then
                self:SetPosition(position.x, position.y, position.z)
                if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
            end
            return
        end

        -- ── 12. Interactable lock ─────────────────────────────────────────────
        if _G.playerNearInteractable then return end

        -- ── 13. Combat movement lock (grounded only) ──────────────────────────
        -- Bypassed while airborne so air control stays responsive mid-combo.
        -- Velocity bleeds at AttackDecay (slow) so momentum carries into hits.
        local isGroundedForLock = CharacterController.IsGrounded(self._controller)
        if _G.player_is_attacking and not self._playerCanMove and isGroundedForLock then
            local decay = 1.0 - math.min(self.AttackDecay * dt, 1.0)
            self._velX = self._velX * decay
            self._velZ = self._velZ * decay

            -- Don't double-write if a lunge already called Move this frame.
            if not (self._lungeTimer and self._lungeTimer > 0) then
                local velMag = math.sqrt(self._velX*self._velX + self._velZ*self._velZ)
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

        -- ── 14. Chain movement constraint ─────────────────────────────────────
        -- Builds tensionRadialX/Z used later in section 21 (velocity).
        tensionRadialX = 0
        tensionRadialZ = 0
        tensionScale   = 1.0

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
                local radX   = pos.x - self._chainEndX
                local radZ   = pos.z - self._chainEndZ
                local radLen = math.sqrt(radX*radX + radZ*radZ)
                if radLen > 1e-4 then
                    tensionRadialX = radX / radLen
                    tensionRadialZ = radZ / radLen
                    tensionScale   = math.max(0.0,
                        1.0 - self._chainConstraintRatio^3 * (self.TensionScale or 0.85))
                end
            end
        end

        -- ── 15. Input ─────────────────────────────────────────────────────────
        -- Always read through InputInterpreter. Never touch _G.Input directly.
        local axis
        if self._freezePending then
            axis = { x = 0, y = 0 }
        else
            local interp = _G.InputInterpreter
            axis = (interp and interp:GetMovementAxis()) or { x = 0, y = 0 }
        end

        local rawX = -axis.x
        local rawZ =  axis.y

        -- Camera-relative movement: rotate input by camera yaw.
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

        local isMoving   = (moveX ~= 0 or moveZ ~= 0)
        local isGrounded = CharacterController.IsGrounded(self._controller)
        local isJumping  = false
        self._animator:SetBool("IsGrounded", isGrounded)

        -- ── 16. Squash & stretch ──────────────────────────────────────────────
        -- Runs each frame while _squashPhase is active.
        -- Modes:
        --   "vertical"  : landing — Y squashes down, XZ widens, Y springs past 1.0
        --   "horizontal": dash end / chain recoil — Y pops up, XZ squashes in
        --   "dashstart" : dash launch — Y stretches tall, XZ compresses briefly
        -- Stretch phase is a two-step spring: peak → overshoot → 1.0
        -- TO ADD a mode: add elseif branch here, call _squashTrigger at the event site.
        if self._squashPhase and self._transform then
            local scaleTable = self._transform.localScale
            if scaleTable then
                self._squashTimer = self._squashTimer + dt
                local i  = (self.SquashStrength or 0.5) * (self._squashIntensity or 1.0)
                local mode = self._squashMode or "vertical"
                -- Rest scale comes from what was set on the transform in Awake/editor,
                -- not a hardcoded 1,1,1, so the effect always springs back to the
                -- authored scale regardless of the object's real proportions.
                local nX = self._normalScaleX or 1.0
                local nY = self._normalScaleY or 1.0
                local nZ = self._normalScaleZ or 1.0
                local sx, sy, sz = nX, nY, nZ

                -- All amplitudes derived from SquashStrength (0-1) * per-event weight.
                -- Hardcoded ratios define the shape; only SquashStrength needs tuning.
                --   vertical  : Y squashes DOWN, XZ widens, Y bounces slightly above normal
                --   horizontal: Y pops UP, XZ squashes IN  (dash end / chain)
                --   dashstart : Y stretches TALL, XZ squashes IN  (dash launch)
                local yPeak, xzPeak, yOver
                if mode == "vertical" then
                    yPeak  = nY * (1.0 - 0.30 * i)   -- Y squashes down  (0.70 at full strength)
                    xzPeak = nX * (1.0 + 0.18 * i)   -- XZ widens out    (1.18 at full strength)
                    yOver  = nY * (1.0 + 0.08 * i)   -- Y bounces above  (1.08 at full strength)
                elseif mode == "horizontal" then
                    yPeak  = nY * (1.0 + 0.08 * i)   -- Y pops up        (1.08 at full strength)
                    xzPeak = nX * (1.0 - 0.10 * i)   -- XZ squashes in   (0.90 at full strength)
                    yOver  = nY
                elseif mode == "dashstart" then
                    yPeak  = nY * (1.0 + 0.10 * i)   -- Y stretches tall (1.10 at full strength)
                    xzPeak = nX * (1.0 - 0.07 * i)   -- XZ squashes in   (0.93 at full strength)
                    yOver  = nY
                end

                if self._squashPhase == "squash" then
                    -- Lerp from normal scale → (yPeak, xzPeak)
                    local t = math.min(self._squashTimer / math.max(self.SquashDuration or 0.07, 1e-4), 1.0)
                    sy = nY + (yPeak  - nY) * t
                    sx = nX + (xzPeak - nX) * t
                    sz = nZ + (xzPeak - nX) * t   -- Z tracks X (uniform XZ)
                    if t >= 1.0 then
                        self._squashPhase = "stretch"
                        self._squashTimer = 0
                    end

                elseif self._squashPhase == "stretch" then
                    -- Two-step spring:
                    --   First half  (t 0→0.5): peak → overshoot
                    --   Second half (t 0.5→1): overshoot → normal scale
                    -- XZ just lerps from xzPeak → normal scale across the full duration.
                    local t = math.min(self._squashTimer / math.max(self.StretchDuration or 0.22, 1e-4), 1.0)
                    if t < 0.5 then
                        local t2 = t * 2  -- 0→1 over first half
                        sy = yPeak  + (yOver - yPeak)  * t2
                        sx = xzPeak + (nX    - xzPeak) * t2
                    else
                        local t2 = (t - 0.5) * 2  -- 0→1 over second half
                        sy = yOver + (nY - yOver) * t2
                        sx = nX
                    end
                    sz = nZ + (sx - nX)   -- Z tracks X offset (uniform XZ)
                    if t >= 1.0 then
                        sx, sy, sz        = nX, nY, nZ
                        self._squashPhase = nil
                    end
                end

                pcall(function()
                    scaleTable.x = sx
                    scaleTable.y = sy
                    scaleTable.z = sz
                    self._transform.isDirty = true
                end)
            end
        end

        -- ── 17. Peak air height tracking + air height for slam threshold ──────────
        -- _lastGroundedY is updated every grounded frame so air height is always
        -- measured from the most recent takeoff point.
        -- _G.player_air_height is read by ComboManager to decide whether an airborne
        -- attack should auto-slam instead of starting the aerial combo.
        if isGrounded then
            -- Don't overwrite _lastGroundedY on the landing frame (while _isJumping
            -- is still true) — the fall distance calculation needs the takeoff Y.
            if not self._isJumping then
                local gPos = CharacterController.GetPosition(self._controller)
                if gPos then self._lastGroundedY = gPos.y end
            end
            _G.player_air_height = 0
        else
            local airPos = CharacterController.GetPosition(self._controller)
            if airPos then
                if airPos.y > (self._peakAirY or airPos.y) then
                    self._peakAirY = airPos.y
                end
                _G.player_air_height = math.max(0, airPos.y - (self._lastGroundedY or airPos.y))
            end
        end

        -- ── 18. Death ─────────────────────────────────────────────────────────
        if self._playerDeadPending and isGrounded then
            self._animator:SetBool("IsDead", true)
            self._playerDead        = true
            self._playerDeadPending = false
            return
        end

        -- ── 19. Roll movement ─────────────────────────────────────────────────
        -- While rolling, push the player forward in the stored roll direction.
        -- Input can steer forward only (same rules as dash steering — no braking).
        if self._isRolling and isGrounded then
            if isMoving then
                local inputLen  = math.sqrt(moveX*moveX + moveZ*moveZ)
                local inputDirX = moveX / inputLen
                local inputDirZ = moveZ / inputLen
                local dot = inputDirX * self._rollDirX + inputDirZ * self._rollDirZ
                if dot > 0 then
                    local maxRotRad = math.rad(self.DashSteerSpeed or 180.0) * dt
                    local t   = math.min(maxRotRad, 1.0)
                    local newX = self._rollDirX + (inputDirX - self._rollDirX) * t
                    local newZ = self._rollDirZ + (inputDirZ - self._rollDirZ) * t
                    local len  = math.sqrt(newX*newX + newZ*newZ)
                    if len > 0.001 then
                        self._rollDirX = newX / len
                        self._rollDirZ = newZ / len
                    end
                end
            end
            CharacterController.Move(self._controller,
                self._rollDirX * self.Speed, 0, self._rollDirZ * self.Speed)
            local position = CharacterController.GetPosition(self._controller)
            if position then
                self:SetPosition(position.x, position.y, position.z)
                if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
            end
            return  -- Skip normal movement while rolling.
        end

        -- ── 20. Dash ──────────────────────────────────────────────────────────
        -- ComboManager fires dash_performed. All dash physics lives here.
        --
        -- Early-cancel: an input during an active dash is held (not dropped) and
        -- fires the moment progress >= DashEarlyCancelRatio.
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
            -- Start dash
            self._dashRequested   = false
            self._isDashing       = true
            self._dashTimer       = self.DashDuration
            _G.player_is_dashing  = true
            self._wasDashingInAir = not isGrounded
            self:_squashTrigger("dashstart", 0.7)

            self._dashUses       = self._dashUses - 1
            self._dashRegenTimer = self.DashCooldown

            -- Confirm the dash actually ran so PlayerHealth can open the i-frame window.
            -- dash_performed may be discarded (no uses, stun, landing) — dash_executed
            -- only fires here, after all guard checks pass.
            if event_bus and event_bus.publish then
                event_bus.publish("dash_executed", {})
            end

            -- Direction: prefer current input, fall back to current facing.
            if isMoving then
                local len = math.sqrt(moveX*moveX + moveZ*moveZ)
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
                    self._dashDirX = 0; self._dashDirZ = 1
                end
            end

            -- Classify as back dash when input opposes facing past the threshold.
            -- No-input dashes always go forward so they are never back dashes.
            local facingDot  = self._dashDirX * self._facingX + self._dashDirZ * self._facingZ
            self._isBackDash = isMoving and facingDot < (self.BackDashDotThreshold or -0.5)

            self._animator:SetBool("IsJumping",    false); self._isJumping = false
            self._animator:SetBool("IsRunning",    false); self._isRunning  = false
            self._animator:SetBool("IsBackDashing", self._isBackDash)
            if self._isBackDash then
                -- TO DO: assign the back dash animation clip in the Animator.
                -- IsBackDashing is set but no clip exists yet — back dash plays no animation.
                print("[PlayerMovement] WARN: IsBackDashing=true — back dash animation clip not yet assigned in Animator")
            end
            if not earlyCancel then self._animator:SetBool("IsDashing", true) end
            self._animator:SetTrigger("Dash")
            AudioHelper.PlayRandomSFX(self._audio, self.playerDashSFX)

        elseif self._dashRequested and self._isDashing then
            -- Inside dash, before early-cancel window. Hold the request.

        elseif self._dashRequested then
            -- Blocked (stun / landing / no uses). Discard.
            print(string.format("[PlayerMovement] Dash discarded (uses=%d stun=%s landing=%s)",
                self._dashUses, tostring(self._isDamageStun), tostring(self._isLanding)))
            self._dashRequested = false
        end

        if self._isDashing then
            self._dashTimer = self._dashTimer - dt

            if self._dashTimer <= 0 then
                -- Dash ended: carry speed into normal movement for a natural momentum arc.
                self._isDashing       = false
                _G.player_is_dashing  = false
                self._wasDashingInAir = false
                self._isBackDash      = false
                self._animator:SetBool("IsDashing",     false)
                self._animator:SetBool("IsBackDashing", false)
                self._velX          = self._dashDirX * self.Speed
                self._velZ          = self._dashDirZ * self.Speed
                self._postDashTimer = self.PostDashRecoveryTime or 0
                self:_squashTrigger("horizontal", 0.6)

                if event_bus and event_bus.publish then
                    event_bus.publish("dash_ended", {
                        uses       = self._dashUses,
                        maxUses    = self.DashMaxUses,
                        regenTimer = self._dashRegenTimer,
                        cooldown   = self.DashCooldown,
                    })
                end

            else
                -- Dash active: steer direction, apply impulse, sync position.
                -- Input steers forward only — pushing back is ignored to prevent braking.
                -- Back dash skips steer entirely; there is no forward to steer toward.
                if isMoving and not self._isBackDash then
                    local inputLen  = math.sqrt(moveX*moveX + moveZ*moveZ)
                    local inputDirX = moveX / inputLen
                    local inputDirZ = moveZ / inputLen
                    local dot = inputDirX * self._dashDirX + inputDirZ * self._dashDirZ
                    if dot > 0 then
                        local maxRotRad = math.rad(self.DashSteerSpeed or 180.0) * dt
                        local t   = math.min(maxRotRad, 1.0)
                        local newX = self._dashDirX + (inputDirX - self._dashDirX) * t
                        local newZ = self._dashDirZ + (inputDirZ - self._dashDirZ) * t
                        local len  = math.sqrt(newX*newX + newZ*newZ)
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

                -- Back dash: player faces forward while sliding backward — skip rotation.
                if self.SetRotation and not self._isBackDash then
                    local w, x, y, z = directionToQuaternion(self._dashDirX, self._dashDirZ)
                    self._currentRotW, self._currentRotX, self._currentRotY, self._currentRotZ = w, x, y, z
                    pcall(self.SetRotation, self, w, x, y, z)
                end

                local position = CharacterController.GetPosition(self._controller)
                if position then
                    self:SetPosition(position.x, position.y, position.z)
                    if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
                end

                return  -- Skip movement + animation while dash is active.
            end
        end

        -- ── 20. Vault detection ───────────────────────────────────────────────
        -- Vaulting is only allowed when at least one enemy is within VaultEnemyRadius.
        -- This prevents the mechanic from accidentally triggering during normal traversal
        -- while keeping it fully available the moment a fight is nearby.
        -- enemy_position events (published every frame by each live EnemyAI) keep
        -- _nearbyEnemyPositions current; dead/despawned enemies stop publishing and
        -- naturally fall out of range on the next proximity check.

        -- Tick the ready timer — keeps _vaultDetected alive even if the ray misses this frame
        if self._vaultReadyTimer > 0 then
            self._vaultReadyTimer = self._vaultReadyTimer - dt
            if self._vaultReadyTimer <= 0 then
                self._vaultDetected = false
            end
        else
            self._vaultDetected = false
        end

        -- Check whether any known enemy is within VaultEnemyRadius.
        local enemyNearby = false
        local vaultRadiusSq = (self.VaultEnemyRadius or 8.0) ^ 2
        local playerPos = CharacterController.GetPosition(self._controller)
        if playerPos then
            for _, epos in pairs(self._nearbyEnemyPositions) do
                local ex = epos.x - playerPos.x
                local ez = epos.z - playerPos.z
                if ex*ex + ez*ez <= vaultRadiusSq then
                    enemyNearby = true
                    break
                end
            end
        end

        if not enemyNearby then
            -- No enemies in range — discard any stale detection and skip probing.
            self._vaultDetected   = false
            self._vaultReadyTimer = 0
        elseif isGrounded and isMoving and not self._isDamageStun then
            -- Low threshold so detection fires early as the player approaches.
            -- 0.15 * Speed means almost any forward-facing movement qualifies.
            local approachDot = self._velX * self._facingX + self._velZ * self._facingZ
            local approachThreshold = (self.Speed or 4.0) * 0.15
            if approachDot > approachThreshold then
                local vpos = CharacterController.GetPosition(self._controller)
                if vpos and Physics and Physics.Raycast then
                    local fx, fz  = self._facingX, self._facingZ
                    local range   = self.VaultDetectRange     or 2.5
                    local minH    = self.VaultMinHeight        or 0.2
                    local maxH    = self.VaultMaxHeight        or 2.0
                    local minDist = self.VaultMinApproachDist  or 0.4

                    -- Min ray: obstacle must reach at least this height
                    local minOk, minHitDist = pcall(function()
                        return Physics.Raycast(vpos.x, vpos.y + minH, vpos.z, fx, 0, fz, range)
                    end)
                    local minHit = minOk and minHitDist and minHitDist > 0

                    if minHit and minHitDist > minDist then
                        -- Max ray: obstacle must NOT reach this height (too tall to vault)
                        local maxOk, maxHitDist = pcall(function()
                            return Physics.Raycast(vpos.x, vpos.y + maxH, vpos.z, fx, 0, fz, range)
                        end)
                        local maxHit = maxOk and maxHitDist and maxHitDist > 0

                        if not maxHit then
                            self._vaultDetected   = true
                            self._vaultJumpHeight = self.VaultJumpHeight or 2.0
                            self._vaultReadyTimer = 0.25   -- hold detection alive for forgiving jump timing
                        end
                    end
                end
            end
        end

        -- ── 21. Jump ──────────────────────────────────────────────────────────
        local interp = _G.InputInterpreter

        -- Consume lift attack flag before the jump check so it wins the height selection.
        -- _liftAttackJump is set by attack_performed when the state is lift_attack.
        -- It fires on the same frame as IsJumpJustPressed, so we treat them as one event.
        local isLiftAttack = self._liftAttackJump
        self._liftAttackJump = false

        if not self._isLanding and not self._freezePending and not self._slamBuffering
            and (isLiftAttack or (interp and interp:IsJumpJustPressed())) and isGrounded
        then
            local jumpH
            if isLiftAttack then
                jumpH = self.LiftAttackHeight or 6.0
            elseif self._vaultDetected then
                jumpH = self._vaultJumpHeight
            else
                jumpH = self.JumpHeight
            end
            CharacterController.Jump(self._controller, jumpH)
            isJumping = true
            self._animator:SetBool("IsJumping", true)
            --put here for now, don't remove comment block
            --if not isLiftAttack then
            --    PlayRandomSFX(self._audio, self.playerJumpSFX)
            --end
            AudioHelper.PlayRandomSFX(self._audio, self.playerJumpSFX)
            local launchPos = CharacterController.GetPosition(self._controller)
            self._peakAirY  = launchPos and launchPos.y or 0

            if self._vaultDetected and not isLiftAttack then
                -- Arm vault air control: starts full, tapers to normal as player reaches peak.
                self._vaultJumpActive      = true
                self._vaultLaunchY         = self._peakAirY
                self._vaultPeakY           = self._peakAirY + jumpH
                self._vaultAirControlBlend = 1.0
                self._vaultDetected        = false
                self._vaultReadyTimer      = 0
                self._vaultAscentLock      = true   -- block landing detection until player starts descending
                self._prevAirY             = self._peakAirY
                if event_bus and event_bus.publish then
                    event_bus.publish("vault_jump", {
                        height  = jumpH,
                        launchY = self._vaultLaunchY,
                        peakY   = self._vaultPeakY,
                    })
                end
            end
            -- TO ADD double-jump: track jump count here, gate on _jumpCount, call Jump again.
        end

        -- ── 21. Velocity ──────────────────────────────────────────────────────
        -- Slam descent takes full priority. Uses SetVelocity (not Move) because
        -- Move's Y is discarded in air by the CC — only XZ from Move is used airborne.
        -- SetVelocity writes directly to the physics body, overriding CC gravity
        -- accumulation and giving full manual control of the descent curve.
        if self._isAirSlam then
            -- Hover: freeze in air briefly before descent begins.
            if self._slamHover then
                self._slamHoverTimer = self._slamHoverTimer - dt
                CharacterController.SetVelocity(self._controller, 0, 0, 0)
                if self._slamHoverTimer <= 0 then
                    self._slamHover = false
                end
                local hoverPos = CharacterController.GetPosition(self._controller)
                if hoverPos then
                    self:SetPosition(hoverPos.x, hoverPos.y, hoverPos.z)
                    if event_bus and event_bus.publish then event_bus.publish("player_position", hoverPos) end
                end
                return
            end

            local slamPos = CharacterController.GetPosition(self._controller)
            -- Raycast straight down from feet. Ray length covers max distance
            -- the player can travel in one frame at full slam speed (~50/60 = 0.84)
            -- plus a small skin buffer so we never tunnel through thin floors.
            -- Ray covers exactly one frame of travel plus a small skin buffer.
            -- No fixed minimum — that was causing early hits while still airborne.
            local rayLen  = (self._slamVelY * dt) + 0.15
            local slamHit = false
            local slamHitDist = 0
            if slamPos and Physics and Physics.Raycast then
                local ok, dist = pcall(function()
                    return Physics.Raycast(slamPos.x, slamPos.y + 0.05, slamPos.z, 0, -1, 0, rayLen)
                end)
                slamHitDist = (ok and dist) or 0
                slamHit = ok and dist and dist > 0
            end
            print(string.format("[SLAM] pos.y=%.3f | rayLen=%.3f | slamHit=%s | hitDist=%.3f | _slamVelY=%.2f",
                slamPos and slamPos.y or -999, rayLen, tostring(slamHit), slamHitDist, self._slamVelY))

            if not slamHit then
                self._slamVelY = math.min(
                    self._slamVelY + (self.AirSlamAcceleration or 80.0) * dt,
                    self.AirSlamMaxSpeed or 50.0
                )
                CharacterController.SetVelocity(self._controller, 0, -self._slamVelY, 0)
            else
                -- Hit floor — fire everything immediately and return clean.
                print(string.format("[SLAM] HIT FLOOR at pos.y=%.3f | firing effects", slamPos and slamPos.y or -999))
                self._isAirSlam  = false
                self._isJumping  = false
                self._isLanding  = true
                self._slamVelY   = 0
                self._velX       = 0
                self._velZ       = 0
                CharacterController.SetVelocity(self._controller, 0, 0, 0)
                if self._animator then
                    self._animator:SetBool("IsJumping",  false)
                    self._animator:SetBool("IsSlamming", false)
                    self._animator:SetTrigger("SlamLanded")
                end
                self:_squashTrigger("vertical", 1.0)
                if event_bus and event_bus.publish then
                    event_bus.publish("slam_landed", {})
                    event_bus.publish("camera_shake", {
                        intensity = self.SlamShakeIntensity or 0.6,
                        duration  = self.SlamShakeDuration  or 0.45,
                        frequency = self.SlamShakeFrequency or 18.0,
                    })
                    event_bus.publish("fx_chromatic", {
                        intensity = 0.8,
                        duration  = 0.3,
                    })
                    event_bus.publish("fx_time_scale", {
                        scale    = self.SlamSlowMoScale    or 0.15,
                        duration = self.SlamSlowMoDuration or 0.18,
                    })
                end
                local position = CharacterController.GetPosition(self._controller)
                if position then
                    self:SetPosition(position.x, position.y, position.z)
                    if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
                end
                return  -- Next frame runs with _isLanding=true and clean state.
            end
        elseif not isJumping then
            if isMoving then
                local targetX = moveX * self.Speed
                local targetZ = moveZ * self.Speed
                local dot = self._velX * targetX + self._velZ * targetZ

                if not isGrounded then
                    -- Vault jump: blend from full control at launch down to normal at peak.
                    -- Progress tracks how far through the ascent the player is.
                    if self._vaultJumpActive then
                        local airPos   = CharacterController.GetPosition(self._controller)
                        local curY     = airPos and airPos.y or self._vaultLaunchY
                        local span     = math.max(0.01, self._vaultPeakY - self._vaultLaunchY)
                        local progress = math.min(1.0, (curY - self._vaultLaunchY) / span)
                        self._vaultAirControlBlend = math.max(0.0, 1.0 - progress)
                    end

                    local baseAir    = self.AirControlMultiplier or 0.3
                    local airControl = self._vaultJumpActive
                        and (baseAir + (1.0 - baseAir) * self._vaultAirControlBlend)
                        or  baseAir

                    -- Lift attack air lock: zero horizontal control so the player
                    -- is carried by the jump arc for PostLiftAirLock seconds.
                    -- Feels heavy and committed — not steerable mid-launch.
                    if self._postLiftAirLock > 0 then
                        airControl = 0
                    end

                    local rate = ((dot < 0) and self.TurnDecel or self.Acceleration) * airControl
                    local t = math.min(rate * dt, 1.0)
                    self._velX = self._velX + (targetX - self._velX) * t
                    self._velZ = self._velZ + (targetZ - self._velZ) * t

                else
                    -- Grounded.
                    -- Post-dash: restrict reverse steering. Dot against dashDir
                    -- (not velocity, which may have already drifted).
                    if self._postDashTimer > 0 then
                        self._postDashTimer = self._postDashTimer - dt
                        local recovery = 1.0 - math.max(0,
                            self._postDashTimer / (self.PostDashRecoveryTime or 0.25))
                        local dashInputDot = moveX * self._dashDirX + moveZ * self._dashDirZ
                        local rate = dashInputDot < 0
                            and self.TurnDecel * (0.1 + 0.9 * recovery)
                            or  self.Acceleration
                        local t = math.min(rate * dt, 1.0)
                        self._velX = self._velX + (targetX - self._velX) * t
                        self._velZ = self._velZ + (targetZ - self._velZ) * t
                    else
                        local rate = (dot < 0) and self.TurnDecel or self.Acceleration
                        local t = math.min(rate * dt, 1.0)
                        self._velX = self._velX + (targetX - self._velX) * t
                        self._velZ = self._velZ + (targetZ - self._velZ) * t
                    end
                end

            else
                if isGrounded then
                    -- No input grounded: bleed to stop.
                    local decay = 1.0 - math.min(self.Deceleration * dt, 1.0)
                    self._velX  = self._velX * decay
                    self._velZ  = self._velZ * decay
                    if math.sqrt(self._velX*self._velX + self._velZ*self._velZ) < 0.001 then
                        self._velX, self._velZ = 0, 0
                    end
                end
                -- No input in air: preserve full momentum. C++ handles gravity arc.
            end

            -- Chain tension: resist outward movement only. Lateral/inward is free.
            local mx, mz     = self._velX, self._velZ
            local tensionDot = mx * tensionRadialX + mz * tensionRadialZ
            if tensionDot > 0 then
                mx = mx - tensionRadialX * tensionDot * (1.0 - tensionScale)
                mz = mz - tensionRadialZ * tensionDot * (1.0 - tensionScale)
            end

            local velMag = math.sqrt(mx*mx + mz*mz)
            if velMag > 0.001 then
                CharacterController.Move(self._controller, mx, 0, mz)
            end
        end

        -- ── 22. Animation ─────────────────────────────────────────────────────
        -- isEffectivelyMoving covers coast-off after stick release (velocity still
        -- high from dash exit or attack carry-over with no input).
        local coastVelMag         = math.sqrt(self._velX*self._velX + self._velZ*self._velZ)
        local isEffectivelyMoving = isMoving or coastVelMag > 0.1

        if not isGrounded then
            if not self._isJumping and not self._isLanding then
                -- Became airborne without jump press (walked off a ledge).
                self._isJumping = true
                self._isRunning = false
                self._animator:SetBool("IsRunning", false)
                self._animator:SetBool("IsJumping", true)
                local walkOffPos = CharacterController.GetPosition(self._controller)
                self._peakAirY   = walkOffPos and walkOffPos.y or 0
            end

            -- Track Y each airborne frame to detect when ascent has peaked.
            -- Once the player starts descending, release the ascent lock.
            if self._vaultAscentLock then
                local airPos = CharacterController.GetPosition(self._controller)
                local curY   = airPos and airPos.y or self._prevAirY
                if curY < self._prevAirY - 0.01 then
                    -- Y has dropped since last frame — past the peak, safe to land now
                    self._vaultAscentLock = false
                end
                self._prevAirY = curY
            end
        else
            if self._isJumping and not self._vaultAscentLock then
                -- Landed.
                self._isJumping             = false
                self._isLanding             = true
                self._vaultJumpActive       = false
                self._vaultAscentLock       = false
                self._airLiftCooldownTimer  = 0
                self._postLiftAirLock       = 0

                local landPos  = CharacterController.GetPosition(self._controller)
                local landY    = landPos and landPos.y or 0
                -- Use launch height (last grounded Y), not peak air Y, so a normal
                -- jump on flat ground gives fallDist ~0 instead of the full jump arc.
                local fallDist = (self._lastGroundedY or landY) - landY

                local intensity
                -- Landing intensity: small hop = 0.3, big drop = 1.0
                intensity = math.max(0.3, math.min(1.0, fallDist / 3.0))

                -- Set destination state BEFORE clearing IsJumping so the animator
                -- sees the correct target condition when IsJumping flips to false.
                self._animator:SetBool("IsLifting", false)
                if fallDist >= (self.RollHeightThreshold or 2.5) then
                    -- High fall -> roll
                    self._isRolling  = true
                    self._rollDirX   = self._facingX or 0
                    self._rollDirZ   = self._facingZ or 0
                    self._animator:SetBool("IsRolling", true)
                    self._animator:SetBool("IsRunning", false)
                    self._isRunning  = false
                elseif isEffectivelyMoving then
                    -- Coasting (no keys but still has velocity) -> run directly
                    self._isRolling = false
                    self._animator:SetBool("IsRolling", false)
                    self._animator:SetBool("IsRunning", true)
                    self._isRunning = true
                else
                    -- Stationary landing
                    self._isRolling = false
                    self._animator:SetBool("IsRolling", false)
                    self._animator:SetBool("IsRunning", false)
                    self._isRunning = false
                end
                self._animator:SetBool("IsJumping", false)

                self:_squashTrigger("vertical", intensity)
                AudioHelper.PlayRandomSFX(self._audio, self.playerLandSFX)

            elseif isEffectivelyMoving and not self._isRunning then
                self._animator:SetBool("IsRunning", true)
                self._isRunning = true

            elseif not isMoving and self._isRunning then
                -- Cut run anim only once velocity has actually bled off.
                if coastVelMag < 0.05 then
                    self._animator:SetBool("IsRunning", false)
                    self._isRunning = false
                end
            end
        end

        -- ── 23. Footsteps ─────────────────────────────────────────────────────
        if self._isRunning and isGrounded and not self._isLanding then
            if not self._wasRunning then
                AudioHelper.PlayRandomSFX(self._audio, self.playerFootstepSFX, 0.5)
                self._footstepTimer = 0
            end
            self._footstepTimer = self._footstepTimer + dt
            if self._footstepTimer >= (self.footstepInterval or 0.35) then
                AudioHelper.PlayRandomSFX(self._audio, self.playerFootstepSFX, 0.5)
                self._footstepTimer = 0
            end
        else
            self._footstepTimer = 0
        end
        self._wasRunning = self._isRunning

        -- ── 24. Rotation ──────────────────────────────────────────────────────
        -- Chain aim: always face the camera's look direction regardless of input.
        -- Normal: rotate toward input while input is held.
        if self._chainAimActive then
            local yr   = math.rad(_G.CAMERA_YAW or self._cameraYaw or 180.0)
            local fwdX = -math.sin(yr)
            local fwdZ = -math.cos(yr)
            local t = math.min((self.rotationSpeed or 10.0) * dt, 1.0)
            local targetW, targetX, targetY, targetZ = directionToQuaternion(fwdX, fwdZ)
            local newW, newX, newY, newZ = lerpQuaternion(
                self._currentRotW, self._currentRotX,
                self._currentRotY, self._currentRotZ,
                targetW, targetX, targetY, targetZ, t)
            self._currentRotW, self._currentRotX, self._currentRotY, self._currentRotZ =
                newW, newX, newY, newZ
            self._facingX = fwdX; self._facingZ = fwdZ
            pcall(self.SetRotation, self, newW, newX, newY, newZ)
        elseif isMoving then
            -- Rotate toward input while input is held.
            -- Air: scaled by AirControlMultiplier (rotation tracks movement capability).
            -- Post-dash: reverse rotation restricted for PostDashRecoveryTime.
            local mag = math.sqrt(moveX*moveX + moveZ*moveZ)
            self._facingX = moveX / mag
            self._facingZ = moveZ / mag

            local targetW, targetX, targetY, targetZ = directionToQuaternion(moveX, moveZ)
            local rotRate = self.rotationSpeed

            if not isGrounded then
                -- Vault jump: blend rotation rate from full speed at launch down to
                -- normal air rate at peak, matching the velocity air control blend.
                local baseAir = self.AirControlMultiplier or 0.1
                local airMult = self._vaultJumpActive
                    and (baseAir + (1.0 - baseAir) * self._vaultAirControlBlend)
                    or  baseAir
                rotRate = rotRate * airMult
            elseif self._postDashTimer > 0 then
                -- Only restrict if input opposes the dash direction.
                local dashDot = moveX * self._dashDirX + moveZ * self._dashDirZ
                if dashDot < 0 then
                    local recovery = 1.0 - math.max(0,
                        self._postDashTimer / (self.PostDashRecoveryTime or 0.25))
                    rotRate = rotRate * (0.1 + 0.9 * recovery)
                end
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

        -- ── 25. Position sync ─────────────────────────────────────────────────
        -- Always the final write each frame. Keeps transform in sync with
        -- the CharacterController's resolved position after physics.
        local position = CharacterController.GetPosition(self._controller)
        if position then
            self:SetPosition(position.x, position.y, position.z)
            if event_bus and event_bus.publish then event_bus.publish("player_position", position) end
        end
    end,

    -- ==========================================================================
    -- ON DISABLE
    -- ==========================================================================
    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe then
            -- Add new handle keys here whenever a subscription is added in Awake.
            local subs = {
                "_cameraYawSub", "_playerDeadSub", "_playerHurtTriggeredSub",
                "_knockSub", "_respawnPlayerSub", "_activatedCheckpointSub",
                "_freezePlayerSub", "_requestPlayerForwardSub", "_attackLungeSub",
                "_combatStateSub", "_forceRotSub", "_chainFiredRotSub",
                "_dashPerformedSub", "_chainConstraintSub", "_enemyPosSub",
                "_chainAimStateSub",
            }
            for _, key in ipairs(subs) do
                if self[key] then event_bus.unsubscribe(self[key]); self[key] = nil end
            end
        end

        -- Reset transient state so re-enable starts clean.
        self._frozenBycinematic       = false
        self._playerCanMove           = true
        self._velX                    = 0
        self._velZ                    = 0
        self._dashUses                = self.DashMaxUses
        self._dashRegenTimer          = 0
        self._isBackDash              = false
        self._chainConstraintRatio    = 0
        self._chainConstraintExceeded = false
        self._chainDrag               = false
        self._squashPhase             = nil
        self._vaultAscentLock         = false
        _G.player_is_dashing          = false
    end,
}