--[[
================================================================================
COMBAT AUDIO
================================================================================
PURPOSE:
    Reacts to combat events published by ComboManager and plays the appropriate
    SFX. This is the combat equivalent of ChainAudio: one system publishes,
    this system listens and plays sounds.

SINGLE RESPONSIBILITY: Play combat SFX. Nothing else.

DESIGN:
    ComboManager publishes attack_performed { state, ... }.
    CombatAudio maps state → clip array and plays a random pick.
    Adding a new attack state? Add an entry to the STATE_TO_CLIPS map.
    No code changes to ComboManager needed, ever.

EVENTS CONSUMED:
    attack_performed   { state }  → play per-state attack SFX

FIELDS (populate clip arrays in editor with audio GUIDs):
    Each array holds one or more clip GUIDs. A random one is picked per play
    with slight pitch/volume variation to stop sounds going stale.

    SlashSFX         — light_1, light_2, light_3 swings
    HeavySlashSFX    — heavy_release swing
    ChainStrikeSFX   — chain_attack impact

AUDIO COMPONENT:
    Attach an AudioComponent to the same entity as this script. CombatAudio
    resolves it in Start() via GetComponent.

AUTHOR: Soh Wei Jie
VERSION: 1.0
================================================================================
--]]

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {

    fields = {
        -- Clip arrays — populate with audio GUIDs in the editor.
        -- Each array is sampled randomly on play, with variation applied.
        SlashSFX      = {},   -- light attack swings
        HeavySlashSFX = {},   -- heavy_release swing
        ChainStrikeSFX = {},  -- chain_attack impact

        -- Variation applied per play to prevent sounds going stale
        PitchVariation  = 0.08,  -- +/- fraction (0.08 = +-8%)
        VolumeVariation = 0.06,  -- +/- fraction
        BaseVolume      = 1.0,
    },

    Awake = function(self)
        self._audio = nil

        -- Map combat state id → clip array field name.
        -- To wire up a new attack state, add a line here.
        self._STATE_TO_CLIPS = {
            light_1       = "SlashSFX",
            light_2       = "SlashSFX",
            light_3       = "SlashSFX",
            heavy_release = "HeavySlashSFX",
            chain_attack  = "ChainStrikeSFX",
        }

        if _G.event_bus and _G.event_bus.subscribe then
            self._attackSub = _G.event_bus.subscribe("attack_performed", function(data)
                if not data or not data.state then return end
                self:_playForState(data.state)
            end)
        end
    end,

    Start = function(self)
        -- Try the entity this component is on first.
        -- If there's no AudioComponent here (e.g. CombatAudio sits on the
        -- ComboManager entity which has no audio), fall back to the Player entity.
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
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._attackSub then _G.event_bus.unsubscribe(self._attackSub) end
        end
    end,

    -- ── Internal ────────────────────────────────────────────────────────────

    _playForState = function(self, stateId)
        if not self._audio then return end

        local fieldName = self._STATE_TO_CLIPS[stateId]
        if not fieldName then return end  -- state has no mapped SFX, intentional

        local clips = self[fieldName]
        if not clips or #clips == 0 then return end

        local clip = clips[math.random(1, #clips)]
        if not clip or clip == "" then return end

        pcall(function()
            -- Slight pitch and volume variation per play
            local pitch  = 1.0 + (math.random() * 2 - 1) * self.PitchVariation
            local vol    = self.BaseVolume + (math.random() * 2 - 1) * self.VolumeVariation
            self._audio:SetPitch(math.max(0.5, pitch))
            self._audio:SetVolume(math.max(0.0, math.min(1.0, vol)))
            self._audio:PlayOneShot(clip)
        end)
    end,
}