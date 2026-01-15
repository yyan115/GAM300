require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    --SHOW SETTINGS UI, DISABLE ALL OTHER BUTTONS, Enable ESC to close as well
    OnClickSettingButton = function(self)
        local settingUIEntity = Engine.GetEntityByName("SettingsUI")
        self._settingUI = GetComponent(settingUIEntity, "ActiveComponent")
        self._settingUI.isActive = true

        --BUTTONS TO DISABLE WHEN SETTINGS MENU IS ACTIVE   
        local targetButtons = {
            "PlayGame", 
            "Credits", 
            "ExitGame"
        }

        for index, value in ipairs(targetButtons) do
            local targetEntity = Engine.GetEntityByName(value)
            self._targetEntityButton = GetComponent(targetEntity, "ButtonComponent")
            self._targetEntityButton.interactable = false
        end
    end,
}

--A way to access other entity   DONE
--Enable/Disable Entity   EITHER MAKE IT SO THAT LUA CAN ENABLE/DISABLE ENTITY OR JUST MAKE A TABLE COPY PASTE...

--Access Component Button -> Toggle "Interactable"      DONE


        -- local targetEntity = Engine.GetEntityByName("Testing Entity")

        -- self._targetCollider = GetComponent(targetEntity,"ColliderComponent")
        -- print("TargetENTITY HERE IS ", targetEntity)

        -- if self._targetCollider then
        --     print("I actually got it?")
        -- end

        -- self._targetCollider.enabled = false


        -- local targetEntity = Engine.GetEntityByName("PlayGame")

        -- self._targetButton = GetComponent(targetEntity,"ButtonComponent")
        -- print("TargetENTITY HERE IS ", targetEntity)

        -- if self._targetCollider then
        --     print("I actually got it?")
        -- end

        -- self._targetButton.interactable = false


        -- self._targetRender = GetComponent(targetEntity,"SpriteRenderComponent")
        -- if self._targetRender then
        --     print("i guess render works huh")
        -- end

        -- self._targetRender.alpha = 0    --this works too


        -- --TOGGLE ENTITY TEST lets go it works
        -- self._targetActive = GetComponent(targetEntity, "ActiveComponent")
        -- self._targetActive.isActive = false
