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
        contrastNotch = "ContrastNotch",
        contrastBar = "ContrastBar"
    },

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
        updateSlider(self.gammaNotch, self.gammaBar, GameSettings.GetDefaultGamma(), 1, 3)
    end,
}
