require("extension.engine_bootstrap")

local Component = require("extension.mono_helper")

local TransformMixin = require("extension.transform_mixin")


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

        local blackScreenUIEntity = Engine.GetEntityByName("BlackScreen")
        self._blackScreenComp = GetComponent(blackScreenUIEntity, "ActiveComponent")

        -- Cache BGM and Ambience audio components for pause/unpause
        local bgmEntity = Engine.GetEntityByName("BGM1")
        self._bgmAudio = bgmEntity and GetComponent(bgmEntity, "AudioComponent")

        local ambienceEntity = Engine.GetEntityByName("Ambience")
        self._ambienceAudio = ambienceEntity and GetComponent(ambienceEntity, "AudioComponent")

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

        local isPressed = Input.IsActionJustPressed("Pause")
        -- Only fire if the button is pressed AND our cooldown has expired
        if isPressed and self._pauseTimer <= 0 then
            self._pauseTimer = 0.1  -- Cooldown to prevent double-fire

            if self._confirmComp.isActive then
                self._confirmComp.isActive = false
                self._pauseComp.isActive = true  -- Go back to Pause menu
                -- Enable pause buttons immediately
                for _, buttonComp in pairs(self._pauseButtons) do
                    if buttonComp then buttonComp.interactable = true end
                end

            elseif self._settingsComp.isActive then
                self._settingsComp.isActive = false
                self._pauseComp.isActive = true  -- Go back to Pause menu
                -- Enable pause buttons immediately
                for _, buttonComp in pairs(self._pauseButtons) do
                    if buttonComp then buttonComp.interactable = true end
                end

            elseif self._pauseComp.isActive then
                -- Unpause game
                self._pauseComp.isActive = false
                Time.SetPaused(false)

                -- Resume BGM and Ambience
                if self._bgmAudio then self._bgmAudio:UnPause() end
                if self._ambienceAudio then self._ambienceAudio:UnPause() end
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

                -- Pause BGM and Ambience (UI SFX will still work)
                if self._bgmAudio then self._bgmAudio:Pause() end
                if self._ambienceAudio then self._ambienceAudio:Pause() end
            end
        end

        local isMenuActive = self._confirmComp.isActive or self._settingsComp.isActive or self._pauseComp.isActive

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
