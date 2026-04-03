--[[
================================================================================
MAIN MENU AUDIO
================================================================================
PURPOSE:
    Centralized audio handler for all main menu UI SFX (hover, click, slider).
    Subscribes to events via event_bus and plays appropriate sounds.
    
SINGLE RESPONSIBILITY: Play main menu UI sounds. Nothing else.

USAGE:
    1. Attach this script to a UI entity with an AudioComponent (e.g., MainMenuUI or BGM)
    2. Configure SFX GUIDs in the fields table via inspector
    3. UI handlers publish events: "main_menu.hover", "main_menu.click", "main_menu.slider"
    
FIELDS:
    HoverSFX   - Array of hover sound GUIDs (random selection)
    ClickSFX   - Array of click sound GUIDs (random selection)
    SliderSFX  - Array of slider interaction sound GUIDs

================================================================================
--]]

require("extension.engine_bootstrap")
local Component   = require("extension.mono_helper")
local AudioHelper = require("extension.audio_helper")

return Component {

    fields = {
        HoverSFX = {},
        ClickSFX = {},
        SliderSFX = {},
        PlayGameSFX = {},
        
        PitchVariation  = 0.08,
        BaseVolume      = 1.0,
    },

    Awake = function(self)
        self._audio = nil
        
        -- Guard against double-Awake (hot-reload / stop-play cycle)
        if _G.event_bus and _G.event_bus.unsubscribe then
            local stale = { "_hoverSub", "_clickSub", "_sliderSub" }
            for _, key in ipairs(stale) do
                if self[key] then _G.event_bus.unsubscribe(self[key]); self[key] = nil end
            end
        end

        if not (_G.event_bus and _G.event_bus.subscribe) then
            print("[MainMenuAudio] WARNING: event_bus not available in Awake")
            return
        end

        self._hoverSub = _G.event_bus.subscribe("main_menu.hover", function(data)
            if not self._audio then return end
            AudioHelper.PlayRandomSFXPitched(self._audio, self.HoverSFX, self.PitchVariation, self.BaseVolume)
        end)
        
        self._clickSub = _G.event_bus.subscribe("main_menu.click", function(data)
            if not self._audio then return end
            AudioHelper.PlayRandomSFXPitched(self._audio, self.ClickSFX, self.PitchVariation, self.BaseVolume)
        end)
        self._clickplaygameSub = _G.event_bus.subscribe("main_menu.clickplaygame", function(data)
            if not self._audio then return end
            AudioHelper.PlayRandomSFX(self._audio, self.PlayGameSFX, self.BaseVolume)
        end)

        self._sliderSub = _G.event_bus.subscribe("main_menu.slider", function(data)
            if not self._audio then return end
            AudioHelper.PlayRandomSFX(self._audio, self.SliderSFX, self.BaseVolume)
        end)
    end,

    Start = function(self)
        -- Try to get AudioComponent from this entity
        self._audio = self:GetComponent("AudioComponent")
        
        if not self._audio then
            -- Fallback: try MainMenuUI or BGM entity
            local mainMenuEntity = Engine.GetEntityByName("MainMenuUI")
            if mainMenuEntity then
                self._audio = GetComponent(mainMenuEntity, "AudioComponent")
            end
        end
        
        if not self._audio then
            local bgmEntity = Engine.GetEntityByName("BGM")
            if bgmEntity then
                self._audio = GetComponent(bgmEntity, "AudioComponent")
            end
        end
        
        if not self._audio then
            print("[MainMenuAudio] WARNING: no AudioComponent found — add one to this entity, MainMenuUI, or BGM")
        end
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            local subs = { "_hoverSub", "_clickSub", "_sliderSub" }
            for _, key in ipairs(subs) do
                if self[key] then _G.event_bus.unsubscribe(self[key]); self[key] = nil end
            end
        end
    end,
}
