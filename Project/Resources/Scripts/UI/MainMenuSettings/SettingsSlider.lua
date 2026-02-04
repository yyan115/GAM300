require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

-- Unified Settings Slider Script
-- Attach to each slider notch entity and configure via fields
-- settingType: "master", "bgm", "sfx", or "gamma"
return Component {
    fields = {
        sliderNotch = "MasterNotch",  -- Name of the notch entity (this entity)
        sliderBar = "MasterBar",      -- Name of the bar entity
        sliderFill = "",              -- Name of the fill entity (optional, leave empty if no fill)
        settingType = "master",       -- "master", "bgm", "sfx", or "gamma"
    },

    Start = function(self)
        -- Helper to strip quotes from string values (in case user entered them in editor)
        local function stripQuotes(str)
            if type(str) ~= "string" then return str end
            return str:gsub('^"(.*)"$', '%1'):gsub("^'(.*)'$", '%1')
        end

        -- Clean field values
        local notchName = stripQuotes(self.sliderNotch)
        local barName = stripQuotes(self.sliderBar)
        local fillName = stripQuotes(self.sliderFill)
        local settingType = stripQuotes(self.settingType)

        -- Debug: Show what field values we received
        print("[SettingsSlider] Start called with fields:")
        print("  sliderNotch = " .. tostring(notchName))
        print("  sliderBar = " .. tostring(barName))
        print("  sliderFill = " .. tostring(fillName))
        print("  settingType = " .. tostring(settingType))

        -- Initialize GameSettings (safe to call multiple times)
        if GameSettings then
            GameSettings.Init()
        else
            print("[SettingsSlider] Warning: GameSettings not available")
            return
        end

        -- Normalize settingType to lowercase for consistent comparison
        if not settingType then
            print("[SettingsSlider] Warning: settingType field not set")
            self._settingType = "master"
        else
            self._settingType = string.lower(settingType)
        end

        -- Cache references to slider entities
        local notchEntity = Engine.GetEntityByName(notchName)
        self._sliderTransform = GetComponent(notchEntity, "Transform")

        -- Cache audio component for click SFX (prefer notch, fallback to bar)
        self._audio = GetComponent(notchEntity, "AudioComponent")

        local barEntity = Engine.GetEntityByName(barName)
        self._barTransform = GetComponent(barEntity, "Transform")
        if not self._audio then
            self._audio = GetComponent(barEntity, "AudioComponent")
        end

        if not self._sliderTransform or not self._barTransform then
            print("[SettingsSlider] Warning: Could not find slider entities for " .. self._settingType)
            return
        end

        -- Calculate slider bounds based on bar dimensions
        local offsetX = self._barTransform.localScale.x / 2.0
        local offsetY = self._barTransform.localScale.y / 2.0
        self._maxSliderX = self._barTransform.localPosition.x + offsetX
        self._minSliderX = self._barTransform.localPosition.x - offsetX
        self._maxSliderY = self._barTransform.localPosition.y + offsetY
        self._minSliderY = self._barTransform.localPosition.y - offsetY

        -- Cache fill bar transform (optional)
        self._fillTransform = nil
        self._fillMaxWidth = nil
        if fillName and fillName ~= "" then
            local fillEntity = Engine.GetEntityByName(fillName)
            if fillEntity then
                self._fillTransform = GetComponent(fillEntity, "Transform")
                if self._fillTransform then
                    -- Max width is the full bar width
                    self._fillMaxWidth = self._barTransform.localScale.x
                    print("[SettingsSlider] Fill bar configured: " .. fillName)
                end
            else
                print("[SettingsSlider] Warning: Fill entity not found: " .. fillName)
            end
        end

        -- Track dragging state
        self._isDragging = false

        -- Get min/max values based on setting type
        if self._settingType == "gamma" then
            self._minValue = 1.0
            self._maxValue = 3.0
        else
            self._minValue = 0.0
            self._maxValue = 1.0
        end

        -- Set initial slider position from saved settings
        local savedValue = self:GetCurrentValue()
        local normalized = self:ValueToNormalized(savedValue)
        local savedPosX = self._minSliderX + (normalized * (self._maxSliderX - self._minSliderX))
        self._sliderTransform.localPosition.x = savedPosX
        self._sliderTransform.isDirty = true

        -- Update fill bar to match initial position
        self:UpdateFillBar(normalized)

        print("[SettingsSlider] " .. self._settingType .. " initialized with value: " .. savedValue)
    end,

    -- Update the fill bar based on normalized value (0-1)
    UpdateFillBar = function(self, normalized)
        if not self._fillTransform or not self._fillMaxWidth then
            return
        end

        -- Calculate fill width based on normalized value
        local fillWidth = normalized * self._fillMaxWidth
        
        -- Minimum width to prevent zero-scale issues
        if fillWidth < 1 then fillWidth = 1 end

        -- Update fill scale (width)
        self._fillTransform.localScale.x = fillWidth

        -- Position the fill bar so its left edge aligns with the bar's left edge
        -- The fill's center should be at: leftEdge + (fillWidth / 2)
        self._fillTransform.localPosition.x = self._minSliderX + (fillWidth / 2)
        self._fillTransform.isDirty = true
    end,

    Update = function(self, dt)
        -- Skip if not properly initialized
        if not self._sliderTransform or not self._barTransform or not self._settingType then
            return
        end

        local pointerPressed = Input.IsPointerPressed()
        local pointerJustPressed = Input.IsPointerJustPressed()

        -- Handle drag start - only if pointer just pressed within our bounds
        if pointerPressed and not self._isDragging then
            local pointerPos = Input.GetPointerPosition()
            if pointerPos then
                local gameCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
                local gameX = gameCoordinate[1]
                local gameY = gameCoordinate[2]

                -- Check if click is within this slider's bounds
                if gameX >= self._minSliderX and gameX <= self._maxSliderX and
                   gameY >= self._minSliderY and gameY <= self._maxSliderY then
                    self._isDragging = true

                    if self._audio then
                        self._audio:Play()
                    end
                end
            end
        end

        -- Handle drag end
        if not pointerPressed then
            self._isDragging = false
        end

        -- Handle dragging - only if we started the drag
        if self._isDragging and pointerPressed then
            local pointerPos = Input.GetPointerPosition()
            if not pointerPos then return end
            local gameCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
            local gameX = gameCoordinate[1]

            -- Clamp X position to slider bounds (allow dragging even if Y is outside)
            local clampedX = math.max(self._minSliderX, math.min(gameX, self._maxSliderX))
            self._sliderTransform.localPosition.x = clampedX
            self._sliderTransform.isDirty = true

            -- Calculate normalized value (0-1)
            local normalized = (clampedX - self._minSliderX) / (self._maxSliderX - self._minSliderX)

            -- Apply snapping at edges
            if normalized < 0.01 then normalized = 0 end
            if normalized > 0.99 then normalized = 1.0 end

            -- Update the fill bar to follow the notch
            self:UpdateFillBar(normalized)

            -- Convert to actual value and apply
            local newValue = self:NormalizedToValue(normalized)
            self:SetCurrentValue(newValue)
        end
    end,

    -- Convert a setting value to normalized 0-1 range
    ValueToNormalized = function(self, value)
        return (value - self._minValue) / (self._maxValue - self._minValue)
    end,

    -- Convert normalized 0-1 to actual setting value
    NormalizedToValue = function(self, normalized)
        return self._minValue + (normalized * (self._maxValue - self._minValue))
    end,

    -- Get current value from GameSettings based on settingType
    GetCurrentValue = function(self)
        if self._settingType == "master" then
            return GameSettings.GetMasterVolume()
        elseif self._settingType == "bgm" then
            return GameSettings.GetBGMVolume()
        elseif self._settingType == "sfx" then
            return GameSettings.GetSFXVolume()
        elseif self._settingType == "gamma" then
            return GameSettings.GetGamma()
        end
        return 1.0
    end,

    -- Set value in GameSettings based on settingType
    SetCurrentValue = function(self, value)
        if self._settingType == "master" then
            GameSettings.SetMasterVolume(value)
        elseif self._settingType == "bgm" then
            GameSettings.SetBGMVolume(value)
        elseif self._settingType == "sfx" then
            GameSettings.SetSFXVolume(value)
        elseif self._settingType == "gamma" then
            GameSettings.SetGamma(value)
        end
    end,
}
