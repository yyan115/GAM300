require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

-- Main menu settings slider controller.
-- Attach one instance to SettingsUI.
return Component {
    fields = {
        enableMaster  = true,
        enableBGM     = true,
        enableSFX     = true,
        enableGamma   = true,
    },

    Awake = function(self)
        self._pendingReset = false
        if event_bus and event_bus.subscribe then
            self._settingsResetSub = event_bus.subscribe("settings_reset", function()
                self._pendingReset = true
            end)
        end
    end,

    Start = function(self)
        if not GameSettings then
            print("[MainMenuSettingsSlider] Warning: GameSettings not available")
            return
        end
        GameSettings.Init()

        local VALUE_RANGES = {
            master             = { 0.0,   1.0 },
            bgm                = { 0.0,   1.0 },
            sfx                = { 0.0,   1.0 },
            gamma              = { 1.0,   3.0 },
            exposure           = { 0.1,   5.0 },
            bloomthreshold     = { 0.0,   5.0 },
            bloomintensity     = { 0.0,   5.0 },
            bloomscatter       = { 0.0,   1.0 },
            vignetteintensity  = { 0.0,   1.0 },
            vignettesmoothness = { 0.0,   1.0 },
            cgbrightness       = { -1.0,  1.0 },
            cgcontrast         = { 0.0,   2.0 },
            cgsaturation       = { 0.0,   2.0 },
            caintensity        = { 0.0,   1.0 },
            capadding          = { 0.0,   1.0 },
            ssaoradius         = { 0.1,   2.0 },
            ssaobias           = { 0.001, 0.1 },
            ssaointensity      = { 0.0,   5.0 },
        }

        local SETTINGS_API = {
            master             = { function() return GameSettings.GetMasterVolume() end,      function(v) GameSettings.SetMasterVolume(v) end },
            bgm                = { function() return GameSettings.GetBGMVolume() end,         function(v) GameSettings.SetBGMVolume(v) end },
            sfx                = { function() return GameSettings.GetSFXVolume() end,         function(v) GameSettings.SetSFXVolume(v) end },
            gamma              = { function() return GameSettings.GetGamma() end,             function(v) GameSettings.SetGamma(v) end },
            exposure           = { function() return GameSettings.GetExposure() end,          function(v) GameSettings.SetExposure(v) end },
            bloomthreshold     = { function() return GameSettings.GetBloomThreshold() end,    function(v) GameSettings.SetBloomThreshold(v) end },
            bloomintensity     = { function() return GameSettings.GetBloomIntensity() end,    function(v) GameSettings.SetBloomIntensity(v) end },
            bloomscatter       = { function() return GameSettings.GetBloomScatter() end,      function(v) GameSettings.SetBloomScatter(v) end },
            vignetteintensity  = { function() return GameSettings.GetVignetteIntensity() end, function(v) GameSettings.SetVignetteIntensity(v) end },
            vignettesmoothness = { function() return GameSettings.GetVignetteSmoothness() end,function(v) GameSettings.SetVignetteSmoothness(v) end },
            cgbrightness       = { function() return GameSettings.GetCGBrightness() end,      function(v) GameSettings.SetCGBrightness(v) end },
            cgcontrast         = { function() return GameSettings.GetCGContrast() end,        function(v) GameSettings.SetCGContrast(v) end },
            cgsaturation       = { function() return GameSettings.GetCGSaturation() end,      function(v) GameSettings.SetCGSaturation(v) end },
            caintensity        = { function() return GameSettings.GetCAIntensity() end,       function(v) GameSettings.SetCAIntensity(v) end },
            capadding          = { function() return GameSettings.GetCAPadding() end,         function(v) GameSettings.SetCAPadding(v) end },
            ssaoradius         = { function() return GameSettings.GetSSAORadius() end,        function(v) GameSettings.SetSSAORadius(v) end },
            ssaobias           = { function() return GameSettings.GetSSAOBias() end,          function(v) GameSettings.SetSSAOBias(v) end },
            ssaointensity      = { function() return GameSettings.GetSSAOIntensity() end,     function(v) GameSettings.SetSSAOIntensity(v) end },
        }

        local SLIDER_DEFS = {
            { field = "enableMaster",  prefix = "Master",  type = "master"  },
            { field = "enableBGM",     prefix = "BGM",     type = "bgm"     },
            { field = "enableSFX",     prefix = "SFX",     type = "sfx"     },
            { field = "enableGamma",   prefix = "Gamma",   type = "gamma"   },
        }

        self._sliders = {}
        self._valueRanges = VALUE_RANGES
        self._settingsAPI = SETTINGS_API

        for _, def in ipairs(SLIDER_DEFS) do
            if self[def.field] then
                local notchEntity = Engine.GetEntityByName(def.prefix .. "Notch")
                local fillEntity  = Engine.GetEntityByName(def.prefix .. "Fill")

                if not notchEntity or not fillEntity then
                    print("[MainMenuSettingsSlider] Warning: Missing entities for " .. def.prefix)
                else
                    local notchTransform = GetComponent(notchEntity, "Transform")
                    local fillTransform  = GetComponent(fillEntity,  "Transform")
                    local fillSprite     = GetComponent(fillEntity,  "SpriteRenderComponent")

                    if not notchTransform or not fillTransform then
                        print("[MainMenuSettingsSlider] Warning: Missing Transform on " .. def.prefix .. " entities")
                    else
                        local offsetX = fillTransform.localScale.x / 2.0
                        local offsetY = fillTransform.localScale.y / 2.0
                        local slider = {
                            settingType    = def.type,
                            notchTransform = notchTransform,
                            fillSprite     = fillSprite,
                            minX           = fillTransform.localPosition.x - offsetX,
                            maxX           = fillTransform.localPosition.x + offsetX,
                            minY           = fillTransform.localPosition.y - offsetY,
                            maxY           = fillTransform.localPosition.y + offsetY,
                            isDragging     = false,
                        }

                        self:_initSliderPosition(slider)
                        table.insert(self._sliders, slider)
                    end
                end
            end
        end
    end,

    Update = function(self, dt)
        if not self._sliders or #self._sliders == 0 then return end

        local settingsUIEntity = Engine.GetEntityByName("SettingsUI")
        if settingsUIEntity then
            local activeComp = GetComponent(settingsUIEntity, "ActiveComponent")
            if activeComp and not activeComp.isActive then
                for _, slider in ipairs(self._sliders) do
                    slider.isDragging = false
                end
                return
            end
        end

        if self._pendingReset then
            self._pendingReset = false
            for _, slider in ipairs(self._sliders) do
                self:_initSliderPosition(slider)
            end
        end

        local pointerPressed = Input.IsPointerPressed()

        local gameX, gameY
        if pointerPressed then
            local pointerPos = Input.GetPointerPosition()
            if pointerPos then
                local gc = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
                gameX, gameY = gc[1], gc[2]
            end
        end

        for _, slider in ipairs(self._sliders) do
            if pointerPressed and not slider.isDragging and gameX then
                if gameX >= slider.minX and gameX <= slider.maxX and
                   gameY >= slider.minY and gameY <= slider.maxY then
                    slider.isDragging = true
                    if event_bus and event_bus.publish then
                        event_bus.publish("main_menu.slider", {})
                    end
                end
            end

            if not pointerPressed then
                slider.isDragging = false
            end

            if slider.isDragging and pointerPressed and gameX then
                local clampedX = math.max(slider.minX, math.min(gameX, slider.maxX))
                slider.notchTransform.localPosition.x = clampedX
                slider.notchTransform.isDirty = true

                local normalized = (clampedX - slider.minX) / (slider.maxX - slider.minX)
                if normalized < 0.01 then normalized = 0 end
                if normalized > 0.99 then normalized = 1.0 end

                if slider.fillSprite then slider.fillSprite.fillValue = normalized end

                local api = self._settingsAPI[slider.settingType]
                if api then
                    local range = self._valueRanges[slider.settingType] or { 0.0, 1.0 }
                    api[2](range[1] + normalized * (range[2] - range[1]))
                end
            end
        end
    end,

    _initSliderPosition = function(self, slider)
        local api   = self._settingsAPI[slider.settingType]
        local range = self._valueRanges[slider.settingType] or { 0.0, 1.0 }
        if not api then return end

        local value      = api[1]()
        local normalized = (value - range[1]) / (range[2] - range[1])
        slider.notchTransform.localPosition.x = slider.minX + (normalized * (slider.maxX - slider.minX))
        slider.notchTransform.isDirty = true
        if slider.fillSprite then slider.fillSprite.fillValue = normalized end
        api[2](value)  -- re-apply so the renderer reflects the value (needed after reset)
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe and self._settingsResetSub then
            event_bus.unsubscribe(self._settingsResetSub)
            self._settingsResetSub = nil
        end
    end,
}
