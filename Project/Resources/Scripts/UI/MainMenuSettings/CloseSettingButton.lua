require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    OnClickCloseButton = function(self)
        print("[CloseSettingButton] OnClickCloseButton called!")

        -- Save settings when closing menu (only writes if dirty)
        GameSettings.SaveIfDirty()
        print("[CloseSettingButton] Settings saved")

        -- BUTTONS TO ENABLE
        local targetButtons = {
            "PlayGame",
            "Credits",
            "ExitGame",
            "Settings"
        }

        for index, value in ipairs(targetButtons) do
            local targetEntity = Engine.GetEntityByName(value)
            if targetEntity and targetEntity ~= -1 then
                local btnComp = GetComponent(targetEntity, "ButtonComponent")
                if btnComp then
                    btnComp.interactable = true
                    print("[CloseSettingButton] Enabled button: " .. value)
                else
                    print("[CloseSettingButton] Warning: No ButtonComponent on " .. value)
                end
            else
                print("[CloseSettingButton] Warning: Could not find entity " .. value)
            end
        end

        -- CLOSE SETTINGS UI
        local settingUIEntity = Engine.GetEntityByName("SettingsUI")
        print("[CloseSettingButton] SettingsUI entity: " .. tostring(settingUIEntity))

        if settingUIEntity and settingUIEntity ~= -1 then
            local activeComp = GetComponent(settingUIEntity, "ActiveComponent")
            if activeComp then
                activeComp.isActive = false
                print("[CloseSettingButton] Set SettingsUI.isActive = false")
            else
                print("[CloseSettingButton] Warning: No ActiveComponent on SettingsUI")
            end
        else
            print("[CloseSettingButton] Warning: Could not find SettingsUI entity")
        end
    end,
}
