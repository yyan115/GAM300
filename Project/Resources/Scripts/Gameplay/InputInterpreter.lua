-- InputInterpreter (fixed for duplicated inputs / defensive debouncing)
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local Input = _G.Input

return Component {
    fields = {
        INPUT_BUFFER_FRAMES = 8,  -- frames to remember button presses
        HOLD_THRESHOLD = 0.2,     -- seconds before a press becomes a hold
        MIN_PRESS_DEBOUNCE = 0.05 -- seconds: ignore presses closer than this (engine noise)
    },

    _publish_event = function(self, topic, payload)
        if type(_G.event_bus) == "table" then
            local ok, res = pcall(function()
                if type(_G.event_bus.publish) == "function" then return _G.event_bus.publish(topic, payload) end
                if type(_G.event_bus.dispatch) == "function" then return _G.event_bus.dispatch(topic, payload) end
                if type(_G.event_bus.send) == "function" then return _G.event_bus.send(topic, payload) end
                if type(_G.event_bus.emit) == "function" then return _G.event_bus.emit(topic, payload) end
                if type(_G.event_bus.publishEvent) == "function" then return _G.event_bus.publishEvent(topic, payload) end
                return nil
            end)
            if ok then return true end
        end
        return false
    end,

    Awake = function(self)
        _G.InputInterpreter = self

        self._currentFrame = {
            attack = false, attackJustPressed = false, attackJustReleased = false,
            chain = false, chainJustPressed = false, chainJustReleased = false,
            dash = false, dashJustPressed = false, dashJustReleased = false,
            movement = { x = 0, y = 0 }, isMoving = false
        }

        self._inputHistory = { attack = {}, chain = {}, dash = {} }
        self._holdTimers = { attack = 0, chain = 0, dash = 0 }
        self._bufferedInputs = { attack = 0, chain = 0, dash = 0 }
        self._holdPublished = { attack = false, chain = false, dash = false }

        -- Defensive state to prevent duplicate processing
        self._lastPressTime = { attack = -math.huge, chain = -math.huge, dash = -math.huge }
        self._lastReleaseTime = { attack = -math.huge, chain = -math.huge, dash = -math.huge }
        self._lastPublishedFrame = {} -- keyed by topic -> frame
        self._frameCount = 0
    end,

    Update = function(self, dt)
        if not Input then return end
        self._frameCount = self._frameCount + 1

        local now = os.clock()

        -- POLL ENGINE INPUT
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

        -- DEBOUNCE AND DEDUPLICATE: only treat a just-press if enough time passed since last recorded press
        local function treatJustPressed(key, flag)
            if not flag then return false end
            if (now - (self._lastPressTime[key] or -math.huge)) < self.MIN_PRESS_DEBOUNCE then
                return false
            end
            self._lastPressTime[key] = now
            return true
        end

        local function treatJustReleased(key, flag)
            if not flag then return false end
            if (now - (self._lastReleaseTime[key] or -math.huge)) < self.MIN_PRESS_DEBOUNCE then
                return false
            end
            self._lastReleaseTime[key] = now
            return true
        end

        attackJustPressed = treatJustPressed("attack", attackJustPressed)
        chainJustPressed = treatJustPressed("chain", chainJustPressed)
        dashJustPressed = treatJustPressed("dash", dashJustPressed)

        attackJustReleased = treatJustReleased("attack", attackJustReleased)
        chainJustReleased = treatJustReleased("chain", chainJustReleased)
        dashJustReleased = treatJustReleased("dash", dashJustReleased)

        -- UPDATE CURRENT FRAME STATE
        self._currentFrame.attack = attackPressed
        self._currentFrame.attackJustPressed = attackJustPressed
        self._currentFrame.attackJustReleased = attackJustReleased

        self._currentFrame.chain = chainPressed
        self._currentFrame.chainJustPressed = chainJustPressed
        self._currentFrame.chainJustReleased = chainJustReleased

        self._currentFrame.dash = dashPressed
        self._currentFrame.dashJustPressed = dashJustPressed
        self._currentFrame.dashJustReleased = dashJustReleased

        self._currentFrame.movement = { x = movementAxis.x or 0, y = movementAxis.y or 0 }
        self._currentFrame.isMoving = (math.abs(movementAxis.x) > 0.1 or math.abs(movementAxis.y) > 0.1)

        -- INPUT BUFFERING: only set buffer and record history if this is a new recorded press
        local function recordBufferedInput(key, justPressedFlag)
            if not justPressedFlag then return end
            local hist = self._inputHistory[key]
            local lastFrame = hist[#hist]
            if lastFrame == self._frameCount then
                return
            end
            self._bufferedInputs[key] = self.INPUT_BUFFER_FRAMES
            table.insert(hist, self._frameCount)
        end

        recordBufferedInput("attack", attackJustPressed)
        recordBufferedInput("chain", chainJustPressed)
        recordBufferedInput("dash", dashJustPressed)

        -- Decay buffered inputs (frames)
        self._bufferedInputs.attack = math.max(0, self._bufferedInputs.attack - 1)
        self._bufferedInputs.chain = math.max(0, self._bufferedInputs.chain - 1)
        self._bufferedInputs.dash = math.max(0, self._bufferedInputs.dash - 1)

        -- HOLD TRACKING
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
        
        -- PUBLISH events once per real press/release (guard by last published frame)
        if attackJustPressed then
            if self._lastPublishedFrame["attack.down"] ~= self._frameCount then
                self:_publish_event("attack.down", { frame = self._frameCount })
                self._lastPublishedFrame["attack.down"] = self._frameCount
            end
        end

        if attackJustReleased then
            if self._lastPublishedFrame["attack.up"] ~= self._frameCount then
                self:_publish_event("attack.up", { frame = self._frameCount })
                self._lastPublishedFrame["attack.up"] = self._frameCount
            end
            self._holdPublished.attack = false
        end

        if self._holdTimers.attack >= self.HOLD_THRESHOLD and not self._holdPublished.attack then
            self._holdPublished.attack = true
            self:_publish_event("attack.hold", { holdTime = self._holdTimers.attack })
        end

        if chainJustPressed then
            if self._lastPublishedFrame["chain.down"] ~= self._frameCount then
                self:_publish_event("chain.down", { frame = self._frameCount })
                self._lastPublishedFrame["chain.down"] = self._frameCount
            end
            self._holdPublished.chain = false
        end

        if chainJustReleased then
            if self._lastPublishedFrame["chain.up"] ~= self._frameCount then
                self:_publish_event("chain.up", { frame = self._frameCount })
                self._lastPublishedFrame["chain.up"] = self._frameCount
            end
            self._holdPublished.chain = false
        end

        if self._holdTimers.chain >= self.HOLD_THRESHOLD and not self._holdPublished.chain then
            self._holdPublished.chain = true
            self:_publish_event("chain.hold", { holdTime = self._holdTimers.chain })
        end

        if dashJustPressed then
            if self._lastPublishedFrame["dash.down"] ~= self._frameCount then
                self:_publish_event("dash.down", { frame = self._frameCount })
                self._lastPublishedFrame["dash.down"] = self._frameCount
            end
        end

        if dashJustReleased then
            if self._lastPublishedFrame["dash.up"] ~= self._frameCount then
                self:_publish_event("dash.up", { frame = self._frameCount })
                self._lastPublishedFrame["dash.up"] = self._frameCount
            end
            self._holdPublished.dash = false
        end

        if self._holdTimers.dash >= self.HOLD_THRESHOLD and not self._holdPublished.dash then
            self._holdPublished.dash = true
            self:_publish_event("dash.hold", { holdTime = self._holdTimers.dash })
        end

        -- CLEANUP OLD HISTORY
        self:_cleanHistory(self._inputHistory.attack)
        self:_cleanHistory(self._inputHistory.chain)
        self:_cleanHistory(self._inputHistory.dash)
    end,

    _cleanHistory = function(self, history)
        local cutoff = self._frameCount - self.INPUT_BUFFER_FRAMES * 2
        while #history > 0 and history[1] < cutoff do
            table.remove(history, 1)
        end
    end,

    -- PUBLIC API (unchanged interface)
    IsAttackPressed = function(self) return self._currentFrame.attack end,
    IsChainPressed = function(self) return self._currentFrame.chain end,
    IsDashPressed = function(self) return self._currentFrame.dash end,

    IsAttackJustPressed = function(self) return self._currentFrame.attackJustPressed end,
    IsChainJustPressed = function(self) return self._currentFrame.chainJustPressed end,
    IsDashJustPressed = function(self) return self._currentFrame.dashJustPressed end,

    IsAttackJustReleased = function(self) return self._currentFrame.attackJustReleased end,
    IsChainJustReleased = function(self) return self._currentFrame.chainJustReleased end,
    IsDashJustReleased = function(self) return self._currentFrame.dashJustReleased end,

    IsAttackHeld = function(self) return self._holdTimers.attack >= self.HOLD_THRESHOLD end,
    IsChainHeld = function(self) return self._holdTimers.chain >= self.HOLD_THRESHOLD end,
    IsDashHeld = function(self) return self._holdTimers.dash >= self.HOLD_THRESHOLD end,

    GetAttackHoldTime = function(self) return self._holdTimers.attack end,
    GetChainHoldTime = function(self) return self._holdTimers.chain end,
    GetDashHoldTime = function(self) return self._holdTimers.dash end,

    HasBufferedAttack = function(self) return self._bufferedInputs.attack > 0 end,
    HasBufferedChain = function(self) return self._bufferedInputs.chain > 0 end,
    HasBufferedDash = function(self) return self._bufferedInputs.dash > 0 end,

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

    GetMovementAxis = function(self) return self._currentFrame.movement end,
    IsMoving = function(self) return self._currentFrame.isMoving end,

    WasInputSequence = function(self, inputName, frameGaps)
        local history = self._inputHistory[inputName]
        if not history or #history < #frameGaps + 1 then return false end
        local startIdx = #history - #frameGaps
        for i = 1, #frameGaps do
            local gap = history[startIdx + i] - history[startIdx + i - 1]
            if gap > frameGaps[i] then return false end
        end
        return true
    end,
}
