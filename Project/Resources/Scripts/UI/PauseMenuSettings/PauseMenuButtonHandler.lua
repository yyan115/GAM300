require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        fadeDuration = 1.0,
        fadeScreenName = "MenuFadeScreen",
    },

    OnClickContinueButton = function(self)
            if Screen and Screen.IsCursorLocked() then
                Screen.SetCursorLocked(true)
            end
        local pauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        if pauseUIEntity then
            local pauseComp = GetComponent(pauseUIEntity, "ActiveComponent")
            if pauseComp then pauseComp.isActive = false end
        end
        Time.SetPaused(false)
    end,

    OnClickSettingButton = function(self)
        local PauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        local SettingsUIEntity = Engine.GetEntityByName("SettingsUI")
        
        if PauseUIEntity then 
            local comp = GetComponent(PauseUIEntity, "ActiveComponent")
            if comp then comp.isActive = false end 
        end
        if SettingsUIEntity then 
            local comp = GetComponent(SettingsUIEntity, "ActiveComponent")
            if comp then comp.isActive = true end 
        end
    end,

    OnClickMainMenuButton = function(self) 
        local pauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        if pauseUIEntity then
            local pauseComp = GetComponent(pauseUIEntity, "ActiveComponent")
            if pauseComp then pauseComp.isActive = false end
        end
           
        local confirmUIEntity = Engine.GetEntityByName("ConfirmationPromptUI")
        if confirmUIEntity then
            local confirmComp = GetComponent(confirmUIEntity, "ActiveComponent")
            if confirmComp then confirmComp.isActive = true end
        end
    end, -- Fixed: Correctly closing the function and adding comma for the next table entry

    Start = function(self)
        self._buttonData = {} 
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

                if hoverSprite then
                    hoverSprite.isVisible = false
                end
            else
                print("Warning: Missing entities for " .. names.base)
            end
        end
    end,

    Update = function(self, dt)
        if not self._buttonData then return end

        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return end

        local mouseCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        local inputX, inputY = mouseCoordinate[1], mouseCoordinate[2]

        for _, data in pairs(self._buttonData) do
            if data.hoverSprite then
                local isHovering = (inputX >= data.minX and inputX <= data.maxX and
                                   inputY >= data.minY and inputY <= data.maxY)
                
                data.hoverSprite.isVisible = isHovering
            end
        end
    end,
}