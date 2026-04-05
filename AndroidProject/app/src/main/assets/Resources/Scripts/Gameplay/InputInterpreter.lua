--[[
================================================================================
INPUT INTERPRETER
================================================================================
PURPOSE:
    Centralized input polling and buffering system.
    Converts raw engine input into gameplay-ready state with input memory.

SINGLE RESPONSIBILITY: Read and buffer raw input. Nothing else.

RESPONSIBILITIES:
    - Poll the unified input system each frame
    - Maintain input buffers (remembers recent presses for combo responsiveness)
    - Track hold durations (distinguish taps from holds)
    - Expose a clean query API for all other systems to consume
    - Suppress all input during legitimate full-freeze states (cinematic, death)

NOT RESPONSIBLE FOR:
    - Knowing about game state (dashing, weapons, combo state)
    - Making decisions about what input means in context
    - Publishing gameplay events
    - Blocking specific inputs based on what the player is doing

FREEZE CONDITIONS (appropriate at input level — suppress everything):
    - cinematic.active : full cinematic freeze
    - playerDead       : player cannot act

UI CLICK-THROUGH GUARD:
    If a UI button consumed an LMB press, we block the attack buffer on that
    same press so the click doesn't trigger a combat action. Cleared on release.

CONFIGURATION:
    INPUT_BUFFER_FRAMES : How long to remember a press (frames). At 60fps,
                          8 frames ≈ 133ms — forgiving without feeling laggy.
    HOLD_THRESHOLD      : Seconds before a press is considered a hold.

PUBLIC API:
    -- Held this frame
    IsAttackPressed()  IsChainPressed()  IsDashPressed()

    -- Pressed this frame
    IsAttackJustPressed()  IsChainJustPressed()  IsDashJustPressed()
    IsJumpJustPressed()

    -- Released this frame
    IsAttackJustReleased()  IsChainJustReleased()  IsDashJustReleased()

    -- Hold detection (past HOLD_THRESHOLD)
    IsAttackHeld()  IsChainHeld()  IsDashHeld()

    -- Raw hold durations (seconds)
    GetAttackHoldTime()  GetChainHoldTime()  GetDashHoldTime()

    -- Consumable buffers (combat system reads then consumes)
    HasBufferedAttack()  HasBufferedChain()  HasBufferedDash()
    ConsumeBufferedAttack()  ConsumeBufferedChain()  ConsumeBufferedDash()

    -- Movement
    GetMovementAxis()  IsMoving()

    -- Advanced sequence detection
    WasInputSequence(inputName, frameGaps)

DEPENDENCIES:
    - _G.Input  (engine unified input system)
    - _G.event_bus

AUTHOR: Soh Wei Jie
VERSION: 2.0
================================================================================
--]]

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local Input = _G.Input

return Component {
    fields = {
        INPUT_BUFFER_FRAMES = 8,
        HOLD_THRESHOLD      = 0.2,
    },

    Awake = function(self)
        _G.InputInterpreter = self

        -- ── Full-freeze flags (suppress ALL input) ─────────────────────────
        self._frozenByCinematic = false
        self._playerDead        = false

        if _G.event_bus and _G.event_bus.subscribe then
            self._cinematicSub = _G.event_bus.subscribe("cinematic.active", function(active)
                self._frozenByCinematic = active or false
            end)

            self._playerDeadSub = _G.event_bus.subscribe("playerDead", function(dead)
                self._playerDead = dead or false
            end)

            self._respawnPlayerSub = _G.event_bus.subscribe("respawnPlayer", function()
                self._playerDead = false
            end)
        end

        -- ── UI click-through guard ─────────────────────────────────────────
        -- Prevents an LMB click on a UI element from also buffering an attack.
        self._uiButtonPressed = false
        if _G.event_bus and _G.event_bus.subscribe then
            self._uiButtonPressedSub = _G.event_bus.subscribe("uiButtonPressed", function(pressed)
                self._uiButtonPressed = pressed or false
            end)
        end

        -- ── Current frame state ───────────────────────────────────────────
        self._currentFrame = {
            attack            = false,
            attackJustPressed  = false,
            attackJustReleased = false,

            chain            = false,
            chainJustPressed  = false,
            chainJustReleased = false,

            dash            = false,
            dashJustPressed  = false,
            dashJustReleased = false,

            jumpJustPressed = false,

            movement = { x = 0, y = 0 },
            isMoving = false,
        }

        -- ── Input history (for sequence / double-tap detection) ───────────
        self._inputHistory = {
            attack = {},
            chain  = {},
            dash   = {},
        }

        -- ── Hold timers ───────────────────────────────────────────────────
        self._holdTimers = {
            attack = 0,
            chain  = 0,
            dash   = 0,
        }

        -- ── Consumable buffers (frame countdown) ──────────────────────────
        self._bufferedInputs = {
            attack = 0,
            chain  = 0,
            dash   = 0,
        }

        self._frameCount = 0
    end,

    Update = function(self, dt)
        if not Input then return end

        -- Full freeze: clear frame state but still decay buffers so stale
        -- inputs don't pile up and fire the moment the freeze lifts.
        if self._frozenByCinematic or self._playerDead then
            self:_clearFrame()
            return
        end

        if Time.IsPaused() then return end

        self._frameCount = self._frameCount + 1

        -- ══════════════════════════════════════════════════════════════════
        -- POLL RAW ENGINE INPUT
        -- ══════════════════════════════════════════════════════════════════
        local attackPressed       = Input.IsActionHeld("Attack")
        local attackJustPressed   = Input.IsActionPressed("Attack")
        local attackJustReleased  = Input.IsActionJustReleased("Attack")

        local chainPressed        = Input.IsActionHeld("ChainAttack")
        local chainJustPressed    = Input.IsActionPressed("ChainAttack")
        local chainJustReleased   = Input.IsActionJustReleased("ChainAttack")

        local dashPressed         = Input.IsActionHeld("Dash")
        local dashJustPressed     = Input.IsActionPressed("Dash")
        local dashJustReleased    = Input.IsActionJustReleased("Dash")

        local jumpJustPressed     = Input.IsActionPressed("Jump")

        local movementAxis        = Input.GetAxis("Movement") or { x = 0, y = 0 }

        -- ══════════════════════════════════════════════════════════════════
        -- UPDATE CURRENT FRAME STATE
        -- ══════════════════════════════════════════════════════════════════
        self._currentFrame.attack            = attackPressed
        self._currentFrame.attackJustPressed  = attackJustPressed
        self._currentFrame.attackJustReleased = attackJustReleased

        self._currentFrame.chain            = chainPressed
        self._currentFrame.chainJustPressed  = chainJustPressed
        self._currentFrame.chainJustReleased = chainJustReleased

        self._currentFrame.dash            = dashPressed
        self._currentFrame.dashJustPressed  = dashJustPressed
        self._currentFrame.dashJustReleased = dashJustReleased

        self._currentFrame.jumpJustPressed  = jumpJustPressed

        -- Convert Vector2D userdata to a plain Lua table
        self._currentFrame.movement = {
            x = movementAxis.x or 0,
            y = movementAxis.y or 0,
        }
        self._currentFrame.isMoving = (
            math.abs(movementAxis.x) > 0.1 or math.abs(movementAxis.y) > 0.1
        )

        -- ══════════════════════════════════════════════════════════════════
        -- INPUT BUFFERING
        -- Buffers are ONLY guarded by the UI click-through flag.
        -- All other context (dashing, attacking, weapon) is for ComboManager
        -- to evaluate — it is not InputInterpreter's concern.
        -- ══════════════════════════════════════════════════════════════════
        if attackJustPressed then
            if not self._uiButtonPressed then
                self._bufferedInputs.attack = self.INPUT_BUFFER_FRAMES
                table.insert(self._inputHistory.attack, self._frameCount)
            end
        end

        -- Clear the UI guard once the attack button is released
        if attackJustReleased and self._uiButtonPressed then
            self._uiButtonPressed = false
        end

        if chainJustPressed then
            self._bufferedInputs.chain = self.INPUT_BUFFER_FRAMES
            table.insert(self._inputHistory.chain, self._frameCount)
        end

        if dashJustPressed then
            self._bufferedInputs.dash = self.INPUT_BUFFER_FRAMES
            table.insert(self._inputHistory.dash, self._frameCount)
        end

        -- Decay buffered inputs by one frame each tick
        self._bufferedInputs.attack = math.max(0, self._bufferedInputs.attack - 1)
        self._bufferedInputs.chain  = math.max(0, self._bufferedInputs.chain  - 1)
        self._bufferedInputs.dash   = math.max(0, self._bufferedInputs.dash   - 1)

        -- ══════════════════════════════════════════════════════════════════
        -- HOLD TRACKING
        -- ══════════════════════════════════════════════════════════════════
        self._holdTimers.attack = attackPressed and (self._holdTimers.attack + dt) or 0
        self._holdTimers.chain  = chainPressed  and (self._holdTimers.chain  + dt) or 0
        self._holdTimers.dash   = dashPressed   and (self._holdTimers.dash   + dt) or 0

        -- ══════════════════════════════════════════════════════════════════
        -- CLEAN UP OLD HISTORY
        -- ══════════════════════════════════════════════════════════════════
        self:_cleanHistory(self._inputHistory.attack)
        self:_cleanHistory(self._inputHistory.chain)
        self:_cleanHistory(self._inputHistory.dash)
    end,

    -- ══════════════════════════════════════════════════════════════════════
    -- INTERNAL HELPERS
    -- ══════════════════════════════════════════════════════════════════════

    -- Zero the frame state while still decaying buffers (called during freeze).
    _clearFrame = function(self)
        self._currentFrame.attack            = false
        self._currentFrame.attackJustPressed  = false
        self._currentFrame.attackJustReleased = false
        self._currentFrame.chain            = false
        self._currentFrame.chainJustPressed  = false
        self._currentFrame.chainJustReleased = false
        self._currentFrame.dash            = false
        self._currentFrame.dashJustPressed  = false
        self._currentFrame.dashJustReleased = false
        self._currentFrame.jumpJustPressed  = false
        self._currentFrame.movement = { x = 0, y = 0 }
        self._currentFrame.isMoving = false

        -- Still decay so stale inputs don't fire the moment the freeze lifts
        self._bufferedInputs.attack = math.max(0, self._bufferedInputs.attack - 1)
        self._bufferedInputs.chain  = math.max(0, self._bufferedInputs.chain  - 1)
        self._bufferedInputs.dash   = math.max(0, self._bufferedInputs.dash   - 1)
    end,

    _cleanHistory = function(self, history)
        local cutoff = self._frameCount - self.INPUT_BUFFER_FRAMES * 2
        while #history > 0 and history[1] < cutoff do
            table.remove(history, 1)
        end
    end,

    -- ══════════════════════════════════════════════════════════════════════
    -- PUBLIC QUERY API
    -- ══════════════════════════════════════════════════════════════════════

    -- Held this frame
    IsAttackPressed  = function(self) return self._currentFrame.attack end,
    IsChainPressed   = function(self) return self._currentFrame.chain  end,
    IsDashPressed    = function(self) return self._currentFrame.dash   end,

    -- Pressed this frame
    IsAttackJustPressed  = function(self) return self._currentFrame.attackJustPressed  end,
    IsChainJustPressed   = function(self) return self._currentFrame.chainJustPressed   end,
    IsDashJustPressed    = function(self) return self._currentFrame.dashJustPressed    end,
    IsJumpJustPressed    = function(self) return self._currentFrame.jumpJustPressed    end,

    -- Released this frame
    IsAttackJustReleased = function(self) return self._currentFrame.attackJustReleased end,
    IsChainJustReleased  = function(self) return self._currentFrame.chainJustReleased  end,
    IsDashJustReleased   = function(self) return self._currentFrame.dashJustReleased   end,

    -- Hold detection (past HOLD_THRESHOLD)
    IsAttackHeld = function(self) return self._holdTimers.attack >= self.HOLD_THRESHOLD end,
    IsChainHeld  = function(self) return self._holdTimers.chain  >= self.HOLD_THRESHOLD end,
    IsDashHeld   = function(self) return self._holdTimers.dash   >= self.HOLD_THRESHOLD end,

    -- Raw hold durations (seconds)
    GetAttackHoldTime = function(self) return self._holdTimers.attack end,
    GetChainHoldTime  = function(self) return self._holdTimers.chain  end,
    GetDashHoldTime   = function(self) return self._holdTimers.dash   end,

    -- Buffered inputs — ComboManager reads these, then immediately consumes them
    HasBufferedAttack = function(self) return self._bufferedInputs.attack > 0 end,
    HasBufferedChain  = function(self) return self._bufferedInputs.chain  > 0 end,
    HasBufferedDash   = function(self) return self._bufferedInputs.dash   > 0 end,

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

    -- Movement
    GetMovementAxis = function(self) return self._currentFrame.movement end,
    IsMoving        = function(self) return self._currentFrame.isMoving  end,

    -- Sequence detection. frameGaps is an array of maximum inter-press gaps.
    -- Example: WasInputSequence("attack", {10}) checks for 2 presses within 10 frames.
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