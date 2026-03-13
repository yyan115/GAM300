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

return Component {
    fields = {
        DefaultComboWindow  = 0.5,
        HeavyChargeTime     = 0.8,
        MaxComboAnimSpeed   = 2.0,
        -- Minimum seconds between chain attacks. The tap-fire (ChainBootstrap)
        -- still fires every press; only the attack animation is gated.
        ChainAttackCooldown = 0.6,
        -- When false, all attack and combo inputs are suppressed and the combo
        -- state machine does not run. Chain weapon events and movement are
        -- unaffected. Toggle from any external system (cutscene, tutorial gate).
        AttacksEnabled      = true,
        -- Minimum height above last grounded Y before aerial attacks are allowed.
        -- Below this the player is too close to the ground for aerial combo to make sense.
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
                        print("[ComboManager] DASH CANCEL: heavy_charge -> dash")
                        self:_transitionTo("dash")
                        return
                    end

                    -- Jump cancel: exit charge cleanly; PlayerMovement fires the jump this frame
                    if input:IsJumpJustPressed() and not _G.player_is_jumping then
                        self._queuedCombo = nil
                        print("[ComboManager] JUMP CANCEL: heavy_charge -> idle")
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
                    print("[ComboManager] Heavy released: " .. math.floor(chargePercent * 100) .. "% charge")
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
                duration     = 1.0,
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
                knockback    = 5.0,
                canMove      = true,
                isAerial     = true,
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
        self._aerialLockoutTimer  = 0   -- blocks aerial attack input when > 0
    end,

    Start = function(self)
        local playerEntityId = Engine.GetEntityByName("Player")
        if not playerEntityId then
            print("[ComboManager] ERROR: Player entity not found!")
            return
        end
        self._playerEntityId = playerEntityId
        print("[ComboManager] Player entity found (ID: " .. tostring(playerEntityId) .. ")")

        self._animator = Engine.FindAnimatorByName("Player")
        if not self._animator then
            print("[ComboManager] ERROR: Player AnimationComponent not found!")
            return
        end

        self._inputInterpreter = _G.InputInterpreter
        if not self._inputInterpreter then
            print("[ComboManager] ERROR: InputInterpreter not found!")
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
                    print("[ComboManager] Throwable hooked - chain_attack blocked")
                end
            end)
            self._subRetracted = _G.event_bus.subscribe("chain.endpoint_retracted", function()
                if self._chainHasThrowable then
                    self._chainHasThrowable = false
                    print("[ComboManager] Throwable released - chain_attack unblocked")
                end
            end)
            self._subThrowFired = _G.event_bus.subscribe("chain.throwable_throw", function()
                if self._chainHasThrowable then
                    self._chainHasThrowable = false
                    print("[ComboManager] Throwable thrown - chain_attack unblocked")
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
                print("[ComboManager] AttacksEnabled = " .. tostring(self.AttacksEnabled))
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
                end
            end)
        end

        print("[ComboManager] Initialized successfully")
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
                print(string.format("[ComboManager] chain press: cooldown=%.2f isExtended=%s",
                    self._chainAttackCooldown, tostring(chainIsOut)))
                if self._chainAttackCooldown <= 0 or chainIsOut then
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
                print("[ComboManager] DASH ACTIVE: clearing queued combo '" .. tostring(self._queuedCombo.stateId) .. "'")
                self._queuedCombo = nil
            end
            return
        end

        -- ══════════════════════════════════════════════════════════════════
        -- SLAM BUFFER LOCK
        -- While slam buffer is active the player is locked out. Discard all
        -- buffered inputs so nothing fires the instant control returns.
        -- ══════════════════════════════════════════════════════════════════
        if _G.player_is_slam_buffering then
            if input:HasBufferedAttack() then input:ConsumeBufferedAttack() end
            if input:HasBufferedChain()  then input:ConsumeBufferedChain()  end
            if input:HasBufferedDash()   then input:ConsumeBufferedDash()   end
            if self._queuedCombo then
                print("[ComboManager] SLAM BUFFER: clearing queued combo '" .. tostring(self._queuedCombo.stateId) .. "'")
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
        local refDuration = state.clipDuration or state.duration
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
            if input:HasBufferedDash() and state.transitions.dash then
                input:ConsumeBufferedDash()
                self._queuedCombo = nil
                print("[ComboManager] DASH CANCEL: " .. state.id .. " -> dash")
                self:_transitionTo("dash")
                return
            end

            if input:IsJumpJustPressed() and not _G.player_is_jumping then
                if state.transitions.jump then
                    -- Lift attack: this state explicitly launches into an aerial state
                    self._queuedCombo = nil
                    print("[ComboManager] LIFT ATTACK: " .. state.id .. " -> " .. state.transitions.jump)
                    self:_transitionTo(state.transitions.jump)
                    return
                elseif not state.isAerial then
                    -- Jump cancel: no lift on this state — exit to idle so
                    -- PlayerMovement's jump check fires naturally this frame.
                    self._queuedCombo = nil
                    print("[ComboManager] JUMP CANCEL: " .. state.id .. " -> idle")
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
                    if self._lastAerialHitLanded then
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
                print("[ComboManager] chain input suppressed - throwable hooked")
            end

        elseif input:HasBufferedDash() then
            candidateStateId = state.transitions.dash
        end

        -- No valid transition — check for auto-idle at end of animation
        if not candidateStateId then
            if self._stateTimer >= state.duration and state.id ~= "idle" then
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

        print("[ComboManager] Candidate: " .. tostring(candidateStateId) .. " from: " .. state.id)

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
        if self._stateTimer >= state.duration and state.id ~= "idle" then
            self:_transitionTo("idle")
        end
    end,

    -- ══════════════════════════════════════════════════════════════════════
    -- STATE TRANSITION
    -- ══════════════════════════════════════════════════════════════════════
    _transitionTo = function(self, stateId, data)
        local newState = self.COMBO_TREE[stateId]
        if not newState then
            print("[ComboManager] ERROR: Invalid state: " .. tostring(stateId))
            return
        end
        if stateId == "chain_attack" then
            self._chainAttackCooldown = self.ChainAttackCooldown or 0.6
        end
        if not self._animator then
            print("[ComboManager] ERROR: Animator not available for transition")
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

        -- ── Update global combat flags ────────────────────────────────────
        _G.player_is_attacking = (stateId ~= "idle" and stateId ~= "dash")
        _G.player_can_move     = newState.canMove or false

        -- Reset aerial hit flag and arm lockout on every aerial state entry.
        if newState.isAerial then
            self._lastAerialHitLanded = false
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

        print("[ComboManager] " .. oldState.id .. " -> " .. stateId
            .. " (ComboStep: " .. newState.animParam .. ")")

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