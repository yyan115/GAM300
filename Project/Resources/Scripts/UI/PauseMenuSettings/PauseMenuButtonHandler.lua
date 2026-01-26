require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        fadeDuration = 1.0,
        fadeScreenName = "MenuFadeScreen",
    },

    Start = function(self)
        self._buttonData = {} 
        self.lastState = 1

        -- Link the detection area to the visual hover entity
        local buttonMapping = {
            { base = "ContinueButton", hover = "HoveredContinue" },
            { base = "SettingsButton", hover = "HoveredSetting" },
            { base = "MainMenuButton", hover = "HoveredMainMenu" }
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

                -- Initialize: Hide all except the default first button
                if hoverSprite then
                    hoverSprite.isVisible = (index == self.lastState)
                end
            end
        end
    end,

    Update = function(self, dt)
        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return end

        local mouseCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        local inputX, inputY = mouseCoordinate[1], mouseCoordinate[2]

        for index, data in ipairs(self._buttonData) do
            if inputX >= data.minX and inputX <= data.maxX and
               inputY >= data.minY and inputY <= data.maxY then

                -- Only swap if we moved to a new button
                if index ~= self.lastState then
                    -- Hide previous
                    if self._buttonData[self.lastState].hoverSprite then
                        self._buttonData[self.lastState].hoverSprite.isVisible = false
                    end

                    -- Show current
                    if data.hoverSprite then
                        data.hoverSprite.isVisible = true
                    end

                    self.lastState = index
                end                
                return 
            end
        end
    end,

    OnClickContinueButton = function(self)
        local ui = Engine.GetEntityByName("PauseMenuUI")
        if ui then GetComponent(ui, "ActiveComponent").isActive = false end
    end,

    OnClickSettingButton = function(self)
        -- Your settings logic
    end,

    OnClickMainMenuButton = function(self)
        -- Your quit logic
    end,
}