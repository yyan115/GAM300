--[[
================================================================================
MINIBOSS AI AUDIO
================================================================================
PURPOSE:
    Centralised manager (attached to EnemyBossAudioManager entity) that plays
    SFX for the miniboss. Subscribes to "miniboss_sfx" events and plays the
    appropriate clip on the boss's AudioComponent.

SINGLE RESPONSIBILITY: Play miniboss SFX. Nothing else.

EVENTS CONSUMED:
    miniboss_sfx → play SFX based on sfxType:
        "hurt"         → enemyHurtSFX
        "death"        → enemyDeathSFX
        "taunt"        → enemyTauntSFX
        "meleeAttack"  → enemyMeleeAttackSFX
        "meleeHit"     → enemyMeleeHitSFX
        "rangedAttack" → enemyRangedAttackSFX
        "rangedHit"    → enemyRangedHitSFX

FIELDS (populate clip arrays in editor with audio GUIDs):
    enemyHurtSFX         — hurt sounds
    enemyDeathSFX        — death sounds
    enemyTauntSFX        — battlecry / taunt sounds
    enemyMeleeAttackSFX  — melee swing sounds
    enemyMeleeHitSFX     — melee hit impact sounds
    enemyRangedAttackSFX — ranged attack sounds (direct boss audio; not knife spatial)
    enemyRangedHitSFX    — ranged hit impact sounds

NOTE: enemyRangedAttackSFX here covers boss-played ranged SFX (e.g. Phase 3
feather bomb). The knife-throw spatial audio uses a separate field still on
MinibossAI.lua (passed directly to knife:Launch for per-projectile positioning).
================================================================================
--]]

require("extension.engine_bootstrap")
local Component   = require("extension.mono_helper")
local AudioHelper = require("extension.audio_helper")

-- ─────────────────────────────────────────────────────────────────────────────

return Component {

    fields = {
        enemyHurtSFX         = {},
        enemyDeathSFX        = {},
        enemyTauntSFX        = {},
        enemyMeleeAttackSFX  = {},
        enemyMeleeHitSFX     = {},
        enemyRangedAttackSFX = {},
        enemyRangedHitSFX    = {},
    },

    Awake = function(self)
        -- Guard against double-Awake (hot-reload / stop-play cycle)
        if _G.event_bus and _G.event_bus.unsubscribe and self._sfxSub then
            _G.event_bus.unsubscribe(self._sfxSub); self._sfxSub = nil
        end

        if not (_G.event_bus and _G.event_bus.subscribe) then
            print("[MinibossAIAudio] WARNING: event_bus not available in Awake")
            return
        end

        self._sfxSub = _G.event_bus.subscribe("miniboss_sfx", function(data)
            if not data then return end
            local audio = GetComponent(data.entityId, "AudioComponent")
            local t = data.sfxType
            if t == "hurt" then
                AudioHelper.PlayRandomSFX(audio, self.enemyHurtSFX)
            elseif t == "death" then
                AudioHelper.PlayRandomSFX(audio, self.enemyDeathSFX)
            elseif t == "taunt" then
                AudioHelper.PlayRandomSFX(audio, self.enemyTauntSFX)
            elseif t == "meleeAttack" then
                AudioHelper.PlayRandomSFX(audio, self.enemyMeleeAttackSFX)
            elseif t == "meleeHit" then
                AudioHelper.PlayRandomSFX(audio, self.enemyMeleeHitSFX)
            elseif t == "rangedAttack" then
                AudioHelper.PlayRandomSFX(audio, self.enemyRangedAttackSFX)
            elseif t == "rangedHit" then
                AudioHelper.PlayRandomSFX(audio, self.enemyRangedHitSFX)
            end
        end)
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe and self._sfxSub then
            _G.event_bus.unsubscribe(self._sfxSub)
            self._sfxSub = nil
        end
    end,
}
