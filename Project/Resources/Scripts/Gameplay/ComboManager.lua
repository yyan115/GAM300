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
    - Trigger animations and broadcast damage events
    - Support diverse combo types (tap, hold, charge, special inputs)

ADDING NEW COMBOS:
    Modify the COMBO_TREE table in Awake(). Each node defines:
    - id: Unique state identifier
    - anim: Animation clip ID to play
    - duration: Animation length (seconds)
    - damage: Base damage value
    - canMove: Whether player can move during this state
    - comboWindow: Time window to input next attack (nil = no combo)
    - onEnter: Optional callback when state begins
    - onUpdate: Optional callback during state (e.g., charging)
    - onExit: Optional callback when state ends
    - transitions: Table of {inputType → nextStateId} for combo paths
    
    Example - Add a new 4-hit combo:
        {
            id = "light_4",
            anim = 13,
            duration = 0.7,
            damage = 25,
            canMove = false,
            comboWindow = 0.5,
            transitions = {
                attack = "light_finisher"  -- Press attack → go to finisher
            }
        }

COMBO TREE STRUCTURE:
    - Start from "idle" state
    - Each attack creates a new node in the tree
    - Transitions define valid combo paths
    - Supports branching (e.g., attack→light or chain→special)

USAGE:
    local comboMgr = self:GetComponent("ComboManager")
    
    if comboMgr:CanMove() then
        -- Process movement
    end
    
    local state = comboMgr:GetCurrentState()
    if comboMgr:IsAttacking() then
        -- Player is mid-combo, apply hit lag or effects
    end

CONFIGURATION:
    Edit COMBO_TREE in Awake() to add/modify combos
    Edit fields (DefaultComboWindow, HeavyChargeTime, DashDuration) in editor

DEPENDENCIES:
    - Requires InputInterpreter (accessed via _G.InputInterpreter singleton)
    - Requires AnimationComponent for attack animations
    - Uses event_bus for broadcasting combat events

EVENTS PUBLISHED:
    - combat_state_changed: {state = stateId}
    - attack_performed: {state, damage, chargePercent?, ...}
    - dash_performed: {}

PUBLIC API:
    GetCurrentState() -> string (current combo state ID)
    IsAttacking() -> bool (true if in any attack state)
    CanMove() -> bool (true if player can move)
    GetCurrentComboChain() -> table (list of state IDs in current combo)

NOTES:
    - Input buffering is handled by InputInterpreter, not here
    - State transitions are deterministic based on combo tree
    - Each attack state can define custom logic via callbacks
    - Heavy attacks use onUpdate for charging mechanics

AUTHOR: Soh Wei Jie
VERSION: 2.0 (Data-Driven)
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
        DashDuration = 0.3,          -- Dash length
    },

    Awake = function(self)
        -- ===============================
        -- COMBO TREE DEFINITION
        -- ===============================
        -- HOW TO ADD NEW COMBOS:
        -- 1. Add a new entry to this table
        -- 2. Set transitions to define combo paths
        -- 3. Animations and damage are automatically applied
        
        self.COMBO_TREE = {
            -- IDLE STATE (starting point)
            idle = {
                id = "idle",
                anim = 0,
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
                anim = 10,
                duration = 0.4,
                damage = 10,
                canMove = false,
                comboWindow = 0.5,
                transitions = {
                    attack = "light_2",
                    chain = "chain_attack"  -- Can cancel into chain
                }
            },

            light_2 = {
                id = "light_2",
                anim = 11,
                duration = 0.45,
                damage = 12,
                canMove = false,
                comboWindow = 0.5,
                transitions = {
                    attack = "light_3",
                    chain = "chain_attack"
                }
            },

            light_3 = {
                id = "light_3",
                anim = 12,
                duration = 0.6,
                damage = 20,
                canMove = false,
                comboWindow = nil,  -- Finisher - no combo continuation
                transitions = {}
            },

            -- HEAVY ATTACK (hold-release)
            heavy_charge = {
                id = "heavy_charge",
                anim = 20,
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
                anim = 21,
                duration = 0.8,
                damage = 30,  -- Will be multiplied by chargePercent
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
                anim = 30,
                duration = 0.5,
                damage = 25,
                canMove = false,
                comboWindow = 0.5,
                transitions = {
                    attack = "light_1"  -- Chain can combo into lights
                }
            },

            -- DASH
            dash = {
                id = "dash",
                anim = 40,
                duration = 0.3,
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

            -- ===============================
            -- EXAMPLE: ADD YOUR OWN COMBOS HERE
            -- ===============================
            -- Uncomment and modify to add a new combo path:
            --[[
            light_4 = {
                id = "light_4",
                anim = 13,
                duration = 0.7,
                damage = 25,
                canMove = false,
                comboWindow = 0.5,
                transitions = {
                    attack = "special_finisher"
                }
            },
            
            special_finisher = {
                id = "special_finisher",
                anim = 50,
                duration = 1.0,
                damage = 50,
                canMove = false,
                comboWindow = nil,
                transitions = {}
            }
            --]]
        }

        -- ===============================
        -- SPECIAL INPUT SEQUENCES
        -- ===============================
        -- Define complex input combos (e.g., quarter-circle + attack)
        -- Format: {name, sequence of {inputType, maxFrames}, resultState}
        self.SPECIAL_INPUTS = {
            -- Example: Double-tap attack within 10 frames = special move
            -- {name = "double_tap_attack", sequence = {{"attack", 10}, {"attack", 0}}, state = "special_move"}
        }

        -- ===============================
        -- RUNTIME STATE
        -- ===============================
        self._inputInterpreter = nil
        self._animator = nil
        
        self._currentStateId = "idle"
        self._currentStateData = self.COMBO_TREE["idle"]
        self._stateTimer = 0
        self._comboChain = {}  -- Track combo sequence for UI/scoring
    end,

    Start = function(self)
        -- Get InputInterpreter from global singleton
        self._inputInterpreter = _G.InputInterpreter
        if not self._inputInterpreter then
            print("[ComboManager] ERROR: InputInterpreter not found! Make sure it's on an entity that loads before ComboManager.")
        end

        self._animator = self:GetComponent("AnimationComponent")
        if not self._animator then
            print("[ComboManager] ERROR: AnimationComponent not found!")
        end
    end,

    Update = function(self, dt)
        if not self._inputInterpreter or not self._animator then return end

        self._stateTimer = self._stateTimer + dt
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
        -- CHECK FOR TRANSITIONS
        -- ===============================
        local input = self._inputInterpreter
        local nextState = nil
        
        -- Determine input type
        if input:HasBufferedAttack() then
            if input:IsAttackHeld() then
                nextState = state.transitions.attack_hold
            else
                nextState = state.transitions.attack
            end
            
            if nextState then
                input:ConsumeBufferedAttack()
            end
        elseif input:HasBufferedChain() then
            nextState = state.transitions.chain
            if nextState then
                input:ConsumeBufferedChain()
            end
        elseif input:HasBufferedDash() then
            nextState = state.transitions.dash
            if nextState then
                input:ConsumeBufferedDash()
            end
        end

        -- Only allow transitions within combo window
        if nextState and state.comboWindow then
            if self._stateTimer <= state.comboWindow then
                self:_transitionTo(nextState)
                return
            end
        elseif nextState and not state.comboWindow and state.id == "idle" then
            -- Idle state always accepts input
            self:_transitionTo(nextState)
            return
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

        -- Exit current state
        local oldState = self._currentStateData
        if oldState.onExit then
            oldState.onExit(self, { id = oldState.id, timer = self._stateTimer }, data)
        end

        -- Enter new state
        self._currentStateId = stateId
        self._currentStateData = newState
        self._stateTimer = 0

        -- Update combo chain tracking
        if stateId ~= "idle" and stateId ~= "dash" then
            table.insert(self._comboChain, stateId)
        else
            self._comboChain = {}
        end

        -- Play animation
     --   if self._animator and newState.anim then
     --       local loop = (stateId == "idle" or stateId == "heavy_charge")
     --      self._animator:PlayClip(newState.anim, loop)
     -- end

        -- Broadcast state change
        if event_bus then
            event_bus.publish("combat_state_changed", { 
                state = stateId,
                comboChain = self._comboChain
            })
        end

        -- Debug: Print current attack state
        print("[ComboManager] Current Attack: " .. stateId .. " (Anim ID: " .. tostring(newState.anim) .. ")")

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
                    damage = newState.damage
                })
            end
        end

        print("[ComboManager] Transition: " .. oldState.id .. " → " .. stateId)
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