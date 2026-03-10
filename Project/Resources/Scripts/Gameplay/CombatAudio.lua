--[[
================================================================================
COMBAT AUDIO
================================================================================
PURPOSE:
    Reacts to combat events published by ComboManager and plays the appropriate
    SFX. Follows the same pattern as ChainAudio: one system publishes, this
    system listens and plays sounds.

SINGLE RESPONSIBILITY: Play combat SFX. Nothing else.

EVENTS CONSUMED:
    attack_performed { state } → play per-state attack SFX

FIELDS (populate clip arrays in editor with audio GUIDs):
    SlashSFX        — light_1, light_2, light_3 swings
    HeavySlashSFX   — heavy_release swing
    ChainStrikeSFX  — chain_attack impact

AUTHOR: Soh Wei Jie
VERSION: 1.1
================================================================================
--]]

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

-- ── Local helpers (plain functions, no Component binding issues) ──────────────

local function pickRandom(clips)
    if not clips or #clips == 0 then return nil end
    return clips[math.random(1, #clips)]
end

local function playForState(audio, clipsMap, state, baseVolume, pitchVar, volVar)
    if not audio then return end
    local clips = clipsMap[state]
    if not clips or #clips == 0 then return end
    local clip = pickRandom(clips)
    if not clip or clip == "" then return end
    pcall(function()
        local pitch = 1.0 + (math.random() * 2 - 1) * pitchVar
        local vol   = baseVolume + (math.random() * 2 - 1) * volVar
        audio:SetPitch(math.max(0.5, pitch))
        audio:SetVolume(math.max(0.0, math.min(1.0, vol)))
        audio:PlayOneShot(clip)
    end)
end

-- ─────────────────────────────────────────────────────────────────────────────

return Component {

    fields = {
        SlashSFX       = {},
        HeavySlashSFX  = {},
        ChainStrikeSFX = {},

        PitchVariation  = 0.08,
        VolumeVariation = 0.06,
        BaseVolume      = 1.0,
    },

    Awake = function(self)
        self._audio    = nil
        self._clipsMap = nil   -- built in Start once inspector fields are resolved

        if _G.event_bus and _G.event_bus.subscribe then
            self._attackSub = _G.event_bus.subscribe("attack_performed", function(data)
                if not data or not data.state then return end
                if not self._audio or not self._clipsMap then return end
                playForState(
                    self._audio,
                    self._clipsMap,
                    data.state,
                    self.BaseVolume,
                    self.PitchVariation,
                    self.VolumeVariation
                )
            end)
        end
    end,

    Start = function(self)
        -- Try the entity this component sits on first, fall back to Player entity.
        self._audio = self:GetComponent("AudioComponent")
        if not self._audio then
            local playerEntityId = Engine.GetEntityByName("Player")
            if playerEntityId then
                self._audio = GetComponent(playerEntityId, "AudioComponent")
            end
        end
        if not self._audio then
            print("[CombatAudio] WARNING: no AudioComponent found — add one to this entity or the Player entity")
        end

        -- Build the state → clip array map from inspector fields.
        -- To add audio for a new combo state, add one line here.
        self._clipsMap = {
            light_1       = self.SlashSFX,
            light_2       = self.SlashSFX,
            light_3       = self.SlashSFX,
            heavy_release = self.HeavySlashSFX,
            chain_attack  = self.ChainStrikeSFX,
        }
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._attackSub then _G.event_bus.unsubscribe(self._attackSub) end
        end
    end,
}