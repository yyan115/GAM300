require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    OnClickCloseButton = function(self)
        --BUTTONS TO ENABLE 
        local targetButtons = {
            "PlayGame", 
            "Credits", 
            "ExitGame",
            "Settings" 
        }

        for index, value in ipairs(targetButtons) do
            local targetEntity = Engine.GetEntityByName(value)
            self._targetEntityButton = GetComponent(targetEntity, "ButtonComponent")
            self._targetEntityButton.interactable = true
        end


        --CLOSE SETTINGS UI
        local settingUIEntity = Engine.GetEntityByName("SettingsUI")
        self._settingUI = GetComponent(settingUIEntity, "ActiveComponent")
        self._settingUI.isActive = false
    end,
}
