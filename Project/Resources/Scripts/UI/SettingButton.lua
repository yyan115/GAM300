require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    OnClickSettingButton = function(self)
        -- local targetEntity = Engine.GetEntityByName("Testing Entity")

        -- self._targetCollider = GetComponent(targetEntity,"ColliderComponent")
        -- print("TargetENTITY HERE IS ", targetEntity)

        -- if self._targetCollider then
        --     print("I actually got it?")
        -- end

        -- self._targetCollider.enabled = false


        local targetEntity = Engine.GetEntityByName("PlayGame")

        self._targetButton = GetComponent(targetEntity,"ButtonComponent")
        print("TargetENTITY HERE IS ", targetEntity)

        if self._targetCollider then
            print("I actually got it?")
        end

        self._targetButton.interactable = false


        self._targetRender = GetComponent(targetEntity,"SpriteRenderComponent")
        if self._targetRender then
            print("i guess render works huh")
        end

        self._targetRender.alpha = 0    --this works too

        





    end,

    Update = function(self, dt)
    end
}

--A way to access other entity   DONE
--Enable/Disable Entity   EITHER MAKE IT SO THAT LUA CAN ENABLE/DISABLE ENTITY OR JUST MAKE A TABLE COPY PASTE...

--Access Component Button -> Toggle "Interactable"      DONE
