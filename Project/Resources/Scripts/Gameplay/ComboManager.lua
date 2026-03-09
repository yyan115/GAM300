--[[
================================================================================
COMBO MANAGER - DATA-DRIVEN COMBAT SYSTEM
================================================================================
PURPOSE:
    Manages all combat states, combos, and attack execution for fighting game.
    Uses data-driven combo tree structure for easy addition of new combos.

RESPONSIBILITIES:
    - Execute attacks based on player input and current combo state
    - Manage combo chains and branching paths
    - Handle attack timing windows and cancels
    - Update animator parameters (not PlayClip)
    - Broadcast damage events
    - Support diverse combo types (tap, hold, charge, special inputs)

ADDING NEW COMBOS:
    Modify the COMBO_TREE table in Awake(). Each node defines:
    - id: Unique state identifier
    - animParam: Integer value for ComboStep animator parameter
    - duration: Animation length (seconds)
    - damage: Base damage value
    - canMove: Whether player can move during this state
    - comboWindow: Time window to input next attack (nil = no combo)
    - onEnter: Optional callback when state begins
    - onUpdate: Optional callback during state (e.g., charging)
    - onExit: Optional callback when state ends
    - transitions: Table of {inputType → nextStateId} for combo paths
    
ANIMATOR PARAMETERS REQUIRED:
    - Integer: ComboStep (0 = idle, 1+ = attack states)
    - Bool: isAttacking
    - Bool: isHeavyCharging
    - Trigger: Attack

AUTHOR: Soh Wei Jie
VERSION: 3.0 (Animator Parameter-Driven)
================================================================================
--]]

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

return Component {
    fields = {
        -- Global combo settings (editable in editor)
        DefaultComboWindow = 0.5,    -- Default time to continue combo
        HeavyChargeTime = 0.8,       -- Time to fully charge heavy
        MaxComboAnimSpeed = 2.0,     -- Max animation speed when chaining quickly (multiplied on top of base)

        -- SFX clip arrays (populate in editor with audio GUIDs)
        playerSlashSFX = {},      -- FastSlash (whoosh on swing)
        playerChainSFX = {},      -- FastSlashHitonFlesh (impact on hit)
    },

    Awake = function(self)
        -- ===============================
        -- COMBO TREE DEFINITION
        -- ===============================
        
        self.COMBO_TREE = {
            -- IDLE STATE (starting point)
            idle = {
                id = "idle",
                animParam = 0,
                duration = 0,
                damage = 0,
                canMove = true,
                comboWindow = nil,  -- Always ready for input
                transitions = {
                    attack = "light_1",
                    attack_hold = "heavy_charge",
                    chain = "chain_attack",
                    dash = "dash"
                }
            },

            -- LIGHT ATTACK CHAIN (tap-tap-tap)
            light_1 = {
                id = "light_1",
                animParam = 1,
                duration = 1.0,
                damage = 10,
                knockback = 20.0,
                canMove = false,
                comboWindow = 0.2,
                transitions = {
                    attack = "light_2",
                    chain = "chain_attack"  -- Can cancel into chain
                }
            },

            light_2 = {
                id = "light_2",
                animParam = 2,
                duration = 1.0,
                damage = 12,
                knockback = 20.0,
                canMove = false,
                comboWindow = 0.2,
                transitions = {
                    attack = "light_3",
                    chain = "chain_attack"
                }
            },

            light_3 = {
                id = "light_3",
                animParam = 3,
                duration = 0.8,
                damage = 20,
                knockback = 200.0,
                canMove = false,
                comboWindow = nil,  -- Finisher - no combo continuation
                transitions = {}
            },

            -- HEAVY ATTACK (hold-release)
            heavy_charge = {
                id = "heavy_charge",
                animParam = 10,
                duration = 999,  -- Indefinite until release
                damage = 0,
                canMove = false,
                comboWindow = nil,
                
                -- Custom charging logic
                onUpdate = function(self, state, dt)
                    local input = self._inputInterpreter
                    local chargeTime = state.timer
                    
                    -- Auto-release at full charge
                    if chargeTime >= self.HeavyChargeTime then
                        self:_transitionTo("heavy_release", { chargePercent = 1.0 })
                        return
                    end
                    
                    -- Check for manual release
                    if input:IsAttackJustReleased() then
                        local chargePercent = math.min(chargeTime / self.HeavyChargeTime, 1.0)
                        self:_transitionTo("heavy_release", { chargePercent = chargePercent })
                    end
                end,
                
                transitions = {}  -- Handled by onUpdate
            },

            heavy_release = {
                id = "heavy_release",
                animParam = 11,
                duration = 0.8,
                damage = 30,  -- Will be multiplied by chargePercent
                knockback = 20.0,
                canMove = false,
                comboWindow = nil,
                
                -- Apply charge multiplier to damage
                onEnter = function(self, state, data)
                    data = data or {}
                    local chargePercent = data.chargePercent or 0.5
                    state.actualDamage = state.damage * (1.0 + chargePercent)
                    
                    -- Broadcast with charge info
                    if event_bus then
                        event_bus.publish("attack_performed", {
                            state = state.id,
                            damage = state.actualDamage,
                            chargePercent = chargePercent
                        })
                    end
                    
                    print("[ComboManager] Heavy released: " .. (chargePercent * 100) .. "% charge")
                end,
                
                transitions = {}
            },

            -- CHAIN ATTACK
            chain_attack = {
                id = "chain_attack",
                animParam = 20,
                duration = 0.5,
                damage = 25,
                knockback = 1.0,
                canMove = false,
                comboWindow = 0.5,
                transitions = {
                    attack = "light_1"  -- Chain can combo into lights
                }
            },

            -- DASH
            dash = {
                id = "dash",
                animParam = 30,
                duration = 1.0,
                damage = 0,
                canMove = false,
                comboWindow = nil,

                onEnter = function(self, state, data)
                    if event_bus then
                        event_bus.publish("dash_performed", {})
                    end
                end,

                transitions = {}
            },
        }

        -- ===============================
        -- SPECIAL INPUT SEQUENCES
        -- ===============================
        self.SPECIAL_INPUTS = {
            -- Example: Double-tap attack within 10 frames = special move
        }

        -- ===============================
        -- RUNTIME STATE
        -- ===============================
        self._inputInterpreter = nil
        self._animator = nil
        self._playerObj = nil
        
        self._currentStateId = "idle"
        self._currentStateData = self.COMBO_TREE["idle"]
        self._stateTimer = 0
        self._comboChain = {}  -- Track combo sequence for UI/scoring

        -- Throwable hook tracking: true while chain endpoint is hooked to a Throwable.
        -- When true, chain input is routed to throw/swing logic instead of chain_attack.
        self._chainHasThrowable = false
    end,

    Start = function(self)
        -- Find Player Entity ID
        local playerEntityId = Engine.GetEntityByName("Player")
        if not playerEntityId then
            print("[ComboManager] ERROR: Player entity not found!")
            return
        end
        print("[ComboManager] Player entity found (ID: " .. tostring(playerEntityId) .. ")")

        -- Store entity ID for later use if needed
        self._playerEntityId = playerEntityId

        -- Get animator from Player using entity ID
        self._animator = Engine.FindAnimatorByName("Player")
        if not self._animator then
            print("[ComboManager] ERROR: Player AnimationComponent not found!")
            return
        end
        print("[ComboManager] Player AnimationComponent found")

        -- Get InputInterpreter from global singleton
        self._inputInterpreter = _G.InputInterpreter
        if not self._inputInterpreter then
            print("[ComboManager] ERROR: InputInterpreter not found!")
            return
        end
        print("[ComboManager] InputInterpreter found")

        -- Get AudioComponent from Player for attack SFX
        self._playerAudio = GetComponent(self._playerEntityId, "AudioComponent")
        if not self._playerAudio then
            print("[ComboManager] WARNING: Player AudioComponent not found")
        end

        -- Initialize animator parameters using the bound methods
        self._animator:SetInt("ComboStep", 0)        -- Note: SetInt, not SetInteger
        self._animator:SetBool("IsAttacking", false)
        self._animator:SetBool("IsHeavyCharging", false)

        -- ── Throwable awareness ──────────────────────────────────────────────
        -- Track whether the chain endpoint is currently hooked to a Throwable.
        -- While true, chain input must NOT become chain_attack — ChainBootstrap
        -- owns that input to decide throw vs swing.
        if _G.event_bus and _G.event_bus.subscribe then
            self._subHitEntity = _G.event_bus.subscribe("chain.endpoint_hit_entity", function(payload)
                if not payload then return end
                if payload.isThrowable then
                    self._chainHasThrowable = true
                    print("[ComboManager] Throwable hooked — chain_attack blocked")
                end
            end)
            -- Clear on retract (chain fully pulled back)
            self._subRetracted = _G.event_bus.subscribe("chain.endpoint_retracted", function(payload)
                if self._chainHasThrowable then
                    self._chainHasThrowable = false
                    print("[ComboManager] Throwable released — chain_attack unblocked")
                end
            end)
            -- Clear immediately when throw is fired (chain still retracting)
            self._subThrowFired = _G.event_bus.subscribe("chain.throwable_throw", function(payload)
                if self._chainHasThrowable then
                    self._chainHasThrowable = false
                    print("[ComboManager] Throwable thrown — chain_attack unblocked")
                end
            end)
        end
        -- ────────────────────────────────────────────────────────────────────

        print("[ComboManager] Initialized successfully")
    end,

    Update = function(self, dt)
        if not self._inputInterpreter or not self._animator or Time.IsPaused() then return end

        -- Block all combo input during dash
        if _G.player_is_dashing then
            -- Also clear any queued combo to prevent it firing after dash ends
            if self._queuedCombo then
                print("[ComboManager] DASH ACTIVE: clearing queued combo '" .. tostring(self._queuedCombo.stateId) .. "'")
                self._queuedCombo = nil
            end
            return
        end

        -- Advance logical timer at the same rate as the animation playback.
        -- When boosted (e.g. speed=4.0), timer runs 4x faster so the combo
        -- window and auto-transition fire at the right moment instead of lagging behind.
        local animSpeed = self._animator.speed or 1.0
        self._stateTimer = self._stateTimer + dt * animSpeed
        local state = self._currentStateData
        
        -- Create state object for callbacks
        local stateObj = {
            id = state.id,
            timer = self._stateTimer,
            damage = state.damage,
            actualDamage = state.actualDamage or state.damage
        }

        -- ===============================
        -- CUSTOM STATE LOGIC (onUpdate)
        -- ===============================
        if state.onUpdate then
            state.onUpdate(self, stateObj, dt)
            return  -- Custom logic takes full control
        end

        -- ===============================
        -- CHECK FOR TRANSITIONS (queued inputs, combo window at animation end)
        -- ===============================
        local input = self._inputInterpreter

        -- Helper: compute time remaining using animator playback info when available, otherwise fallback to state.duration
        local timeRemaining = nil
        if self._animator and self._animator.GetCurrentStateLength and self._animator.GetCurrentStateTime then
            local length = self._animator:GetCurrentStateLength()
            local time = self._animator:GetCurrentStateTime()
            if length and time then
                timeRemaining = math.max(0, length - time)
            end
        end
        if not timeRemaining and state.duration and state.duration > 0 then
            timeRemaining = state.duration - self._stateTimer
        end

        -- Combo window for this state (real-time seconds). nil means "no continuation allowed".
        -- Scale by animSpeed so the real-time window stays constant regardless of playback rate.
        local window = state.comboWindow
        if window ~= nil then
            window = (window or self.DefaultComboWindow) * animSpeed
        end

        -- If we have a queued combo input, try to execute it once the window opens (or immediately if idle)
        if self._queuedCombo then
            if state.id == "idle" or (timeRemaining and window and timeRemaining <= window) then
                local queued = self._queuedCombo
                self._queuedCombo = nil
                self:_transitionTo(queued.stateId, queued.data)
                return
            else
                -- Optional: expire stale queued inputs (avoid forever queue). Lifetime = 1.0s by default.
                local maxQueueLife = self.maxQueuedInputLife or 1.0
                if (self._stateTimer - (self._queuedCombo.requestedAt or 0)) > maxQueueLife then
                    self._queuedCombo = nil
                end
            end
        end

        -- Read buffered inputs (priority: attack > chain > dash)
        local candidateStateId = nil
        local candidateData = nil

        if input:HasBufferedAttack() then
            print("[ComboManager] HasBufferedAttack=true, player_is_dashing=" .. tostring(_G.player_is_dashing))
            if input:IsAttackHeld() then
                candidateStateId = state.transitions.attack_hold
            else
                candidateStateId = state.transitions.attack
            end
        elseif input:HasBufferedChain() then
            print("[ComboManager] HasBufferedChain=true, player_is_dashing=" .. tostring(_G.player_is_dashing))
            -- If a throwable is hooked, the chain input belongs to ChainBootstrap
            -- (throw vs swing decision). Do NOT consume it here — let it pass through
            -- to chain.up so Bootstrap can act on it.
            if self._chainHasThrowable then
                print("[ComboManager] chain input suppressed — throwable is hooked, deferring to ChainBootstrap")
            else
                candidateStateId = state.transitions.chain
            end
        elseif input:HasBufferedDash() then
            candidateStateId = state.transitions.dash
        end

        if candidateStateId then
            print("[ComboManager] Candidate transition: " .. tostring(candidateStateId) .. " from state: " .. state.id)
            -- Consume the buffered input immediately so it doesn't re-fire repeatedly
            if input:HasBufferedAttack() then input:ConsumeBufferedAttack()
            elseif input:HasBufferedChain() then input:ConsumeBufferedChain()
            elseif input:HasBufferedDash() then input:ConsumeBufferedDash() end

            -- If we're idle -> transition immediately
            if state.id == "idle" then
                self:_transitionTo(candidateStateId, candidateData)
                return
            end

            -- If this state has no combo continuation (comboWindow == nil) -> ignore/consume input (no queue)
            if window == nil then
                -- nothing to do (input consumed)
                return
            end

            -- If we're already inside the *end* of the animation (final `window` seconds) -> transition now
            if timeRemaining and window and timeRemaining <= window then
                self:_transitionTo(candidateStateId, candidateData)
                return
            end

            -- Otherwise: queue the input so it will fire when the window opens.
            -- Replace any existing queued input with the latest (player intent = latest press).
            self._queuedCombo = {
                stateId = candidateStateId,
                data = candidateData,
                requestedAt = self._stateTimer
            }

            -- Speed up current animation based on how early the input came in.
            -- earlyFactor=1 (pressed right at start) → maxSpeed, earlyFactor=0 (pressed at window) → 1.0
            if timeRemaining and state.duration and state.duration > 0 and self._animator then
                -- window is already in animation-time (scaled by animSpeed above).
                -- earlyFactor: 1.0 = pressed at the very start, 0.0 = pressed right at window open.
                local earlyTime = math.max(0, timeRemaining - (window or 0))
                local earlyFactor = math.min(1.0, earlyTime / math.max(0.001, state.duration - (window or 0)))
                -- Read the state machine's configured speed for this state (e.g. 2.0).
                -- Boost is a multiplier on top of that: MaxComboAnimSpeed=2 means "up to 2x faster than normal".
                local base = self._animator.speed
                local speedMult = base * (1.0 + (self.MaxComboAnimSpeed - 1.0) * earlyFactor)
                self._animator:SetSpeed(speedMult)
            end
        end

        -- ===============================
        -- AUTO-TRANSITION ON ANIMATION END
        -- ===============================
        if self._stateTimer >= state.duration and state.id ~= "idle" then
            self:_transitionTo("idle")
        end
    end,

    -- ===============================
    -- STATE TRANSITION SYSTEM
    -- ===============================
    _transitionTo = function(self, stateId, data)
        local newState = self.COMBO_TREE[stateId]
        if not newState then
            print("[ComboManager] ERROR: Invalid state: " .. tostring(stateId))
            return
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
        self._currentStateId = stateId
        self._currentStateData = newState
        self._stateTimer = 0

        -- Update global attacking flag so PlayerMovement can lock movement
        _G.player_is_attacking = (stateId ~= "idle" and stateId ~= "dash")

        -- Update combo chain tracking
        if stateId ~= "idle" and stateId ~= "dash" then
            table.insert(self._comboChain, stateId)
        else
            self._comboChain = {}
        end

        -- ===============================
        -- UPDATE ANIMATOR PARAMETERS
        -- ===============================
        self._animator:SetInt("ComboStep", newState.animParam)  -- Changed from SetInteger
        
        -- Set state bools
        if stateId == "heavy_charge" then
            self._animator:SetBool("IsHeavyCharging", true)
            self._animator:SetBool("IsAttacking", false)
        elseif stateId == "idle" or stateId == "dash" then
            self._animator:SetBool("IsAttacking", false)
            self._animator:SetBool("IsHeavyCharging", false)
        else
            self._animator:SetBool("IsAttacking", true)
            self._animator:SetBool("IsHeavyCharging", false)
        end
        
        -- Trigger transition (skip for idle and dash to avoid unnecessary triggers/SFX)
        if stateId ~= "idle" and stateId ~= "dash" then
            self._animator:SetTrigger("Attack")
            if stateId == "chain_attack" then
                self:_playRandomSFX(self.playerChainSFX)
            else
                self:_playRandomSFX(self.playerSlashSFX)
            end
        end

        -- Broadcast state change
        if event_bus then
            event_bus.publish("combat_state_changed", { 
                state = stateId,
                comboChain = self._comboChain
            })
        end

        print("[ComboManager] Transition: " .. oldState.id .. " → " .. stateId .. " (ComboStep: " .. newState.animParam .. ")")

        -- Call onEnter callback
        if newState.onEnter then
            local stateObj = {
                id = newState.id,
                timer = 0,
                damage = newState.damage,
                actualDamage = newState.actualDamage or newState.damage
            }
            newState.onEnter(self, stateObj, data)
        elseif stateId ~= "idle" and stateId ~= "dash" and stateId ~= "heavy_charge" then
            -- Default attack broadcast (if no custom onEnter)
            if event_bus then
                event_bus.publish("attack_performed", {
                    state = stateId,
                    damage = newState.damage,
                    knockback = newState.knockback or 0
                })
                
            end
        end
    end,

    -- ===============================
    -- SFX HELPERS
    -- ===============================
    _playRandomSFX = function(self, clips)
        local audio = self._playerAudio
        if not audio or not clips or #clips == 0 then return end
        audio:PlayOneShot(clips[math.random(1, #clips)])
    end,

    -- ===============================
    -- PUBLIC QUERY API
    -- ===============================
    GetCurrentState = function(self)
        return self._currentStateId
    end,

    IsAttacking = function(self)
        return self._currentStateData.damage > 0
    end,

    CanMove = function(self)
        return self._currentStateData.canMove
    end,

    GetCurrentComboChain = function(self)
        return self._comboChain
    end,
}