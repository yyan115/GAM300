--[[
================================================================================
PAUSE MENU AUDIO
================================================================================
PURPOSE:
    Centralized audio handler for ALL pause menu UI SFX (hover, click, slider).
    Subscribes to events via event_bus and plays appropriate sounds.
    Follows the same pattern as PlayerAudio and EnemyAIAudio.
    
SINGLE RESPONSIBILITY: Play pause menu UI sounds. Nothing else.

EVENTS CONSUMED:
    pause_menu.hover   → play HoverSFX (pitched)
    pause_menu.click   → play ClickSFX (pitched)
    pause_menu.slider  → play SliderSFX
    
USAGE:
    1. Attach this script to the PauseMenuHandlers entity (which has AudioComponent)
    2. Configure SFX GUIDs in the fields table via inspector
    3. Button/slider handlers publish events via event_bus

FIELDS:
    HoverSFX   - Array of hover sound GUIDs (random selection with pitch variation)
    ClickSFX   - Array of click sound GUIDs (random selection with pitch variation)
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
            print("[PauseMenuAudio] WARNING: event_bus not available in Awake")
            return
        end

        -- Hover sound
        self._hoverSub = _G.event_bus.subscribe("pause_menu.hover", function(data)
            if not self._audio then return end
            AudioHelper.PlayRandomSFXPitched(self._audio, self.HoverSFX, self.PitchVariation, self.BaseVolume)
        end)
        
        -- Click sound
        self._clickSub = _G.event_bus.subscribe("pause_menu.click", function(data)
            if not self._audio then return end
            AudioHelper.PlayRandomSFXPitched(self._audio, self.ClickSFX, self.PitchVariation, self.BaseVolume)
        end)
        
        -- Slider interaction sound
        self._sliderSub = _G.event_bus.subscribe("pause_menu.slider", function(data)
            if not self._audio then return end
            AudioHelper.PlayRandomSFX(self._audio, self.SliderSFX, self.BaseVolume)
        end)
    end,

    Start = function(self)
        -- Try to get AudioComponent from this entity first
        self._audio = self:GetComponent("AudioComponent")
        
        if not self._audio then
            -- Fallback: try PauseMenuHandlers entity
            local handlersEntity = Engine.GetEntityByName("PauseMenuHandlers")
            if handlersEntity then
                self._audio = GetComponent(handlersEntity, "AudioComponent")
            end
        end
        
        if not self._audio then
            -- Second fallback: try PauseMenuUI entity
            local pauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
            if pauseUIEntity then
                self._audio = GetComponent(pauseUIEntity, "AudioComponent")
            end
        end
        
        if not self._audio then
            print("[PauseMenuAudio] WARNING: no AudioComponent found — add one to PauseMenuHandlers")
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
