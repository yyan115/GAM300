require("extension.engine_bootstrap")

local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    fields = {
    },

    Start = function(self)
        self._isGamePaused = false
        self._pauseTimer = 0  -- Initialize timer

        local pauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        self._pauseComp = GetComponent(pauseUIEntity, "ActiveComponent")

        local settingsUIEntity = Engine.GetEntityByName("SettingsUI")
        self._settingsComp = GetComponent(settingsUIEntity, "ActiveComponent")

        local confirmUIEntity = Engine.GetEntityByName("ConfirmationPromptUI")
        self._confirmComp = GetComponent(confirmUIEntity, "ActiveComponent")

        local controlsUIEntity = Engine.GetEntityByName("ControlsUI")
        self._controlsComp = controlsUIEntity and GetComponent(controlsUIEntity, "ActiveComponent") or nil

        local blackScreenUIEntity = Engine.GetEntityByName("BlackScreen")
        self._blackScreenComp = GetComponent(blackScreenUIEntity, "ActiveComponent")

        -- Cache pause menu button components for direct control
        -- This ensures buttons are enabled in the same callback that opens the menu
        self._pauseButtons = {}
        local pauseButtonNames = {"ContinueButton", "SettingsButton", "MainMenuButton"}
        for _, name in ipairs(pauseButtonNames) do
            local buttonEntity = Engine.GetEntityByName(name)
            if buttonEntity then
                local buttonComp = GetComponent(buttonEntity, "ButtonComponent")
                if buttonComp then
                    self._pauseButtons[name] = buttonComp
                end
            end
        end

        -- Subscribe to player dead and player respawn events to prevent pausing when the player is dead.
        self._playerDead = false
        self._playerDeadSub = event_bus.subscribe("playerDead", function(dead)
            self._playerDead = true
        end)

        self._respawnPlayerSub = event_bus.subscribe("respawnPlayer", function(respawn)
            self._playerDead = false
        end)
    end,

    Update = function(self, dt)
        if not self._pauseComp or not self._settingsComp or not self._confirmComp then
            return
        end

        -- Use unscaled delta time for the cooldown timer so it works even when paused
        local unscaledDt = Time.GetUnscaledDeltaTime()
        if self._pauseTimer > 0 then
            self._pauseTimer = self._pauseTimer - unscaledDt
        end

        local isPressed = Input.IsActionPressed("Pause")
        -- Only fire if the button is pressed AND our cooldown has expired AND the player is not dead
        if isPressed and self._pauseTimer <= 0 and not self._playerDead then
            self._pauseTimer = 0.1  -- Cooldown to prevent double-fire

            local onSubPage = self._settingsComp.isActive or
                              (self._controlsComp and self._controlsComp.isActive)

            if not onSubPage then

            if self._confirmComp.isActive then
                self._confirmComp.isActive = false
                self._pauseComp.isActive = true  -- Go back to Pause menu
                -- Enable pause buttons immediately
                for _, buttonComp in pairs(self._pauseButtons) do
                    if buttonComp then buttonComp.interactable = true end
                end

            elseif self._pauseComp.isActive then
                -- Unpause game
                self._pauseComp.isActive = false
                Time.SetPaused(false)

                -- Re-lock cursor when unpausing via Esc
                if Screen then Screen.SetCursorLocked(true) end

                -- Unpause all game audio
                Audio.SetBusPaused("BGM", false)
                Audio.SetBusPaused("SFX", false)

                if event_bus and event_bus.publish then
                    event_bus.publish("game_paused", false)
                end
            else
                -- Pause game
                self._pauseComp.isActive = true
                Time.SetPaused(true)

                -- Directly enable pause buttons to avoid script execution order issues
                -- This ensures buttons are interactable on the same frame the menu opens
                for _, buttonComp in pairs(self._pauseButtons) do
                    if buttonComp then
                        buttonComp.interactable = true
                    end
                end

                -- Pause all game audio (UI on "UI" bus still plays)
                Audio.SetBusPaused("BGM", true)
                Audio.SetBusPaused("SFX", true)

                if event_bus and event_bus.publish then
                    event_bus.publish("game_paused", true)
                end
            end
            end  -- if not onSubPage
        end

        local isMenuActive = self._confirmComp.isActive or self._settingsComp.isActive or self._pauseComp.isActive or
                             (self._controlsComp and self._controlsComp.isActive)

        if isMenuActive then
            self._blackScreenComp.isActive = true
            if Screen and Screen.IsCursorLocked() then
                Screen.SetCursorLocked(false)
            end
        else
            self._blackScreenComp.isActive = false
        end
    end,
}
