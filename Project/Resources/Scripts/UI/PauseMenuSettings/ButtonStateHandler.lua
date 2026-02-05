require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

-- This script ensures buttons are only interactable when the appropriate UI menu is active.
-- Button interactability is based on menu visibility, not pause state, to avoid
-- system execution order issues where Time.IsPaused() might lag behind menu activation.

-- Helper function to set interactable state for a group of buttons
local function SetGroupInteractable(buttonTable, isInteractable)
    for _, buttonComp in pairs(buttonTable) do
        if buttonComp then
            buttonComp.interactable = isInteractable
        end
    end
end

return Component {
    Start = function(self)
        -- Initialize GameSettings (safe to call multiple times)
        if GameSettings then
            GameSettings.Init()
        end

        -- Initialize storage tables for button groups
        self._pauseButtons   = {}
        self._settingButtons = {}
        self._confirmButtons = {}

        -- Define the button-to-menu mapping
        local menuMapping = {
            { names = {"ContinueButton", "SettingsButton", "MainMenuButton"}, storage = self._pauseButtons },
            { names = {"BackButton", "ResetButton"}, storage = self._settingButtons },
            { names = {"YesButton", "NoButton"}, storage = self._confirmButtons }
        }

        -- Populate ButtonComponent references
        for _, config in ipairs(menuMapping) do
            for _, buttonName in ipairs(config.names) do
                local buttonEntity = Engine.GetEntityByName(buttonName)

                if buttonEntity then
                    local buttonComp = GetComponent(buttonEntity, "ButtonComponent")
                    if buttonComp then
                        config.storage[buttonName] = buttonComp
                    end
                else
                    print("[ButtonStateHandler] Warning: Entity " .. buttonName .. " not found")
                end
            end
        end

        -- Cache UI ActiveComponent references
        local pauseMenuEntity   = Engine.GetEntityByName("PauseMenuUI")
        local settingMenuEntity = Engine.GetEntityByName("SettingsUI")
        local confirmMenuEntity = Engine.GetEntityByName("ConfirmationPromptUI")

        self._pauseMenuUI   = pauseMenuEntity and GetComponent(pauseMenuEntity, "ActiveComponent")
        self._settingMenuUI = settingMenuEntity and GetComponent(settingMenuEntity, "ActiveComponent")
        self._confirmMenuUI = confirmMenuEntity and GetComponent(confirmMenuEntity, "ActiveComponent")

        -- Force initial state update
        self:_updateButtonStates()
    end,

    Update = function(self, dt)
        self:_updateButtonStates()
    end,

    _updateButtonStates = function(self)
        -- Get current menu states
        local confirmActive = self._confirmMenuUI and self._confirmMenuUI.isActive
        local settingActive = self._settingMenuUI and self._settingMenuUI.isActive
        local pauseActive = self._pauseMenuUI and self._pauseMenuUI.isActive

        -- Enable buttons based on which menu is active (priority order: Confirmation > Settings > Pause)
        -- Only one menu should be active at a time, and only its buttons should be interactable.
        if confirmActive then
            SetGroupInteractable(self._pauseButtons, false)
            SetGroupInteractable(self._settingButtons, false)
            SetGroupInteractable(self._confirmButtons, true)
        elseif settingActive then
            SetGroupInteractable(self._pauseButtons, false)
            SetGroupInteractable(self._settingButtons, true)
            SetGroupInteractable(self._confirmButtons, false)
        elseif pauseActive then
            SetGroupInteractable(self._pauseButtons, true)
            SetGroupInteractable(self._settingButtons, false)
            SetGroupInteractable(self._confirmButtons, false)
        else
            -- No menu visible - disable all buttons
            SetGroupInteractable(self._pauseButtons, false)
            SetGroupInteractable(self._settingButtons, false)
            SetGroupInteractable(self._confirmButtons, false)
        end
    end,
}
