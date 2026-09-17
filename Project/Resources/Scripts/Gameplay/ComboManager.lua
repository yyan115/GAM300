--[[
================================================================================
COMBO MANAGER
================================================================================
PURPOSE:
    Reads processed input from InputInterpreter and decides what combat action
    to execute. Manages the combo state machine and publishes decisions for
    PlayerMovement and the chain weapon system to act on.

SINGLE RESPONSIBILITY: Decide combat actions. Nothing else.

RESPONSIBILITIES:
    - Read buffered input from InputInterpreter each frame
    - Run the combo state machine (transitions, windows, input queuing)
    - Publish combat decisions via event_bus
    - Publish chain weapon events (chain.down / chain.up / chain.hold) with
      full context awareness (throwable hooked, dashing, weapon equipped)
    - Set global combat flags (_G.player_is_attacking, _G.player_can_move)
    - Drive combat-specific animator parameters (ComboStep, IsAttacking, etc.)

NOT RESPONSIBLE FOR:
    - Reading raw engine input (owned by InputInterpreter)
    - Moving the CharacterController (owned by PlayerMovement)
    - Physics execution of any kind (owned by PlayerMovement)

EVENTS PUBLISHED:
    attack_performed     { state, damage, knockback, lunge, chargePercent? }
    dash_performed       {}          -- PlayerMovement owns DashDuration
    combat_state_changed { state, canMove, comboChain }
    chain.down           {}
    chain.up             {}
    chain.hold           {}

COMBO TREE NODE FIELDS:
    id          : Unique state identifier string
    animParam   : Integer → ComboStep animator parameter
    duration    : Animation length in seconds (999 = indefinite)
    damage      : Base damage value
    knockback   : Knockback impulse magnitude
    canMove     : Whether PlayerMovement may move during this state
    lunge       : { speed, duration } impulse injected into PlayerMovement on entry
    comboWindow : Seconds before end during which next input is accepted (nil = no chain)
    onEnter     : function(self, stateObj, data)  — called on state entry
    onUpdate    : function(self, stateObj, dt)    — overrides default logic if set
    onExit      : function(self, stateObj, data)  — called on state exit
    transitions : { inputType → nextStateId }

AUTHOR: Soh Wei Jie
VERSION: 4.0
================================================================================
--]]

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

-- ══════════════════════════════════════════════════════════════════════════
-- SWING TUNING (item 8)
-- ══════════════════════════════════════════════════════════════════════════
-- One table per variant so a swing can be reshaped without touching the clips
-- or the animator file, and so two variants can be compared by changing one
-- word. Per combo step:
--
--   start   normalized time the swing starts at, so a wind up can be skipped
--   finish  normalized time it ends at, so a recovery can be cut short
--   ramp    { {at, speed}, ... } in normalized time, held between points, so
--           one part of a swing can be fast and another normal
--
-- "off" is the shipped behaviour and is what every variant is measured against.
_G.SWING_TRACE = _G.SWING_TRACE or false

local SWING_VARIANTS = {
    off = {},

    -- A. the blunt version: the third hit simply plays faster
    flat_fast_3 = {
        light_3 = { ramp = { {0.0, 1.6} } },
    },

    -- B. his example on the first hit, fast on the way down and normal at the
    -- end, and the same shape on the third
    ramp_all = {
        light_1 = { ramp = { {0.0, 1.8}, {0.45, 1.0} } },
        light_2 = { ramp = { {0.0, 1.6}, {0.45, 1.0} } },
        light_3 = { ramp = { {0.0, 2.2}, {0.30, 1.0}, {0.70, 2.0} } },
    },

    -- C. cut the front of the third hit only
    cut_front_3 = {
        light_3 = { start = 0.22 },
    },

    -- D. cut the end of the third hit only
    cut_end_3 = {
        light_3 = { finish = 0.80 },
    },

    -- E. cut both, which is the horizontal swing he described
    horizontal_3 = {
        light_3 = { start = 0.22, finish = 0.78 },
    },

    -- E+B. the same cut with a ramp through what is left
    horizontal_3_ramped = {
        light_3 = { start = 0.22, finish = 0.78, ramp = { {0.0, 1.5}, {0.5, 1.0} } },
    },
}

return Component {
    fields = {
        DefaultComboWindow  = 0.5,
        HeavyChargeTime     = 0.8,
        MaxComboAnimSpeed   = 2.0,
        -- Which entry of SWING_VARIANTS above is live. "off" is what ships.
        SwingVariant        = "off",
        -- Minimum seconds between chain attacks. The tap-fire (ChainBootstrap)
        -- still fires every press; only the attack animation is gated.
        ChainAttackCooldown = 0.6,
        -- When false, all attack and combo inputs are suppressed and the combo
        -- state machine does not run. Chain weapon events and movement are
        -- unaffected. Toggle from any external system (cutscene, tutorial gate).
        AttacksEnabled      = true,
        -- Minimum height above last grounded Y before aerial attacks are allowed.
        -- Below this the player is too close to the ground for aerial combo to make sense.
        --
        -- 0.8 is deliberate and correct. The way into the aerial combo is
        -- lift_attack, which jumps at PlayerMovement's LiftAttackHeight of
        -- 1.35 and clears this easily. A standing jump peaks at 0.527 and is
        -- meant to be refused. I lowered this to 0.25 on the mistaken reading
        -- that the moveset was unreachable, having only ever tested a standing
        -- jump from idle.
        MinAerialAttackHeight = 0.8,
        -- Height above ground that auto-routes idle airborne attack to air_slam.
        SlamHeightThreshold   = 5.0,
        -- Minimum seconds between aerial attack state entries.
        -- Blocks mash without punishing timed inputs — window is shorter than any animation.
        AerialHitLockout      = 0.18,
        -- No SFX fields. Audio is owned by CombatAudio, which reacts to
        -- attack_performed events. ComboManager publishes; it does not play sounds.
    },

    Awake = function(self)
        -- ══════════════════════════════════════════════════════════════════
        -- COMBO TREE DEFINITION
        -- Each state owns its lunge data so PlayerMovement can execute it
        -- without hardcoding anything. Change a value here and every system
        -- that reacts to attack_performed automatically gets the new data.
        -- ══════════════════════════════════════════════════════════════════
        self.COMBO_TREE = {

            idle = {
                id           = "idle",
                animParam    = 0,
                clipDuration = nil,  -- no duration measurement needed for idle
                duration     = 0,
                damage      = 0,
                canMove     = true,
                comboWindow = nil,
                lunge       = nil,
                transitions = {
                    attack      = "light_1",
                    attack_hold = "heavy_charge",
                    chain       = "chain_attack",
                    dash        = "dash",
                },
            },

            -- ── Light combo ───────────────────────────────────────────────
            light_1 = {
                id           = "light_1",
                animParam    = 1,
                clipDuration = 1.4,
                duration     = 1.4,
                damage      = 10,
                knockback   = 1.5,
                canMove     = false,
                comboWindow = 0.25,
                lunge       = { speed = 3.0, duration = 0.10 },
                transitions = {
                    attack = "light_2",
                    chain  = "chain_attack",
                    jump   = "lift_attack",
                    dash   = "dash",
                },
            },

            light_2 = {
                id           = "light_2",
                animParam    = 2,
                clipDuration = 1.4,
                duration     = 1.4,
                damage      = 12,
                knockback   = 1.5,
                canMove     = false,
                comboWindow = 0.25,
                lunge       = { speed = 3.5, duration = 0.11 },
                transitions = {
                    attack = "light_3",
                    chain  = "chain_attack",
                    -- no jump transition: jump during light_2 is a plain cancel, not a lift
                    dash   = "dash",
                },
            },

            light_3 = {
                id           = "light_3",
                animParam    = 3,
                clipDuration = 1.2,
                duration     = 1.2,
                damage      = 20,
                knockback   = 4.0,
                canMove     = false,
                comboWindow = nil,
                lunge       = { speed = 5.0, duration = 0.18 },
                transitions = {
                    jump = "lift_attack",
                    dash = "dash",
                },
            },

            -- ── Heavy attack ──────────────────────────────────────────────
            heavy_charge = {
                id           = "heavy_charge",
                animParam    = 10,
                clipDuration = nil,  -- indefinite; timer-driven by HeavyChargeTime
                duration     = 999,  -- indefinite until release or full charge
                damage      = 0,
                canMove     = false,
                comboWindow = nil,
                lunge       = nil,

                onUpdate = function(self, state, dt)
                    local input = self._inputInterpreter

                    -- Dash cancel: exit charge immediately into dash
                    if input:HasBufferedDash() then
                        input:ConsumeBufferedDash()
                        self._queuedCombo = nil
                        --print("[ComboManager] DASH CANCEL: heavy_charge -> dash")
                        self:_transitionTo("dash")
                        return
                    end

                    -- Jump cancel: exit charge cleanly; PlayerMovement fires the jump this frame
                    if input:IsJumpJustPressed() and not _G.player_is_jumping then
                        self._queuedCombo = nil
                        --print("[ComboManager] JUMP CANCEL: heavy_charge -> idle")
                        self:_transitionTo("idle")
                        return
                    end

                    -- Auto-release at full charge
                    if state.timer >= self.HeavyChargeTime then
                        self:_transitionTo("heavy_release", { chargePercent = 1.0 })
                        return
                    end

                    -- Manual release
                    if input:IsAttackJustReleased() then
                        local pct = math.min(state.timer / self.HeavyChargeTime, 1.0)
                        self:_transitionTo("heavy_release", { chargePercent = pct })
                    end
                end,

                transitions = {},
            },

            heavy_release = {
                id           = "heavy_release",
                animParam    = 11,
                clipDuration = 1.2,
                duration     = 1.2,
                damage      = 30,
                knockback   = 3.0,
                canMove     = false,
                comboWindow = nil,
                lunge       = { speed = 6.0, duration = 0.22 },  -- heavy shove

                onEnter = function(self, state, data)
                    data = data or {}
                    local chargePercent = data.chargePercent or 0.5
                    state.actualDamage = state.damage * (1.0 + chargePercent)

                    if event_bus then
                        event_bus.publish("attack_performed", {
                            state         = state.id,
                            damage        = state.actualDamage,
                            knockback     = self.COMBO_TREE["heavy_release"].knockback or 0,
                            chargePercent = chargePercent,
                            lunge         = self.COMBO_TREE["heavy_release"].lunge,
                        })
                    end
                    --print("[ComboManager] Heavy released: " .. math.floor(chargePercent * 100) .. "% charge")
                end,

                transitions = {
                    jump = "lift_attack",
                    dash = "dash",
                },
            },

            -- ── Chain attack ──────────────────────────────────────────────
            chain_attack = {
                id           = "chain_attack",
                animParam    = 20,
                clipDuration = 0.5,  -- natural clip length in seconds at speed 1.0
                duration     = 0.5,
                damage      = 25,
                knockback   = 1.0,
                canMove     = true,    -- movement allowed; chain tap fires simultaneously via ChainBootstrap
                comboWindow = 0.5,
                lunge       = { speed = 2.0, duration = 0.08 },  -- subtle pull
                transitions = {
                    attack = "light_1",
                    jump   = "lift_attack",
                    dash   = "dash",
                },
            },

            -- ── Dash ──────────────────────────────────────────────────────
            -- ComboManager decides a dash should happen and publishes the
            -- event. PlayerMovement owns the physics of actually dashing.
            -- The duration is published so PlayerMovement has a single source
            -- of truth — it does not maintain its own separate dash duration.
            dash = {
                id           = "dash",
                animParam    = 30,
                clipDuration = nil,  -- dash animation timing owned by PlayerMovement
                duration     = 0,
                damage      = 0,
                canMove     = false,
                comboWindow = nil,
                lunge       = nil,

                onEnter = function(self, state, data)
                    -- Publish with no duration: PlayerMovement owns DashDuration.
                    -- ComboManager's job is to say "dash now", not how long it lasts.
                    if event_bus then
                        event_bus.publish("dash_performed", {})
                    end
                end,

                transitions = {},
            },

            -- ── Lift attack (ground→air launcher) ────────────────────────────────
            -- Fired by jump-just-pressed during any ground attack.
            -- canMove = true so PlayerMovement's jump check runs this frame.
            -- isAerial = true sets IsAirAttacking on the animator.
            lift_attack = {
                id           = "lift_attack",
                animParam    = 40,
                clipDuration = 0.8,
                duration     = 0.8,
                damage       = 15,
                knockback    = 4.0,
                canMove      = true,
                isAerial     = true,
                isLift       = true,
                comboWindow  = 0.25,
                lunge        = { speed = 3.0, duration = 0.12 },
                transitions  = {
                    attack = "air_light_1",
                },
            },

            -- ── Aerial combo ─────────────────────────────────────────────────────────────────
            -- All aerial states have canMove = true so air-steering stays active.
            air_light_1 = {
                id           = "air_light_1",
                animParam    = 41,
                clipDuration = 0.85,
                duration     = 0.85,
                damage       = 12,
                knockback    = 1.0,
                canMove      = true,
                isAerial     = true,
                comboWindow  = 0.2,
                lunge        = { speed = 2.5, duration = 0.09 },
                transitions  = {
                    attack = "air_light_2",
                    dash   = "dash",
                },
            },

            air_light_2 = {
                id           = "air_light_2",
                animParam    = 42,
                clipDuration = 0.85,
                duration     = 0.85,
                damage       = 14,
                knockback    = 1.0,
                canMove      = true,
                isAerial     = true,
                comboWindow  = 0.2,
                lunge        = { speed = 2.5, duration = 0.09 },
                transitions  = {
                    attack = "air_slam",
                    dash   = "dash",
                },
            },

            -- ── Air slam ─────────────────────────────────────────────────────────
            -- canMove = false + PlayerMovement zeroes XZ each frame via SetVelocity.
            -- Long duration = full commitment. No dash cancel during active slam.
            air_slam = {
                id           = "air_slam",
                animParam    = 44,
                clipDuration = 1.1,
                duration     = 1.1,
                damage       = 30,
                knockback    = 8.0,
                canMove      = false,
                isAerial     = true,
                isSlam       = true,
                comboWindow  = nil,
                lunge        = nil,
                transitions  = {},     -- no escape: slam is fully committed, no dash cancel
            },
        }

        -- ── Runtime state ─────────────────────────────────────────────────
        self._inputInterpreter = nil
        self._animator         = nil
        self._playerAudio      = nil

        self._currentStateId   = "idle"
        self._currentStateData = self.COMBO_TREE["idle"]
        self._stateTimer       = 0
        self._queuedCombo      = nil
        self._comboChain       = {}

        -- Chain weapon awareness: while a Throwable is hooked the chain input
        -- must NOT route into chain_attack — ChainBootstrap owns that decision.
        self._chainHasThrowable   = false
        self._chainHoldPublished  = false
        self._chainAttackCooldown = 0
        self._chainPressBlocked   = false
        self._lastAerialHitLanded = false
        self._aerialStringHit     = false
        self._aerialLockoutTimer  = 0   -- blocks aerial attack input when > 0
    end,

    Start = function(self)
        local playerEntityId = Engine.GetEntityByName("Player")
        if not playerEntityId then
            --print("[ComboManager] ERROR: Player entity not found!")
            return
        end
        self._playerEntityId = playerEntityId
        --print("[ComboManager] Player entity found (ID: " .. tostring(playerEntityId) .. ")")

        self._animator = Engine.FindAnimatorByName("Player")
        -- SetNormalizedTime moves bone transforms, so it needs the entity that
        -- owns them. The animator here is a raw AnimationComponent rather than
        -- the wrapper that injects it, so it has to be passed.
        self._animatorEntity = Engine.GetEntityByName("Player")
        if not self._animator then
            --print("[ComboManager] ERROR: Player AnimationComponent not found!")
            return
        end

        self._inputInterpreter = _G.InputInterpreter
        if not self._inputInterpreter then
            --print("[ComboManager] ERROR: InputInterpreter not found!")
            return
        end

        -- Note: no AudioComponent needed here. SFX is owned by CombatAudio,
        -- which subscribes to attack_performed and plays sounds independently.

        self._animator:SetInt("ComboStep", 0)
        self._animator:SetBool("IsAttacking", false)
        self._animator:SetBool("IsHeavyCharging", false)

        -- ── Chain weapon awareness subscriptions ──────────────────────────
        if _G.event_bus and _G.event_bus.subscribe then
            self._subHitEntity = _G.event_bus.subscribe("chain.endpoint_hit_entity", function(payload)
                if payload and payload.isThrowable then
                    self._chainHasThrowable = true
                    --print("[ComboManager] Throwable hooked - chain_attack blocked")
                end
            end)
            self._subRetracted = _G.event_bus.subscribe("chain.endpoint_retracted", function()
                if self._chainHasThrowable then
                    self._chainHasThrowable = false
                    --print("[ComboManager] Throwable released - chain_attack unblocked")
                end
            end)
            self._subThrowFired = _G.event_bus.subscribe("chain.throwable_throw", function()
                if self._chainHasThrowable then
                    self._chainHasThrowable = false
                    --print("[ComboManager] Throwable thrown - chain_attack unblocked")
                end
            end)
            self._chainExtendedSub = _G.event_bus.subscribe("chain.extended_changed", function(payload)
                self._chainIsExtended = payload and payload.isExtended or false
            end)
        end

        -- ── Attack enable/disable ─────────────────────────────────────────
        -- External systems (e.g. DoorTrigger during a cutscene/pickup sequence)
        -- publish set_attacks_enabled with a boolean to gate the state machine
        -- without touching raw input or global flags.
        if _G.event_bus and _G.event_bus.subscribe then
            self._attacksEnabledSub = _G.event_bus.subscribe("set_attacks_enabled", function(enabled)
                self.AttacksEnabled = (enabled ~= false)
                --print("[ComboManager] AttacksEnabled = " .. tostring(self.AttacksEnabled))
            end)
        end

        -- ── Aerial hit confirmation ───────────────────────────────────────
        -- AttackHitbox publishes "attack_hit_confirmed" each time the player
        -- hitbox connects with an enemy during an active attack frame.
        -- NOTE: match this event name to whatever AttackHitbox.lua publishes.
        -- air_light_2 reads _lastAerialHitLanded to decide its next state:
        --   true  → loop back to air_light_1 (hit confirms stay airborne)
        --   false → route to air_slam (whiff punish / committed dive)
        if _G.event_bus and _G.event_bus.subscribe then
            self._attackHitSub = _G.event_bus.subscribe("attack_hit_confirmed", function()
                if self._currentStateData and self._currentStateData.isAerial then
                    self._lastAerialHitLanded = true
                    -- Scoped to the whole aerial string, not to one state. See
                    -- the branch in air_light_2 for why the per-state flag
                    -- cannot be the one that decides it.
                    self._aerialStringHit = true
                end
            end)
        end

        if _G.event_bus and _G.event_bus.subscribe then
            self._slamLandedSub = _G.event_bus.subscribe("slam_landed", function()
                -- Slam has hit the ground — PlayerMovement owns the landing from here.
                -- Force ComboManager to idle immediately so player_is_attacking clears
                -- and section 13 in PlayerMovement stops blocking movement/animation.
                if self._currentStateId == "air_slam" then
                    self:_transitionTo("idle")
                end
            end)
        end

        --print("[ComboManager] Initialized successfully")
    end,

    Update = function(self, dt)
        if not self._inputInterpreter or not self._animator or Time.IsPaused() then return end

        local input = self._inputInterpreter

        -- ══════════════════════════════════════════════════════════════════
        -- CHAIN WEAPON EVENTS
        -- InputInterpreter knows that chain was pressed/released/held.
        -- ComboManager decides what that means in context and publishes
        -- the appropriate event for the chain weapon system to act on.
        -- ══════════════════════════════════════════════════════════════════
        if _G.playerHasWeapon and not _G.player_is_dashing and event_bus then
            if input:IsChainJustPressed() then
                -- Always allow if chain is already extended (retract path).
                -- Block only if retracted and cooldown is active (would start new extension).
                local chainIsOut = self._chainIsExtended

                -- Block new chain extension during committed attacks (before 80%)
                local attackLocked = false
                if not chainIsOut and self._currentStateId ~= "idle" and self._currentStateId ~= "dash" then
                    local dur = self._currentStateData.clipDuration or self._currentStateData.duration or 0
                    local prog = (dur > 0) and math.min(self._stateTimer / dur, 1.0) or 1.0
                    attackLocked = (prog < 0.6)
                end

                --print(string.format("[ComboManager] chain press: cooldown=%.2f isExtended=%s",
                --    self._chainAttackCooldown, tostring(chainIsOut)))
                if attackLocked then
                    self._chainPressBlocked = true
                elseif self._chainAttackCooldown <= 0 or chainIsOut then
                    event_bus.publish("chain.down", {})
                    self._chainPressBlocked = false
                else
                    self._chainPressBlocked = true
                end
            end

            if input:IsChainJustReleased() then
                if not self._chainPressBlocked then
                    event_bus.publish("chain.up", {})
                end
                self._chainPressBlocked = false
            end

            -- Publish hold once when threshold is crossed, not every frame
            if input:IsChainHeld() then
                if not self._chainHoldPublished then
                    event_bus.publish("chain.hold", {})
                    self._chainHoldPublished = true
                end
            else
                self._chainHoldPublished = false
            end
        end

        -- ══════════════════════════════════════════════════════════════════
        -- DASH LOCK
        -- During a dash the combo state machine is paused. Clear any queued
        -- input so it doesn't fire the instant the dash ends.
        -- ══════════════════════════════════════════════════════════════════
        -- Block all combo input near interactable (tooltip active)
        if _G.playerNearInteractable then return end

        -- Before weapon pickup, only dash is available; skip all combat logic
        if not _G.playerHasWeapon then
            if input:HasBufferedDash() and not _G.player_is_dashing then
                input:ConsumeBufferedDash()
                if event_bus then
                    event_bus.publish("dash_performed", {})
                end
            end
            return
        end

        -- When attacks are disabled, drain stale buffers, force idle if mid-combo,
        -- and skip the state machine entirely. Chain weapon events (published above)
        -- and player movement are unaffected.
        if not self.AttacksEnabled then
            if input:HasBufferedAttack() then input:ConsumeBufferedAttack() end
            if input:HasBufferedChain()  then input:ConsumeBufferedChain()  end
            if self._currentStateId ~= "idle" and self._currentStateId ~= "dash" then
                self:_transitionTo("idle")
            end
            self._queuedCombo = nil
            return
        end

        -- ══════════════════════════════════════════════════════════════════
        -- DASH LOCK
        -- During a dash the combo state machine is paused. Clear any queued
        -- input so it doesn't fire the instant the dash ends.
        -- ══════════════════════════════════════════════════════════════════
        -- Block all combo input during dash
        if _G.player_is_dashing then
            if self._queuedCombo then
                --print("[ComboManager] DASH ACTIVE: clearing queued combo '" .. tostring(self._queuedCombo.stateId) .. "'")
                self._queuedCombo = nil
            end
            return
        end

        -- ══════════════════════════════════════════════════════════════════
        -- ADVANCE STATE TIMER
        -- Scaled by animation playback speed so the combo window and
        -- auto-transition fire at the correct moment even when boosted.
        -- ══════════════════════════════════════════════════════════════════
        local animSpeed = self._animator.speed or 1.0
        if self._chainAttackCooldown > 0 then
            self._chainAttackCooldown = self._chainAttackCooldown - dt
        end
        if self._aerialLockoutTimer > 0 then
            self._aerialLockoutTimer = self._aerialLockoutTimer - dt
        end
        self._stateTimer = self._stateTimer + dt * animSpeed
        local state = self._currentStateData
        self:_applySwingTuning(state)

        local stateObj = {
            id           = state.id,
            timer        = self._stateTimer,
            damage       = state.damage,
            actualDamage = state.actualDamage or state.damage,
        }

        -- ══════════════════════════════════════════════════════════════════
        -- CUSTOM STATE LOGIC (onUpdate)
        -- If a state defines onUpdate it takes full control for this tick.
        -- ══════════════════════════════════════════════════════════════════
        if state.onUpdate then
            state.onUpdate(self, stateObj, dt)
            return
        end

        -- ══════════════════════════════════════════════════════════════════
        -- COMPUTE TIME REMAINING
        -- clipDuration is the natural clip length in seconds at speed 1.0,
        -- set as a field on each combo state — no animator queries.
        -- timeRemaining = clipDuration - stateTimer
        -- Falls back to state.duration - stateTimer if clipDuration is nil.
        -- ══════════════════════════════════════════════════════════════════
        local timeRemaining = nil
        local refDuration = self:_swingDuration(state)
        if refDuration and refDuration > 0 then
            timeRemaining = math.max(0, refDuration - self._stateTimer)
        end

        -- Combo window scaled to real-time (constant regardless of anim speed)
        local window = state.comboWindow
        if window ~= nil then
            window = window * animSpeed
        end

        -- ══════════════════════════════════════════════════════════════════
        -- IMMEDIATE CANCELS  (dash cancel / lift attack / jump cancel)
        -- These bypass the combo window and queuing system entirely.
        -- Priority: dash cancel > lift attack / jump cancel.
        --
        --   Dash cancel  : dash buffered from any non-idle state.
        --                  Fires immediately, clears any queued input.
        --
        --   Lift attack  : jump just-pressed AND state has transitions.jump.
        --                  Only light_1 and light_3 route here (per combo list).
        --                  canMove=true on lift_attack lets PlayerMovement
        --                  execute the jump on the same frame.
        --
        --   Jump cancel  : jump just-pressed AND state has NO transitions.jump
        --                  AND state is not aerial (already airborne).
        --                  Exits to idle cleanly — PlayerMovement sees
        --                  IsJumpJustPressed() still true this frame and fires
        --                  a normal jump, cutting the attack's recovery frames.
        -- ══════════════════════════════════════════════════════════════════
        if state.id ~= "idle" then
            -- Animation commitment: attacks are locked until 80% of the animation
            -- plays out. Prevents spam-cancelling attacks with dash/jump.
            local cancelThreshold = 0.6
            local animProgress = (refDuration and refDuration > 0)
                and math.min(self._stateTimer / refDuration, 1.0) or 1.0

            if input:HasBufferedDash() and state.transitions.dash and animProgress >= cancelThreshold then
                input:ConsumeBufferedDash()
                self._queuedCombo = nil
                --print("[ComboManager] DASH CANCEL: " .. state.id .. " -> dash")
                self:_transitionTo("dash")
                return
            end

            if input:IsJumpJustPressed() and not _G.player_is_jumping and animProgress >= cancelThreshold then
                if state.transitions.jump then
                    -- Lift attack: this state explicitly launches into an aerial state
                    self._queuedCombo = nil
                    --print("[ComboManager] LIFT ATTACK: " .. state.id .. " -> " .. state.transitions.jump)
                    self:_transitionTo(state.transitions.jump)
                    return
                elseif not state.isAerial then
                    -- Jump cancel: no lift on this state — exit to idle so
                    -- PlayerMovement's jump check fires naturally this frame.
                    self._queuedCombo = nil
                    --print("[ComboManager] JUMP CANCEL: " .. state.id .. " -> idle")
                    self:_transitionTo("idle")
                    return
                end
            end
        end

        -- ══════════════════════════════════════════════════════════════════
        -- EXECUTE QUEUED COMBO (fires when the window opens or on idle)
        -- ══════════════════════════════════════════════════════════════════
        if self._queuedCombo then
            local isWindowOpen = state.id == "idle"
                or (timeRemaining and window and timeRemaining <= window)

            if isWindowOpen then
                local queued = self._queuedCombo
                self._queuedCombo = nil
                self:_transitionTo(queued.stateId, queued.data)
                return
            else
                -- Expire stale queued inputs to prevent forever-queue
                local maxQueueLife = self.maxQueuedInputLife or 1.0
                if (self._stateTimer - (self._queuedCombo.requestedAt or 0)) > maxQueueLife then
                    self._queuedCombo = nil
                end
            end
        end

        -- ══════════════════════════════════════════════════════════════════
        -- READ BUFFERED INPUTS → CANDIDATE TRANSITION
        -- Priority: attack > chain > dash
        -- ══════════════════════════════════════════════════════════════════
        local candidateStateId = nil
        local candidateData    = nil

        if input:HasBufferedAttack() then
            if input:IsAttackHeld() then
                candidateStateId = state.transitions.attack_hold

            elseif _G.player_is_jumping then
                -- All aerial attack routing lives here.
                local height    = _G.player_air_height or 0
                local minHeight = self.MinAerialAttackHeight or 0.8

                -- Height gate: player must be above MinAerialAttackHeight.
                -- Too close to the ground → consume input silently, let them land.
                if height < minHeight then
                    input:ConsumeBufferedAttack()

                -- Aerial lockout: blocks mash between hits without punishing timing.
                elseif self._aerialLockoutTimer > 0 then
                    -- Don't consume — let the buffer carry until lockout clears.
                    -- The queued combo system will pick it up when the window opens.

                elseif state.id == "idle" then
                    -- No active combo. Height decides slam vs aerial start.
                    local threshold = self.SlamHeightThreshold or 5.0
                    if threshold > 0 and height >= threshold then
                        candidateStateId = "air_slam"
                    else
                        candidateStateId = "air_light_1"
                    end

                elseif state.id == "air_light_2" then
                    -- Hit confirmed → loop; missed → slam.
                    --
                    -- This reads the string-scoped flag rather than the
                    -- per-state one. The branch is evaluated the moment the
                    -- attack input arrives, which is before air_light_2's own
                    -- hitbox has connected, while the per-state flag is reset
                    -- on entry to air_light_2. So the per-state flag could
                    -- only be true if a hit landed in the sliver between
                    -- entering the state and the player pressing again, and
                    -- measured traces took the slam path on every string that
                    -- did connect. The question the branch is asking is
                    -- whether this trip through the air has been landing, so
                    -- the flag it reads is reset when an aerial string begins.
                    if self._aerialStringHit then
                        candidateStateId = "air_light_1"
                    else
                        candidateStateId = "air_slam"
                    end

                else
                    candidateStateId = state.transitions.attack
                end

            else
                candidateStateId = state.transitions.attack
            end

        elseif input:HasBufferedChain() then
            -- If a throwable is hooked, chain input belongs to ChainBootstrap.
            -- The chain.down event was already published above — don't consume
            -- this buffer for chain_attack, let it pass through.
            if not self._chainHasThrowable then
                -- Only trigger chain_attack when retracted AND cooldown clear.
                -- If chain is already out, the press is a retract — don't attack.
                if self._chainAttackCooldown <= 0 and not self._chainIsExtended then
                    candidateStateId = state.transitions.chain
                else
                    input:ConsumeBufferedChain()
                end
            else
                --print("[ComboManager] chain input suppressed - throwable hooked")
            end

        elseif input:HasBufferedDash() then
            -- During active attacks, dash is handled exclusively by the
            -- immediate cancel system above (gated at 80% animation progress).
            -- Don't route through the combo window / queuing path here, as that
            -- would speed up the animation and allow earlier cancels.
            -- Only allow the regular path when idle (normal dash from standing).
            if state.id == "idle" then
                candidateStateId = state.transitions.dash
            end
        end

        -- No valid transition — check for auto-idle at end of animation
        if not candidateStateId then
            if self._stateTimer >= self:_swingEnd(state) and state.id ~= "idle" then
                self:_swingTrace(state)
                self:_transitionTo("idle")
            end
            return
        end

        -- Consume the buffered input immediately
        if input:HasBufferedAttack() then
            input:ConsumeBufferedAttack()
        elseif input:HasBufferedChain() then
            input:ConsumeBufferedChain()
        elseif input:HasBufferedDash() then
            input:ConsumeBufferedDash()
        end

        --print("[ComboManager] Candidate: " .. tostring(candidateStateId) .. " from: " .. state.id)

        -- Idle → transition immediately
        if state.id == "idle" then
            self:_transitionTo(candidateStateId, candidateData)
            return
        end

        -- State has no combo continuation → silently discard
        if window == nil then
            return
        end

        -- Already inside the combo window → transition now
        if timeRemaining and timeRemaining <= window then
            self:_transitionTo(candidateStateId, candidateData)
            return
        end

        -- Pressed too early → queue and accelerate animation toward the window
        self._queuedCombo = {
            stateId     = candidateStateId,
            data        = candidateData,
            requestedAt = self._stateTimer,
        }

        if timeRemaining and state.duration and state.duration > 0 and self._animator then
            local earlyTime   = math.max(0, timeRemaining - (window or 0))
            local earlyFactor = math.min(1.0, earlyTime / math.max(0.001, state.duration - (window or 0)))
            local base        = self._animator.speed
            local speedMult   = base * (1.0 + (self.MaxComboAnimSpeed - 1.0) * earlyFactor)
            self._animator:SetSpeed(speedMult)
        end

        -- Check for auto-idle at end of animation (in case nothing queued fires)
        if self._stateTimer >= self:_swingEnd(state) and state.id ~= "idle" then
            self:_swingTrace(state)
            self:_transitionTo("idle")
        end
    end,

    -- ══════════════════════════════════════════════════════════════════════
    -- STATE TRANSITION
    -- ══════════════════════════════════════════════════════════════════════
    -- Reshape a swing without touching its clip. See SWING_VARIANTS at the top.
    _swingTuningFor = function(self, stateId)
        local variant = SWING_VARIANTS[self.SwingVariant or "off"]
        if not variant then return nil end
        return variant[stateId]
    end,

    -- The clip length a variant leaves behind, which is what the combo window
    -- is measured against.
    _swingDuration = function(self, state)
        local ref = state.clipDuration or state.duration
        local tune = self:_swingTuningFor(state.id)
        if tune and tune.finish and ref and ref > 0 then
            ref = ref * math.max(0.05, math.min(1.0, tune.finish))
        end
        return ref
    end,

    -- Where the state stops. Separate from the above because the exit has
    -- always compared against state.duration, and only one of the two is
    -- allowed to be nil.
    -- Set true to trace each swing's real length to stderr. Off in a shipped
    -- build; this is how the variant numbers in .agents/item8 were measured.
    _swingTrace = function(self, state)
        if not _G.SWING_TRACE then return end
        local tune = self:_swingTuningFor(state.id) or {}
        local clip = state.clipDuration or state.duration or 0
        local from = (tune.start or 0) * clip
        local ran = (self._stateTimer or 0) - from
        io.stderr:write(string.format(
            "[swing] variant=%s state=%s from=%.3f to=%.3f ran=%.3f clipsec "
            .. "animspeed=%.2f\n",
            tostring(self.SwingVariant), tostring(state.id), from,
            self._stateTimer or -1, ran,
            (self._animator and self._animator.speed) or -1))
    end,

    _swingEnd = function(self, state)
        local tune = self:_swingTuningFor(state.id)
        if tune and tune.finish and state.duration and state.duration > 0 then
            return state.duration * math.max(0.05, math.min(1.0, tune.finish))
        end
        return state.duration
    end,

    _applySwingTuning = function(self, state)
        if not (state and state.id and self._animator) then return end
        local tune = self:_swingTuningFor(state.id)
        if not tune then return end

        local clip = state.clipDuration or state.duration or 0
        if clip <= 0 then return end

        -- Captured on entry, before any ramp has touched it.
        if self._swingBaseSpeed == nil then
            self._swingBaseSpeed = self._animator.speed or 1.0
        end

        -- Skip the wind up. The animator switches clips on its own update, so
        -- the first tick after the transition can still be showing the previous
        -- one; this keeps trying for a few ticks and stops as soon as the
        -- playhead it moved is the one it meant to move.
        local startAt = tune.start or 0
        if self._swingStartPending == state.id then
            if startAt <= 0 then
                self._swingStartPending = nil
            else
                self._swingStartTries = (self._swingStartTries or 0) + 1
                local now = self._animator.GetNormalizedTime
                          and self._animator:GetNormalizedTime() or 0
                if now < startAt or self._swingStartTries >= 5 then
                    local ok, err = pcall(function()
                        self._animator:SetNormalizedTime(startAt,
                                                         self._animatorEntity)
                    end)
                    if not ok and _G.SWING_TRACE then
                        io.stderr:write("[swing] SetNormalizedTime error: "
                                        .. tostring(err) .. "\n")
                    end
                    if _G.SWING_TRACE then
                        io.stderr:write(string.format(
                            "[swing] start-trim %s at %.2f applied=%s tries=%d was=%.3f\n",
                            tostring(state.id), startAt, tostring(ok),
                            self._swingStartTries, now))
                    end
                    -- The state timer measures the same clip, so it starts
                    -- there too and the combo window still lands where it does.
                    self._stateTimer = startAt * clip
                    self._swingStartPending = nil
                    self._swingStartTries = 0
                end
            end
        end

        if not tune.ramp then return end

        -- Held between points: the last point at or before now wins.
        local at = math.min(self._stateTimer / clip, 1.0)
        local speed = nil
        for _, point in ipairs(tune.ramp) do
            if at >= point[1] then speed = point[2] else break end
        end
        -- Relative to the speed the animator state already runs at, which for
        -- every light hit is 2.0. Treating these as absolute made "1.8" a
        -- slowdown and the first measured ramp came out slower than baseline.
        if speed and speed ~= self._swingRampApplied then
            self._swingRampApplied = speed
            self._animator:SetSpeed(speed * (self._swingBaseSpeed or 1.0))
        end
    end,

    _transitionTo = function(self, stateId, data)
        local newState = self.COMBO_TREE[stateId]
        if not newState then
            --print("[ComboManager] ERROR: Invalid state: " .. tostring(stateId))
            return
        end
        if stateId == "chain_attack" then
            self._chainAttackCooldown = self.ChainAttackCooldown or 0.6
        end
        if not self._animator then
            --print("[ComboManager] ERROR: Animator not available for transition")
            return
        end

        -- Exit current state
        local oldState = self._currentStateData
        if oldState.onExit then
            oldState.onExit(self, { id = oldState.id, timer = self._stateTimer }, data)
        end

        -- Enter new state
        self._currentStateId   = stateId
        self._currentStateData = newState
        self._stateTimer       = 0
        self._swingRampApplied = nil
        self._swingStartPending = stateId
        self._swingStartTries = 0
        self._swingBaseSpeed = nil

        -- ── Update global combat flags ────────────────────────────────────
        _G.player_is_attacking = (stateId ~= "idle" and stateId ~= "dash")
        _G.player_can_move     = newState.canMove or false

        -- Reset aerial hit flag and arm lockout on every aerial state entry.
        if newState.isAerial then
            self._lastAerialHitLanded = false
            -- A new aerial string starts only when entering the air from a
            -- state that was not itself aerial. Within a string the flag
            -- carries, so a hit in air_light_1 still counts when air_light_2
            -- decides whether to loop.
            if not (oldState and oldState.isAerial) then
                self._aerialStringHit = false
            end
            if newState.isSlam ~= true then
                -- Lockout prevents input being registered again until this decays.
                -- Slam is excluded — it's a commitment, not a repeatable attack.
                self._aerialLockoutTimer = self.AerialHitLockout or 0.18
            end
        end

        -- Track combo chain sequence (for UI / scoring)
        if stateId ~= "idle" and stateId ~= "dash" then
            table.insert(self._comboChain, stateId)
        else
            self._comboChain = {}
        end

        -- ── Animator: combat parameters only ─────────────────────────────
        -- Movement parameters (IsRunning, IsJumping, IsDashing) remain
        -- exclusively owned by PlayerMovement.
        self._animator:SetInt("ComboStep", newState.animParam)

        if stateId == "heavy_charge" then
            self._animator:SetBool("IsHeavyCharging", true)
            self._animator:SetBool("IsAttacking", false)
            self._animator:SetBool("IsAirAttacking", false)
        elseif stateId == "idle" or stateId == "dash" then
            self._animator:SetBool("IsAttacking", false)
            self._animator:SetBool("IsHeavyCharging", false)
            self._animator:SetBool("IsAirAttacking", false)
        elseif newState.isAerial then
            -- Aerial states drive a separate animator layer so ground
            -- and air attack clips don't share the same bool.
            self._animator:SetBool("IsAirAttacking", true)
            self._animator:SetBool("IsAttacking", false)
            self._animator:SetBool("IsHeavyCharging", false)
        else
            self._animator:SetBool("IsAttacking", true)
            self._animator:SetBool("IsHeavyCharging", false)
            self._animator:SetBool("IsAirAttacking", false)
        end

        if stateId ~= "idle" and stateId ~= "dash" then
            self._animator:SetTrigger("Attack")
            -- SFX is handled by CombatAudio, which reacts to attack_performed.
            -- ComboManager triggers the animation; it does not play sounds.
        end

        -- ── Broadcast: combat_state_changed ──────────────────────────────
        -- Carries canMove so PlayerMovement can adjust movement without
        -- polling _G globals directly.
        if event_bus then
            event_bus.publish("combat_state_changed", {
                state      = stateId,
                canMove    = newState.canMove or false,
                comboChain = self._comboChain,
                isAerial   = newState.isAerial or false,
            })
        end

        --print("[ComboManager] " .. oldState.id .. " -> " .. stateId
        --    .. " (ComboStep: " .. newState.animParam .. ")")

        -- ── State entry callbacks ─────────────────────────────────────────
        if newState.onEnter then
            local stateObj = {
                id           = newState.id,
                timer        = 0,
                damage       = newState.damage,
                actualDamage = newState.actualDamage or newState.damage,
            }
            newState.onEnter(self, stateObj, data)

        elseif stateId ~= "idle" and stateId ~= "dash" and stateId ~= "heavy_charge" then
            -- Default: broadcast attack_performed with per-state lunge data so
            -- PlayerMovement can execute the correct impulse without hardcoding.
            if event_bus then
                event_bus.publish("attack_performed", {
                    state     = stateId,
                    damage    = newState.damage,
                    knockback = newState.knockback or 0,
                    lunge     = newState.lunge,
                    isAerial  = newState.isAerial or false,
                    isSlam    = newState.isSlam or false,
                    isLift    = newState.isLift  or false,
                })
            end
        end
    end,

    -- ══════════════════════════════════════════════════════════════════════
    -- PUBLIC API
    -- ══════════════════════════════════════════════════════════════════════
    GetCurrentState = function(self)
        return self._currentStateId
    end,

    IsAttacking = function(self)
        return self._currentStateData.damage > 0
    end,

    CanMove = function(self)
        return self._currentStateData.canMove or false
    end,

    GetCurrentComboChain = function(self)
        return self._comboChain
    end,
}