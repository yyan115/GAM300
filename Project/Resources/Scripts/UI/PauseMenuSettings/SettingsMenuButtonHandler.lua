require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        fadeDuration = 1.0,
        fadeScreenName = "MenuFadeScreen",
    },

    OnClickResetButton = function(self)
    end,

    OnClickBackButton = function(self)
        --DISABLE SETTINGS UI SCREEN, ENABLE PAUSE UI SCREEN
        local SettingsUIEntity = Engine.GetEntityByName("SettingsUI")
        local SettingsComp = GetComponent(SettingsUIEntity, "ActiveComponent")
        SettingsComp.isActive = false

        local PauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        local PauseComp = GetComponent(PauseUIEntity, "ActiveComponent")
        PauseComp.isActive = true

    end,


    Start = function(self)
        self._buttonData = {} 

        local buttonMapping = {
            { base = "ResetButton", hover = "HoveredResetButton" },
            { base = "BackButton", hover = "HoveredBackButton" }
        }

        local audioSliders = {
            slider = "MasterTip", fill = "MasterAudioFill"
        }

        for index, names in ipairs(buttonMapping) do
            local baseEnt = Engine.GetEntityByName(names.base)
            local hoverEnt = Engine.GetEntityByName(names.hover)

            if baseEnt and hoverEnt then
                local transform = GetComponent(baseEnt, "Transform")
                local pos = transform.localPosition
                local scale = transform.localScale
                local hoverSprite = GetComponent(hoverEnt, "SpriteRenderComponent")

                self._buttonData[index] = {
                    hoverSprite = hoverSprite,
                    minX = pos.x - (scale.x / 2),
                    maxX = pos.x + (scale.x / 2),
                    minY = pos.y - (scale.y / 2),
                    maxY = pos.y + (scale.y / 2)
                }

                -- Ensure they start hidden
                if hoverSprite then hoverSprite.isVisible = false end
            else
                print("Warning: Missing entities for " .. names.base)
            end
        end
    end,

--HANDLE TOGGLE SPRITE UPON HOVER LOGIC
--HANDLE DRAGGING OF AUDIO SLIDERS

    Update = function(self, dt)
        if not self._buttonData then return end

        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return end

        local mouseCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        local inputX, inputY = mouseCoordinate[1], mouseCoordinate[2]

        -- Step 1: Hide EVERY hover sprite first
        for _, data in pairs(self._buttonData) do
            if data.hoverSprite then
                data.hoverSprite.isVisible = false
            end
        end

        -- Step 2: Check if mouse is over any button
        for _, data in pairs(self._buttonData) do
            if inputX >= data.minX and inputX <= data.maxX and
               inputY >= data.minY and inputY <= data.maxY then
                
                -- Show only this one
                if data.hoverSprite then
                    data.hoverSprite.isVisible = true
                end
                
                -- Stop looking once we find the hovered button
                return 
            end
        end
    end,
}