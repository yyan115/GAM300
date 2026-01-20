require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    OnValueChanged = function(self)
        local slider = self:GetComponent("SliderComponent")
        if slider then
            print("[SliderTest] Slider value: " .. tostring(slider.value))
        end
    end
}
