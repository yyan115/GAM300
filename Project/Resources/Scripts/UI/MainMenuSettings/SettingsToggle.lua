require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

-- Unified Settings Toggle Script
-- Attach to a checkbox/toggle entity
return Component {
    fields = {
        settingType = "vsync",       -- "vsync", "fullscreen", "bloom", "ssao", etc.
        onSprite = "",               -- Optional: sprite to show when ON
        offSprite = "",              -- Optional: sprite to show when OFF
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
        -- Helper to strip quotes
        local function stripQuotes(str)
            if type(str) ~= "string" then return str end
            return str:gsub('^"(.*)"$', '%1'):gsub("^'(.*)'$", '%1')
        end

        self._settingType = string.lower(stripQuotes(self.settingType) or "vsync")
        
        -- Cache components
        self._button = GetComponent(self.entity, "ButtonComponent")
        self._sprite = GetComponent(self.entity, "SpriteRenderComponent")

        -- Set initial state
        self:UpdateVisuals()
    end,

    Update = function(self, dt)
        if self._pendingReset then
            self._pendingReset = false
            self:UpdateVisuals()
        end

        -- Check for button click
        if self._button and self._button.isClicked then
            local current = self:GetCurrentValue()
            self:SetCurrentValue(not current)
            self:UpdateVisuals()
            
            if event_bus and event_bus.publish then
                event_bus.publish("main_menu.click", {})
            end
        end
    end,

    UpdateVisuals = function(self)
        local val = self:GetCurrentValue()
        -- You could swap sprites here if provided in fields
        -- For now, maybe change color as a placeholder
        if self._sprite then
            if val then
                self._sprite.color = {1, 1, 1, 1} -- White (ON)
            else
                self._sprite.color = {0.5, 0.5, 0.5, 1} -- Gray (OFF)
            end
        end
    end,

    GetCurrentValue = function(self)
        if self._settingType == "vsync" then return GameSettings.GetVSync()
        elseif self._settingType == "fullscreen" then return GameSettings.GetFullscreen()
        elseif self._settingType == "bloom" then return GameSettings.GetBloomEnabled()
        elseif self._settingType == "vignette" then return GameSettings.GetVignetteEnabled()
        elseif self._settingType == "colorgrading" then return GameSettings.GetColorGradingEnabled()
        elseif self._settingType == "ca" then return GameSettings.GetCAEnabled()
        elseif self._settingType == "ssao" then return GameSettings.GetSSAOEnabled()
        end
        return false
    end,

    SetCurrentValue = function(self, value)
        if self._settingType == "vsync" then GameSettings.SetVSync(value)
        elseif self._settingType == "fullscreen" then GameSettings.SetFullscreen(value)
        elseif self._settingType == "bloom" then GameSettings.SetBloomEnabled(value)
        elseif self._settingType == "vignette" then GameSettings.SetVignetteEnabled(value)
        elseif self._settingType == "colorgrading" then GameSettings.SetColorGradingEnabled(value)
        elseif self._settingType == "ca" then GameSettings.SetCAEnabled(value)
        elseif self._settingType == "ssao" then GameSettings.SetSSAOEnabled(value)
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe and self._settingsResetSub then
            event_bus.unsubscribe(self._settingsResetSub)
            self._settingsResetSub = nil
        end
    end,
}
