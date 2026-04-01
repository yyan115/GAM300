--[[
================================================================================
PAUSE MENU AUDIO
================================================================================
PURPOSE:
    Centralized audio handler for all pause menu button hover/click SFX.
    Subscribes to button hover/click events via event_bus and plays SFX.
    
SINGLE RESPONSIBILITY: Play pause menu UI sounds. Nothing else.

USAGE:
    1. Attach this script to a UI entity with an AudioComponent (e.g., PauseMenuUI)
    2. Configure SFX GUIDs in the fields table via inspector
    3. Button handlers publish events: "pause_menu.hover" and "pause_menu.click"
    
FIELDS:
    HoverSFX   - Array of hover sound GUIDs (random selection)
    ClickSFX   - Array of click sound GUIDs (random selection)
================================================================================
--]]

require("extension.engine_bootstrap")
local Component   = require("extension.mono_helper")
local AudioHelper = require("extension.audio_helper")

return Component {

    fields = {
        HoverSFX = {},
        ClickSFX = {},
        
        PitchVariation  = 0.08,
        BaseVolume      = 1.0,
    },

    Awake = function(self)
        self._audio = nil
        
        -- Guard against double-Awake (hot-reload / stop-play cycle)
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._hoverSub then _G.event_bus.unsubscribe(self._hoverSub); self._hoverSub = nil end
            if self._clickSub then _G.event_bus.unsubscribe(self._clickSub); self._clickSub = nil end
        end

        if _G.event_bus and _G.event_bus.subscribe then
            self._hoverSub = _G.event_bus.subscribe("pause_menu.hover", function(data)
                if not self._audio then return end
                AudioHelper.PlayRandomSFXPitched(self._audio, self.HoverSFX, self.PitchVariation, self.BaseVolume)
            end)
            
            self._clickSub = _G.event_bus.subscribe("pause_menu.click", function(data)
                if not self._audio then return end
                AudioHelper.PlayRandomSFXPitched(self._audio, self.ClickSFX, self.PitchVariation, self.BaseVolume)
            end)
        end
    end,

    Start = function(self)
        -- Try to get AudioComponent from this entity
        self._audio = self:GetComponent("AudioComponent")
        
        if not self._audio then
            -- Fallback: try PauseMenuUI entity
            local pauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
            if pauseUIEntity then
                self._audio = GetComponent(pauseUIEntity, "AudioComponent")
            end
        end
        
        if not self._audio then
            print("[PauseMenuAudio] WARNING: no AudioComponent found — add one to this entity or PauseMenuUI")
        end
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._hoverSub then _G.event_bus.unsubscribe(self._hoverSub) end
            if self._clickSub then _G.event_bus.unsubscribe(self._clickSub) end
        end
    end,
}
