require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        fadeDuration = 1.0,
        fadeScreenName = "MenuFadeScreen",
        mainMenuScene = "Resources/Scenes/01_MainMenu.scene"

    },

    OnClickYesButton = function(self)
        --TODO TRANSITION
        print("TIME TO RETURN TO MAIN MENU")
        Time.SetPaused(false)  -- Reset pause state before loading scene
        Time.SetTimeScale(1.0)  -- Reset time scale to normal
        Scene.Load(self.mainMenuScene)
    end,

    --CLOSE CONFIRM UI MENU, OPEN UP PAUSE UI MENU
    OnClickNoButton = function(self)
        local confirmUIEntity = Engine.GetEntityByName("ConfirmationPromptUI")
        local confirmComp = GetComponent(confirmUIEntity, "ActiveComponent")
        confirmComp.isActive = false

        local pauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        local pauseComp = GetComponent(pauseUIEntity, "ActiveComponent")
        pauseComp.isActive = true
    end,

    Start = function(self)
        self._buttonData = {} 
        local buttonMapping = {
            { base = "YesButton", hover = "HoveredYesButton" },
            { base = "NoButton", hover = "HoveredNoButton" },
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

                -- Initialize: All hovers start OFF
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

        -- Logic: Check every button every frame
        for _, data in pairs(self._buttonData) do
            if data.hoverSprite then
                -- The hover sprite is ONLY visible if the mouse is inside the box
                local isHovering = (inputX >= data.minX and inputX <= data.maxX and
                                   inputY >= data.minY and inputY <= data.maxY)
                
                data.hoverSprite.isVisible = isHovering
            end
        end
    end,
}