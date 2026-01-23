require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")


return Component {
    --To be EDITED SO CAN USE FOR ALL NOTCHES
    fields = {
        sliderName = "BGMNotch",
        sliderBar = "BGMBar"
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
    end,

    Update = function(self, dt)
        if not self._sliderTransform or not self._musicBar then
            print("Failed to get Component")
        end

        --GET INPUT OF MOUSE IN GAME COORDINATE (unified input system)
        if Input.IsPointerPressed() then
            local pointerPos = Input.GetPointerPosition()
            if not pointerPos then return end
            local gameCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)

            local gameX = gameCoordinate[1]
            local gameY = gameCoordinate[2]

            if gameX < self._minSliderX or gameX > self._maxSliderX or gameY < self._minSliderY or gameY > self._maxSliderY then
                return
            end

            --CLAMP AND UPDATE SLIDER TRANSFORM BASED ON CLICK/DRAG
            self._sliderTransform.localPosition.x = math.max(self._minSliderX, math.min(gameX, self._maxSliderX))
            self._sliderTransform.isDirty = true        


            --TODO: ENSURE IT PERSIST THROUGHOUT THE GAME


            --UPDATE MUSIC BASED ON INPUT 
            local newVolume = (self._sliderTransform.localPosition.x - self._minSliderX) / (self._maxSliderX - self._minSliderX)

            --APPLY SNAPPING
            if newVolume < 0.01 then
                newVolume = 0
            end
            if newVolume > 0.99 then
                newVolume = 1.0
            end

            Audio.SetBusVolume("BGM", newVolume)    --Persist through states

        end
    end,
}
