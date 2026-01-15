require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    OnClickHoldButton = function(self)
        local targetEntity = Engine.GetEntityByName("PlayGame")
        self._targetEntityButton = GetComponent(targetEntity, "ButtonComponent")
        self._targetEntityButton.interactable = true

        self._slider = self:GetComponent("Transform")

    end,

    Update = function(self, dt)
        if not self._slider then
            print("Why cannot get COMPONENT ")
        end
        -- if Input.GetMouseButton(Input.MouseButton.Left) then
        --     local mouseX = Input.GetMouseX()
        --     local mouseY = Input.GetMouseY()
        --     print("Left mouse at: (" .. mouseX .. ", " .. mouseY .. ")")
        -- end
    end,
}
