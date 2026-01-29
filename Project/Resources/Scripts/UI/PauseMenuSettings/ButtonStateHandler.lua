require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")



--THIS SCRIPT IS JUST TO ENSURE THAT OTHER BUTTONS CANNOT BE PRESSED THE UI TAGGED TO IT IS INACTIVE


--HELPER FUNCTION
    local function SetGroupInteractable(buttonTable, isInteractable)
        for _, buttonComp in pairs(buttonTable) do
            buttonComp.interactable = isInteractable
        end
    end




return Component {
    Start = function(self)
        -- 1. Initialize storage tables
        self._pauseButtons   = {}
        self._settingButtons = {}
        self._confirmButtons = {}

        -- 2. Define the data mapping
        local menuMapping = {
            { names = {"ContinueButton", "SettingsButton", "MainMenuButton"}, storage = self._pauseButtons },
            { names = {"BackButton", "ResetButton"}, storage = self._settingButtons },
            { names = {"YesButton", "NoButton"}, storage = self._confirmButtons }
        }

        -- Nested loop to populate ButtonComponent
        for _, config in ipairs(menuMapping) do
            for _, buttonName in ipairs(config.names) do
                local buttonEntity = Engine.GetEntityByName(buttonName)
                
                if buttonEntity then
                    local buttonComp = GetComponent(buttonEntity, "ButtonComponent")
                    if buttonComp then
                        -- Store in the respective table 
                        config.storage[buttonName] = buttonComp
                    end
                else
                    print("Error: Entity " .. buttonName .. " not found in scene.")
                end
            end
        end

        local pauseMenuEntity   = Engine.GetEntityByName("PauseMenuUI")
        local settingMenuEntity = Engine.GetEntityByName("SettingsUI")
        local confirmMenuEntity = Engine.GetEntityByName("ConfirmationPromptUI")

        self._pauseMenuUI   = GetComponent(pauseMenuEntity, "ActiveComponent")
        self._settingMenuUI = GetComponent(settingMenuEntity, "ActiveComponent")
        self._confirmMenuUI = GetComponent(confirmMenuEntity, "ActiveComponent")
    end,



    Update = function(self, dt)
        if self._settingMenuUI.isActive or self._confirmMenuUI.isActive then 
            for name, buttonComp in pairs(self._pauseButtons) do
                buttonComp.interactable = false
            end
        end

        --PAUSE MENU ACTIVE, SET BUTTON TO INTERACTABLE, THE REST ALL FALSE
        if self._pauseMenuUI.isActive then
            SetGroupInteractable(self._pauseButtons, true)
            SetGroupInteractable(self._settingButtons, false)
            SetGroupInteractable(self._confirmButtons, false)
        end
        if self._settingMenuUI.isActive then
            SetGroupInteractable(self._pauseButtons, false)
            SetGroupInteractable(self._settingButtons, true)
            SetGroupInteractable(self._confirmButtons, false)
        end

        if self._confirmMenuUI.isActive then
            SetGroupInteractable(self._pauseButtons, false)
            SetGroupInteractable(self._settingButtons, false)
            SetGroupInteractable(self._confirmButtons, true)
        end
    end,
}