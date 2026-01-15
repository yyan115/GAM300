require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")


return Component {
    --To be EDITED SO CAN USE FOR ALL NOTCHES
    fields = {
        sliderName = "MasterNotch",
        sliderBar = "MasterBar"
    },

    OnClickHoldButton = function(self)

        local sliderEntity = Engine.GetEntityByName(self.sliderName)
        self._sliderTransform = GetComponent(sliderEntity, "Transform")
        
        local sliderBarEntity = Engine.GetEntityByName(self.sliderBar)
        self._musicBar = GetComponent(sliderBarEntity, "Transform")

        --CALCULATION TO CLAMP THE SLIDER
        local offsetX = self._musicBar.localScale.x / 2.0
        local offsetY = self._musicBar.localScale.y / 2.0 + 120     --For user experience
        self._maxSliderX = self._musicBar.localPosition.x + offsetX
        self._minSliderX = self._musicBar.localPosition.x - offsetX

        self._maxSliderY = self._musicBar.localPosition.y + offsetY
        self._minSliderY = self._musicBar.localPosition.y - offsetY 

        local masterVolumeEntity = Engine.GetEntityByName("BGM")
        Audio.SetMasterVolume(0.5)
    end,

    Update = function(self, dt)
        if not self._sliderTransform or not self._musicBar then
            print("Failed to get Component")
        end

        if Input.GetMouseButton(Input.MouseButton.Left) then
            local mouseX = Input.GetMouseX()
            local mouseY = Input.GetMouseY()
            local gameCoordinate = Engine.GetGameCoordinate(mouseX, mouseY)

            local gameX = gameCoordinate[1]
            local gameY = gameCoordinate[2]

            if gameX < self._minSliderX or gameX > self._maxSliderX or gameY < self._minSliderY or gameY > self._maxSliderY then
                return
            end

            self._sliderTransform.localPosition.x = math.max(self._minSliderX, math.min(gameX, self._maxSliderX))
            self._sliderTransform.isDirty = true        
        end
    end,
}
