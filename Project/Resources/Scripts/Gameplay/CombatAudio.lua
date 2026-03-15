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
    attack_performed     { state } → play swing SFX (skipped if a hit occurred this frame)
    deal_damage_to_entity         → play SlashHitSFX (suppresses swing SFX same frame)
    knifeHitPlayerDmg             → play EnemyRangedHitSFX

FIELDS (populate clip arrays in editor with audio GUIDs):
    SlashSFX           — light_1, light_2, light_3 swings (plays on miss)
    HeavySlashSFX      — heavy_release swing
    ChainStrikeSFX     — chain_attack impact
    SlashHitSFX        — flesh impact when player weapon hits enemy
    EnemyRangedHitSFX  — impact when enemy knife hits player

AUTHOR: Soh Wei Jie
VERSION: 1.2
================================================================================
--]]

require("extension.engine_bootstrap")
local Component     = require("extension.mono_helper")
local AudioHelper   = require("extension.audio_helper")

-- ─────────────────────────────────────────────────────────────────────────────

return Component {

    fields = {
        SlashSFX           = {},
        HeavySlashSFX      = {},
        ChainStrikeSFX     = {},
        SlashHitSFX        = {},
        EnemyRangedHitSFX  = {},

        PitchVariation  = 0.08,
        VolumeVariation = 0.06,
        BaseVolume      = 1.0,
    },

    Awake = function(self)
        self._audio         = nil
        self._clipsMap      = nil   -- built in Start once inspector fields are resolved
        self._hitThisFrame  = false -- set by deal_damage_to_entity to suppress swing SFX

        if _G.event_bus and _G.event_bus.subscribe then
            -- Subscribe deal_damage BEFORE attack_performed so the flag is set first
            -- when both events are published in the same frame (melee hit case).
            self._hitSub = _G.event_bus.subscribe("deal_damage_to_entity", function(data)
                self._hitThisFrame = true
                AudioHelper.PlayRandomSFXPitched(self._audio, self.SlashHitSFX, self.PitchVariation, self.BaseVolume)
            end)

            self._attackSub = _G.event_bus.subscribe("attack_performed", function(data)
                if not data or not data.state then return end
                if not self._audio or not self._clipsMap then return end
                if self._hitThisFrame then
                    self._hitThisFrame = false
                    return  -- hit sound already playing; skip swing SFX
                end
                AudioHelper.PlayRandomSFXPitched(self._audio, self._clipsMap[data.state], self.PitchVariation, self.BaseVolume)
            end)

            self._rangedHitSub = _G.event_bus.subscribe("knifeHitPlayerDmg", function(data)
                AudioHelper.PlayRandomSFXPitched(self._audio, self.EnemyRangedHitSFX, self.PitchVariation, self.BaseVolume)
            end)
        end
    end,

    Start = function(self)
        -- Try the entity this component sits on first, fall back to Player entity.
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
            if self._hitSub      then _G.event_bus.unsubscribe(self._hitSub)      end
            if self._attackSub   then _G.event_bus.unsubscribe(self._attackSub)   end
            if self._rangedHitSub then _G.event_bus.unsubscribe(self._rangedHitSub) end
        end
    end,
}
