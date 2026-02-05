require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        -- Entity names for slider notches to update visual positions
        masterNotch = "MasterNotch",
        masterBar = "MasterBar",
        masterFill = "MasterFill",
        bgmNotch = "BGMNotch",
        bgmBar = "BGMBar",
        bgmFill = "BGMFill",
        sfxNotch = "SFXNotch",
        sfxBar = "SFXBar",
        sfxFill = "SFXFill",
        -- Audio: [1] = hover SFX, [2] = click SFX
        buttonSFX = {},
        -- Sprite GUIDs: [1] = normal, [2] = hover
        ResetSpriteGUIDs = {},
        BackSpriteGUIDs = {},
    },

    Start = function(self)
        -- Initialize GameSettings (safe to call multiple times)
        if GameSettings then
            GameSettings.Init()
        end

        -- Cache audio component from Buttons entity
        self._audio = self:GetComponent("AudioComponent")

        -- Setup button data with sprite swapping support
        self._buttonData = {}
        local buttonMapping = {
            { base = "ResetButton", spriteGUIDs = self.ResetSpriteGUIDs },
            { base = "BackButton", spriteGUIDs = self.BackSpriteGUIDs },
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
                print("[SettingsMenuButtonHandler] Warning: Missing entity " .. config.base)
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

    -- Reset all settings to defaults
    OnClickResetButton = function(self)
        local audiocomp = GetComponent(Engine.GetEntityByName("ResetButton"), "AudioComponent")
        if audiocomp then
            audiocomp:Play()
        end

        -- Initialize GameSettings (safe to call multiple times)
        GameSettings.Init()

        -- Reset to defaults (this also applies and saves via C++)
        GameSettings.ResetToDefaults()

        -- Update slider visual positions to reflect default values
        self:UpdateSliderPositions()

        print("[SettingsMenuButtonHandler] All settings reset to defaults")
    end,

    OnClickBackButton = function(self)
        local audiocomp = GetComponent(Engine.GetEntityByName("BackButton"), "AudioComponent")
        if audiocomp then
            audiocomp:Play()
        end

        -- Disable Settings UI, enable Pause UI
        local SettingsUIEntity = Engine.GetEntityByName("SettingsUI")
        local SettingsComp = GetComponent(SettingsUIEntity, "ActiveComponent")
        if SettingsComp then SettingsComp.isActive = false end

        local PauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        local PauseComp = GetComponent(PauseUIEntity, "ActiveComponent")
        if PauseComp then PauseComp.isActive = true end
    end,

    -- Helper function to update all slider visual positions
    UpdateSliderPositions = function(self)
        local function updateSlider(notchName, barName, fillName, value, minVal, maxVal)
            local notchEntity = Engine.GetEntityByName(notchName)
            local barEntity = Engine.GetEntityByName(barName)

            if notchEntity and notchEntity ~= -1 and barEntity and barEntity ~= -1 then
                local notchTransform = GetComponent(notchEntity, "Transform")
                local barTransform = GetComponent(barEntity, "Transform")

                if notchTransform and barTransform then
                    local offsetX = barTransform.localScale.x / 2.0
                    local minX = barTransform.localPosition.x - offsetX
                    local maxX = barTransform.localPosition.x + offsetX

                    -- Normalize value to 0-1 range
                    local normalized = (value - minVal) / (maxVal - minVal)

                    local newPosX = minX + (normalized * (maxX - minX))
                    notchTransform.localPosition.x = newPosX
                    notchTransform.isDirty = true

                    -- Update fill bar if specified
                    if fillName and fillName ~= "" then
                        local fillEntity = Engine.GetEntityByName(fillName)
                        if fillEntity and fillEntity ~= -1 then
                            local fillTransform = GetComponent(fillEntity, "Transform")
                            if fillTransform then
                                local fillMaxWidth = barTransform.localScale.x
                                local fillWidth = normalized * fillMaxWidth
                                if fillWidth < 1 then fillWidth = 1 end

                                fillTransform.localScale.x = fillWidth
                                fillTransform.localPosition.x = minX + (fillWidth / 2)
                                fillTransform.isDirty = true
                            end
                        end
                    end
                end
            else
                print("[SettingsMenuButtonHandler] Warning: Could not find " .. notchName .. " or " .. barName)
            end
        end

        -- Update all sliders using default values from C++
        -- Volume sliders: 0-1 range
        updateSlider(self.masterNotch, self.masterBar, self.masterFill, GameSettings.GetDefaultMasterVolume(), 0, 1)
        updateSlider(self.bgmNotch, self.bgmBar, self.bgmFill, GameSettings.GetDefaultBGMVolume(), 0, 1)
        updateSlider(self.sfxNotch, self.sfxBar, self.sfxFill, GameSettings.GetDefaultSFXVolume(), 0, 1)
    end,
}
