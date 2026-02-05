require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        -- Audio: [1] = hover SFX, [2] = click SFX
        ButtonSFX = {},
        -- Sprite GUIDs: [1] = normal, [2] = hover
        ContinueSpriteGUIDs = {},
        SettingSpriteGUIDs = {},
        MainMenuSpriteGUIDs = {},
    },

    Start = function(self)
        -- Cache audio component from PauseMenuUI entity
        self._audio = self:GetComponent("AudioComponent")

        -- Setup button data with sprite swapping support
        self._buttonData = {}
        local buttonMapping = {
            { base = "ContinueButton", spriteGUIDs = self.ContinueSpriteGUIDs },
            { base = "SettingsButton", spriteGUIDs = self.SettingSpriteGUIDs },
            { base = "MainMenuButton", spriteGUIDs = self.MainMenuSpriteGUIDs },
        }

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
                print("[PauseMenuButtonHandler] Warning: Missing entity " .. config.base)
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
                if self._audio and self.ButtonSFX and self.ButtonSFX[1] then
                    self._audio:PlayOneShot(self.ButtonSFX[1])
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

    OnClickContinueButton = function(self)
        local audiocomp = GetComponent(Engine.GetEntityByName("ContinueButton"), "AudioComponent")
        if audiocomp then
            audiocomp:Play()
        end
        
        -- Lock cursor for gameplay (FPS camera control)
        if Screen then
            Screen.SetCursorLocked(true)
        end

        -- Hide pause menu
        local pauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        if pauseUIEntity then
            local pauseComp = GetComponent(pauseUIEntity, "ActiveComponent")
            if pauseComp then pauseComp.isActive = false end
        end

        -- Resume BGM and Ambience
        local bgmEntity = Engine.GetEntityByName("BGM1")
        if bgmEntity then
            local bgmAudio = GetComponent(bgmEntity, "AudioComponent")
            if bgmAudio then bgmAudio:UnPause() end
        end
        local ambienceEntity = Engine.GetEntityByName("Ambience")
        if ambienceEntity then
            local ambienceAudio = GetComponent(ambienceEntity, "AudioComponent")
            if ambienceAudio then ambienceAudio:UnPause() end
        end

        Time.SetPaused(false)
    end,

    OnClickSettingButton = function(self)
        local audiocomp = GetComponent(Engine.GetEntityByName("SettingsButton"), "AudioComponent")
        if audiocomp then
            audiocomp:Play()
        end

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
        local audiocomp = GetComponent(Engine.GetEntityByName("MainMenuButton"), "AudioComponent")
        if audiocomp then
            audiocomp:Play()
        end

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
    end,
}
