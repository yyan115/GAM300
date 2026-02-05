require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        mainMenuScene = "Resources/Scenes/01_MainMenu.scene",
        -- Sprite GUIDs: [1] = normal, [2] = hover
        YesSpriteGUIDs = {},
        NoSpriteGUIDs = {},
        buttonSFX = {},  -- Audio: [1] = hover SFX, [2] = click SFX
    },

    Start = function(self)
        -- Setup button data with sprite swapping support
        self._buttonData = {}
        local buttonMapping = {
            { base = "YesButton", spriteGUIDs = self.YesSpriteGUIDs },
            { base = "NoButton", spriteGUIDs = self.NoSpriteGUIDs },
        }

        -- Cache audio component from Buttons entity
        self._audio = self:GetComponent("AudioComponent")

        for index, config in ipairs(buttonMapping) do
            local baseEnt = Engine.GetEntityByName(config.base)

            if baseEnt then
                local transform = GetComponent(baseEnt, "Transform")
                local sprite = GetComponent(baseEnt, "SpriteRenderComponent")
                local pos = transform.localPosition
                local scale = transform.localScale

                self._buttonData[index] = {
                    name = config.base,
                    sprite = sprite,
                    spriteGUIDs = config.spriteGUIDs,
                    minX = pos.x - (scale.x / 2),
                    maxX = pos.x + (scale.x / 2),
                    minY = pos.y - (scale.y / 2),
                    maxY = pos.y + (scale.y / 2),
                    wasHovered = false
                }
            else
                print("[ConfirmationMenuHandler] Warning: Missing entity " .. config.base)
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
            local isHovering = inputX >= data.minX and inputX <= data.maxX and
                               inputY >= data.minY and inputY <= data.maxY

            -- Handle hover enter
            if isHovering and not data.wasHovered then
                -- Play hover sound
                if self._audio and self.buttonSFX and self.buttonSFX[1] then
                    self._audio:PlayOneShot(self.buttonSFX[1])
                end
                -- Switch to hover sprite
                if data.sprite and data.spriteGUIDs and data.spriteGUIDs[2] then
                    data.sprite:SetTextureFromGUID(data.spriteGUIDs[2])
                end
            -- Handle hover exit
            elseif not isHovering and data.wasHovered then
                -- Switch back to normal sprite
                if data.sprite and data.spriteGUIDs and data.spriteGUIDs[1] then
                    data.sprite:SetTextureFromGUID(data.spriteGUIDs[1])
                end
            end

            data.wasHovered = isHovering
        end
    end,

    OnClickYesButton = function(self)
        local audiocomp = GetComponent(Engine.GetEntityByName("YesButton"), "AudioComponent")
        if audiocomp then
            audiocomp:Play()
        end

        print("[ConfirmationMenuHandler] Returning to main menu")
        Time.SetPaused(false)  -- Reset pause state before loading scene
        Time.SetTimeScale(1.0)  -- Reset time scale to normal
        Scene.Load(self.mainMenuScene)
    end,

    OnClickNoButton = function(self)
        local audiocomp = GetComponent(Engine.GetEntityByName("NoButton"), "AudioComponent")
        if audiocomp then
            audiocomp:Play()
        end

        -- Close Confirmation UI, open Pause UI
        local confirmUIEntity = Engine.GetEntityByName("ConfirmationPromptUI")
        local confirmComp = GetComponent(confirmUIEntity, "ActiveComponent")
        if confirmComp then confirmComp.isActive = false end

        local pauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        local pauseComp = GetComponent(pauseUIEntity, "ActiveComponent")
        if pauseComp then pauseComp.isActive = true end
    end,
}
