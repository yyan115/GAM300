require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        -- Entity names for slider notches to update visual positions
        masterNotch = "MasterNotch",
        masterBar = "MasterBar",
        bgmNotch = "BGMNotch",
        bgmBar = "BGMBar",
        sfxNotch = "SFXNotch",
        sfxBar = "SFXBar",
        -- Sprite GUIDs array: [1] = normal sprite, [2] = hover sprite
        -- Drag-drop textures from editor (recognized via "sprite" in field name)
        spriteGUIDs = {},
    },

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        self._audio = self:GetComponent("AudioComponent")
        self._sprite = self:GetComponent("SpriteRenderComponent")
        self._isHovered = false
    end,

    Update = function(self, dt)
        self:_updateHover()
    end,

    -- Simple hover detection and sprite swap
    _updateHover = function(self)
        if not self._transform then return end

        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return end

        local mouseCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        local inputX = mouseCoordinate[1]
        local inputY = mouseCoordinate[2]

        local pos = self._transform.localPosition
        local scale = self._transform.localScale
        local minX = pos.x - (scale.x / 2)
        local maxX = pos.x + (scale.x / 2)
        local minY = pos.y - (scale.y / 2)
        local maxY = pos.y + (scale.y / 2)

        local isHovering = inputX >= minX and inputX <= maxX and inputY >= minY and inputY <= maxY

        if isHovering and not self._isHovered then
            self._isHovered = true
            if self._audio then self._audio:Play() end
            -- Switch to hover sprite
            if self._sprite and self.spriteGUIDs and self.spriteGUIDs[2] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[2])
            else
                print("[ResetSettingsButton] Cannot switch to hover - sprite: " .. tostring(self._sprite) .. ", spriteGUIDs[2]: " .. tostring(self.spriteGUIDs and self.spriteGUIDs[2]))
            end
        elseif not isHovering and self._isHovered then
            self._isHovered = false
            -- Switch back to normal sprite
            if self._sprite and self.spriteGUIDs and self.spriteGUIDs[1] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[1])
            else
                print("[ResetSettingsButton] Cannot switch to normal - sprite: " .. tostring(self._sprite) .. ", spriteGUIDs[1]: " .. tostring(self.spriteGUIDs and self.spriteGUIDs[1]))
            end
        end
    end,

    -- Reset all settings to defaults
    OnClickResetButton = function(self)
        print("[ResetSettings] OnClickResetButton called!")

        -- Initialize GameSettings (safe to call multiple times)
        GameSettings.Init()

        -- Reset to defaults (this also applies and saves via C++)
        GameSettings.ResetToDefaults()

        -- Update slider visual positions to reflect default values
        self:UpdateSliderPositions()

        print("[ResetSettings] All settings reset to defaults")
    end,

    UpdateSliderPositions = function(self)
        -- Helper function to update a slider's visual position
        local function updateSlider(notchName, barName, value, minVal, maxVal)
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

                    print("[ResetSettings] Updated " .. notchName .. " to value " .. value)
                end
            else
                print("[ResetSettings] Warning: Could not find " .. notchName .. " or " .. barName)
            end
        end

        -- Update all sliders using default values from C++
        -- Volume sliders: 0-1 range
        -- Gamma slider: 1-3 range
        updateSlider(self.masterNotch, self.masterBar, GameSettings.GetDefaultMasterVolume(), 0, 1)
        updateSlider(self.bgmNotch, self.bgmBar, GameSettings.GetDefaultBGMVolume(), 0, 1)
        updateSlider(self.sfxNotch, self.sfxBar, GameSettings.GetDefaultSFXVolume(), 0, 1)
        -- updateSlider(self.gammaNotch, self.gammaBar, GameSettings.GetDefaultGamma(), 1, 3)
    end,
}
