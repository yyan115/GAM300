require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    _pendingScene = nil,

    -- Play Button Function
    OnClickPlayButton = function(self)
        self._pendingScene = "Resources/Scenes/M3_Gameplay.scene"
    end,

    -- Quit Button Function
    OnClickQuitButton = function(self)
        Screen.RequestClose()
    end,

    -- Settings Button Function
    OnClickSettingButton = function(self)
        local settingUIEntity = Engine.GetEntityByName("SettingsUI")
        local settingUI = GetComponent(settingUIEntity, "ActiveComponent")
        settingUI.isActive = true

        -- Disable buttons when settings menu is active
        local targetButtons = {"PlayGame", "Credits", "ExitGame", "Settings"}
        for _, buttonName in ipairs(targetButtons) do
            local entity = Engine.GetEntityByName(buttonName)
            local button = GetComponent(entity, "ButtonComponent")
            button.interactable = false
        end
    end,

    -- Credits Button Function
    OnClickCreditButton = function(self)
        -- Add credits logic here
    end,

    -- Update handles scene loading for Play button
    Update = function(self, dt)
        if self._pendingScene then
            Scene.Load(self._pendingScene)
            self._pendingScene = nil
        end
    end
}
