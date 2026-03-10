require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

--[[
    TutorialPrompts.lua
    Standalone tutorial script — attach to the TutorialDialogue entity.
    Listens for player actions and calls ScrollNext to advance dialogue entries.

    Dialogue setup (in editor):
        Dialogue Name: "Tutorial"
        Auto Start: true
        Appearance: Fade In / Out

        Entry 0: "Move around"              | Sprite: MovementControlsSprite | Advance By: Action
        Entry 1: "Try to Dash"              | Sprite: DashControlsSprite     | Advance By: Action
        (future entries added via editor for Slash, Chain, etc.)

    Inspector fields let you configure delays and which entry index maps to which action.
]]

return Component {
    fields = {
        dialogueName       = "Tutorial",

        -- Entry index → action mapping
        moveEntryIndex     = 0,       -- which entry waits for movement
        dashEntryIndex     = 1,       -- which entry waits for dash
        slashEntryIndex    = -1,      -- which entry waits for attack (-1 = unused)
        chainEntryIndex    = -1,      -- which entry waits for chain  (-1 = unused)

        -- Delay before dismissing (seconds after condition met)
        dismissDelay       = 1.0,
    },

    Start = function(self)
        self._dismissTimer   = 0
        self._conditionMet   = false
        self._lastIndex      = -1

        -- Track whether player has moved / dashed / attacked / chained
        self._playerMoved    = false
        self._playerDashed   = false
        self._playerAttacked = false
        self._playerChained  = false

        -- Subscribe to gameplay events for dash, attack, chain
        self._dashSub = event_bus.subscribe("dash_performed", function()
            self._playerDashed = true
        end)

        self._attackSub = event_bus.subscribe("attack_performed", function()
            self._playerAttacked = true
        end)

        self._chainThrowSub = event_bus.subscribe("player_chain_throw", function()
            self._playerChained = true
        end)

        self._chainPullSub = event_bus.subscribe("player_chain_pull", function()
            self._playerChained = true
        end)
    end,

    Update = function(self, dt)
        if not DialogueManager.IsDialogueActive(self.dialogueName) then
            return
        end

        local currentIndex = DialogueManager.GetCurrentIndex(self.dialogueName)

        -- Reset dismiss timer when entry changes
        if currentIndex ~= self._lastIndex then
            self._lastIndex    = currentIndex
            self._conditionMet = false
            self._dismissTimer = 0
        end

        if currentIndex < 0 then return end

        -- Check if the condition for the current entry has been met
        if not self._conditionMet then
            if currentIndex == self.moveEntryIndex then
                -- Detect any movement input
                local axis = Input.GetAxis("Movement")
                if axis and (math.abs(axis.x) > 0.1 or math.abs(axis.y) > 0.1) then
                    self._playerMoved = true
                    self._conditionMet = true
                end

            elseif currentIndex == self.dashEntryIndex then
                if self._playerDashed then
                    self._conditionMet = true
                end

            elseif currentIndex == self.slashEntryIndex then
                if self._playerAttacked then
                    self._conditionMet = true
                end

            elseif currentIndex == self.chainEntryIndex then
                if self._playerChained then
                    self._conditionMet = true
                end
            end
        end

        -- Once condition is met, wait for dismissDelay then advance
        if self._conditionMet then
            self._dismissTimer = self._dismissTimer + dt
            if self._dismissTimer >= self.dismissDelay then
                DialogueManager.ScrollNext(self.dialogueName)
                self._conditionMet = false
                self._dismissTimer = 0
            end
        end
    end,

    Shutdown = function(self)
        if self._dashSub then event_bus.unsubscribe(self._dashSub) end
        if self._attackSub then event_bus.unsubscribe(self._attackSub) end
        if self._chainThrowSub then event_bus.unsubscribe(self._chainThrowSub) end
        if self._chainPullSub then event_bus.unsubscribe(self._chainPullSub) end
    end,
}
