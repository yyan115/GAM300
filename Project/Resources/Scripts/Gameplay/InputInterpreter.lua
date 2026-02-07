--[[
================================================================================
INPUT INTERPRETER
================================================================================
PURPOSE:
    Centralized input polling and buffering system for fighting game mechanics.
    Converts raw engine input into gameplay-ready state with input memory.

RESPONSIBILITIES:
    - Poll unified input system each frame (Attack, Chain, Dash, Movement)
    - Maintain input buffer (remembers recent presses for combo responsiveness)
    - Track hold duration (distinguish taps from holds)
    - Expose clean query API for combat/movement systems
    - Support consumable buffers (prevent double-execution)

USAGE:
    local inputInterp = self:GetComponent("InputInterpreter")
    
    if inputInterp:HasBufferedAttack() then
        inputInterp:ConsumeBufferedAttack()
        -- Execute combo continuation
    end
    
    if inputInterp:IsAttackHeld() then
        -- Charge heavy attack
    end

CONFIGURATION:
    INPUT_BUFFER_FRAMES: How long to remember button presses (frames)
    HOLD_THRESHOLD: Seconds before a press becomes a hold

DEPENDENCIES:
    - Requires _G.Input (engine's unified input system)
    - Uses Input.IsActionPressed/JustPressed/JustReleased
    - Uses Input.GetAxis for movement

PUBLIC API:
    -- Current state queries
    IsAttackPressed(), IsChainPressed(), IsDashPressed()
    IsAttackJustPressed(), IsChainJustPressed(), IsDashJustPressed()
    IsAttackJustReleased(), IsChainJustReleased(), IsDashJustReleased()
    
    -- Hold detection
    IsAttackHeld(), IsChainHeld(), IsDashHeld()
    GetAttackHoldTime(), GetChainHoldTime(), GetDashHoldTime()
    
    -- Buffered inputs (combo system)
    HasBufferedAttack(), HasBufferedChain(), HasBufferedDash()
    ConsumeBufferedAttack(), ConsumeBufferedChain(), ConsumeBufferedDash()
    
    -- Movement
    GetMovementAxis(), IsMoving()
    
    -- Advanced
    WasInputSequence(inputName, frameGaps) -- For double-tap detection

NOTES:
    - Input buffering makes combos feel responsive (player doesn't need frame-perfect timing)
    - Consumable buffers prevent buffered input from triggering multiple actions
    - Hold detection threshold should match game feel (0.2s = fast, 0.5s = deliberate)

AUTHOR: Soh Wei Jie
VERSION: 1.0
================================================================================
--]]

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local Input = _G.Input

return Component {
    fields = {
        INPUT_BUFFER_FRAMES = 8,  -- How long to remember button presses (at 60fps = ~133ms)
        HOLD_THRESHOLD = 0.2,     -- Seconds before press becomes hold
    },

    Awake = function(self)
        -- Register as global singleton (only one player input system)
        _G.InputInterpreter = self
        
        -- Current frame states (updated every frame)
        self._currentFrame = {
            attack = false,
            attackJustPressed = false,
            attackJustReleased = false,
            
            chain = false,
            chainJustPressed = false,
            chainJustReleased = false,
            
            dash = false,
            dashJustPressed = false,
            dashJustReleased = false,
            
            movement = { x = 0, y = 0 },
            isMoving = false
        }

        -- Input history for buffering
        self._inputHistory = {
            attack = {},
            chain = {},
            dash = {}
        }

        -- Hold tracking
        self._holdTimers = {
            attack = 0,
            chain = 0,
            dash = 0
        }

        -- Consumable buffer (for combo system to "eat" inputs)
        self._bufferedInputs = {
            attack = 0,  -- frames remaining
            chain = 0,
            dash = 0
        }

        self._frameCount = 0
    end,

    Update = function(self, dt)
        if not Input then return end

        self._frameCount = self._frameCount + 1

        -- ===============================
        -- POLL ENGINE INPUT
        -- ===============================
        local attackPressed = Input.IsActionPressed("Attack")
        local attackJustPressed = Input.IsActionJustPressed("Attack")
        local attackJustReleased = Input.IsActionJustReleased("Attack")

        local chainPressed = Input.IsActionPressed("ChainAttack")
        local chainJustPressed = Input.IsActionJustPressed("ChainAttack")
        local chainJustReleased = Input.IsActionJustReleased("ChainAttack")

        local dashPressed = Input.IsActionPressed("Dash")
        local dashJustPressed = Input.IsActionJustPressed("Dash")
        local dashJustReleased = Input.IsActionJustReleased("Dash")

        local movementAxis = Input.GetAxis("Movement") or { x = 0, y = 0 }

        -- ===============================
        -- UPDATE CURRENT FRAME STATE
        -- ===============================
        self._currentFrame.attack = attackPressed
        self._currentFrame.attackJustPressed = attackJustPressed
        self._currentFrame.attackJustReleased = attackJustReleased

        self._currentFrame.chain = chainPressed
        self._currentFrame.chainJustPressed = chainJustPressed
        self._currentFrame.chainJustReleased = chainJustReleased

        self._currentFrame.dash = dashPressed
        self._currentFrame.dashJustPressed = dashJustPressed
        self._currentFrame.dashJustReleased = dashJustReleased

        -- Convert Vector2D userdata to plain Lua table for serializability
        self._currentFrame.movement = {
            x = movementAxis.x or 0,
            y = movementAxis.y or 0
        }
        self._currentFrame.isMoving = (math.abs(movementAxis.x) > 0.1 or math.abs(movementAxis.y) > 0.1)

        -- ===============================
        -- INPUT BUFFERING
        -- ===============================
        if attackJustPressed then
            self._bufferedInputs.attack = self.INPUT_BUFFER_FRAMES
            table.insert(self._inputHistory.attack, self._frameCount)
        end

        if chainJustPressed then
            self._bufferedInputs.chain = self.INPUT_BUFFER_FRAMES
            table.insert(self._inputHistory.chain, self._frameCount)
        end

        if dashJustPressed then
            self._bufferedInputs.dash = self.INPUT_BUFFER_FRAMES
            table.insert(self._inputHistory.dash, self._frameCount)
        end

        -- Decay buffered inputs
        self._bufferedInputs.attack = math.max(0, self._bufferedInputs.attack - 1)
        self._bufferedInputs.chain = math.max(0, self._bufferedInputs.chain - 1)
        self._bufferedInputs.dash = math.max(0, self._bufferedInputs.dash - 1)

        -- ===============================
        -- HOLD TRACKING
        -- ===============================
        if attackPressed then
            self._holdTimers.attack = self._holdTimers.attack + dt
        else
            self._holdTimers.attack = 0
        end

        if chainPressed then
            self._holdTimers.chain = self._holdTimers.chain + dt
        else
            self._holdTimers.chain = 0
        end

        if dashPressed then
            self._holdTimers.dash = self._holdTimers.dash + dt
        else
            self._holdTimers.dash = 0
        end

        -- ===============================
        -- PUBLISH CHAIN EVENTS TO EVENT BUSS
        -- ===============================
        if _G.event_bus and _G.event_bus.publish then
            if chainJustPressed then
                _G.event_bus.publish("chain.down", {})
            end
            
            if chainJustReleased then
                _G.event_bus.publish("chain.up", {})
            end
            
            -- Publish hold event if held past threshold
            if self._holdTimers.chain >= self.HOLD_THRESHOLD and chainPressed then
                -- Only publish once when threshold is crossed, not every frame
                if not self._chainHoldPublished then
                    _G.event_bus.publish("chain.hold", {})
                    self._chainHoldPublished = true
                end
            else
                self._chainHoldPublished = false
            end
        end

        -- ===============================
        -- CLEANUP OLD HISTORY
        -- ===============================
        self:_cleanHistory(self._inputHistory.attack)
        self:_cleanHistory(self._inputHistory.chain)
        self:_cleanHistory(self._inputHistory.dash)
    end,

    -- Internal: Remove inputs older than buffer window
    _cleanHistory = function(self, history)
        local cutoff = self._frameCount - self.INPUT_BUFFER_FRAMES * 2
        while #history > 0 and history[1] < cutoff do
            table.remove(history, 1)
        end
    end,

    -- ===============================
    -- PUBLIC QUERY API
    -- ===============================

    -- Check if button is currently held
    IsAttackPressed = function(self) return self._currentFrame.attack end,
    IsChainPressed = function(self) return self._currentFrame.chain end,
    IsDashPressed = function(self) return self._currentFrame.dash end,

    -- Check if button was just pressed THIS frame
    IsAttackJustPressed = function(self) return self._currentFrame.attackJustPressed end,
    IsChainJustPressed = function(self) return self._currentFrame.chainJustPressed end,
    IsDashJustPressed = function(self) return self._currentFrame.dashJustPressed end,

    -- Check if button was just released THIS frame
    IsAttackJustReleased = function(self) return self._currentFrame.attackJustReleased end,
    IsChainJustReleased = function(self) return self._currentFrame.chainJustReleased end,
    IsDashJustReleased = function(self) return self._currentFrame.dashJustReleased end,

    -- Check if button is being held (past threshold)
    IsAttackHeld = function(self) return self._holdTimers.attack >= self.HOLD_THRESHOLD end,
    IsChainHeld = function(self) return self._holdTimers.chain >= self.HOLD_THRESHOLD end,
    IsDashHeld = function(self) return self._holdTimers.dash >= self.HOLD_THRESHOLD end,

    -- Get hold duration
    GetAttackHoldTime = function(self) return self._holdTimers.attack end,
    GetChainHoldTime = function(self) return self._holdTimers.chain end,
    GetDashHoldTime = function(self) return self._holdTimers.dash end,

    -- Buffered input (remembers recent presses even if released)
    HasBufferedAttack = function(self) return self._bufferedInputs.attack > 0 end,
    HasBufferedChain = function(self) return self._bufferedInputs.chain > 0 end,
    HasBufferedDash = function(self) return self._bufferedInputs.dash > 0 end,

    -- Consume buffered input (combo system should call this when using a buffered input)
    ConsumeBufferedAttack = function(self)
        if self._bufferedInputs.attack > 0 then
            self._bufferedInputs.attack = 0
            return true
        end
        return false
    end,

    ConsumeBufferedChain = function(self)
        if self._bufferedInputs.chain > 0 then
            self._bufferedInputs.chain = 0
            return true
        end
        return false
    end,

    ConsumeBufferedDash = function(self)
        if self._bufferedInputs.dash > 0 then
            self._bufferedInputs.dash = 0
            return true
        end
        return false
    end,

    -- Movement queries
    GetMovementAxis = function(self) return self._currentFrame.movement end,
    IsMoving = function(self) return self._currentFrame.isMoving end,

    -- Advanced: Check input sequence (e.g., double-tap detection)
    -- Example: WasInputSequence("attack", {0, 10}) checks if attack was pressed twice within 10 frames
    WasInputSequence = function(self, inputName, frameGaps)
        local history = self._inputHistory[inputName]
        if not history or #history < #frameGaps + 1 then
            return false
        end

        -- Check if last N presses match the timing pattern
        local startIdx = #history - #frameGaps
        for i = 1, #frameGaps do
            local gap = history[startIdx + i] - history[startIdx + i - 1]
            if gap > frameGaps[i] then
                return false
            end
        end

        return true
    end,
}