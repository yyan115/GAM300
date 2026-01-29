require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        fadeDuration = 1.0,
        fadeScreenName = "MenuFadeScreen",
    },

    OnClickContinueButton = function(self)
        -- Logic for continue
    end,

    OnClickSettingButton = function(self)
        --DISABLE PAUSE MENU UI SCREEN, ENABLE SETTINGS UI SCREEN
        local PauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        local PauseComp = GetComponent(PauseUIEntity, "ActiveComponent")
        PauseComp.isActive = false

        local SettingsUIEntity = Engine.GetEntityByName("SettingsUI")
        local SettingsComp = GetComponent(SettingsUIEntity, "ActiveComponent")
        SettingsComp.isActive = true

    

        print("Hello boss")
    end,

    OnClickMainMenuButton = function(self)
        -- Logic for quit
    end,

    Start = function(self)
        self._buttonData = {} 
        self.lastState = 1

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

                -- Initialize: Show only the default button if it exists
                if hoverSprite then
                    hoverSprite.isVisible = (index == self.lastState)
                end
            else
                self._buttonData[index] = nil
                print("Warning: Missing entities for " .. names.base)
            end
        end
    end,

    Update = function(self, dt)
        if not self._buttonData then 
        return 
    end
        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return end

        local mouseCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        local inputX, inputY = mouseCoordinate[1], mouseCoordinate[2]

        for index, data in pairs(self._buttonData) do
            if inputX >= data.minX and inputX <= data.maxX and
               inputY >= data.minY and inputY <= data.maxY then
                
                -- Only update if the button state has actually changed
                if index ~= self.lastState then
                    
                    -- Hide the previous hover sprite safely
                    local prevData = self._buttonData[self.lastState]
                    if prevData and prevData.hoverSprite then
                        prevData.hoverSprite.isVisible = false
                    end

                    -- Show the new hover sprite safely
                    if data.hoverSprite then
                        data.hoverSprite.isVisible = true
                    end

                    self.lastState = index
                end                
                return 
            end
        end
    end,
}