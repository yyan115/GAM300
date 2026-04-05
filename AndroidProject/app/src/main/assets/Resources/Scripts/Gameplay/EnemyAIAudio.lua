--[[
================================================================================
ENEMY AI AUDIO
================================================================================
PURPOSE:
    Centralised manager (attached to EnemyAudioManager entity) that plays SFX
    for all enemies. Subscribes to every "enemy_sfx" event, fetches the
    AudioComponent of the originating enemy by entityId, and plays the clip.

SINGLE RESPONSIBILITY: Play enemy SFX. Nothing else.

EVENTS CONSUMED:
    enemy_sfx → play SFX based on sfxType:
        "hurt"         → enemyHurtSFX
        "death"        → enemyDeathSFX
        "alert"        → enemyAlertSFX (30-second per-enemy cooldown)
        "meleeAttack"  → enemyMeleeAttackSFX
        "rangedAttack" → enemyRangedAttackSFX
        "meleeHit"     → enemyMeleeHitSFX
        "rangedHit"    → enemyRangedHitSFX
        "footstep"     → enemyFootstepSFX (volume 0.6)
        "groundSlam"   → enemyGroundSlamSFX

FIELDS (populate clip arrays in editor with audio GUIDs):
    enemyHurtSFX         — hurt sounds
    enemyDeathSFX        — death sounds
    enemyAlertSFX        — alert growl when first detecting player
    enemyMeleeAttackSFX  — melee swing sounds
    enemyMeleeHitSFX     — melee hit impact sounds
    enemyRangedAttackSFX — ranged throw sounds
    enemyRangedHitSFX    — ranged hit impact sounds
    enemyFootstepSFX     — footstep sounds
    enemyGroundSlamSFX   — ground slam impact sounds
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
        enemyAlertSFX        = {},
        enemyMeleeAttackSFX  = {},
        enemyMeleeHitSFX     = {},
        enemyRangedAttackSFX = {},
        enemyRangedHitSFX    = {},
        enemyFootstepSFX     = {},
        enemyGroundSlamSFX   = {},
    },

    Awake = function(self)
        self._alertCooldowns = {}   -- [entityId] = seconds remaining

        -- Guard against double-Awake (hot-reload / stop-play cycle)
        if _G.event_bus and _G.event_bus.unsubscribe and self._sfxSub then
            _G.event_bus.unsubscribe(self._sfxSub); self._sfxSub = nil
        end

        if not (_G.event_bus and _G.event_bus.subscribe) then
            --print("[EnemyAIAudio] WARNING: event_bus not available in Awake")
            return
        end

        -- if self.enemyRangedAttackSFX then
        --     self.enemyRangedAttackSFX._debug = true
        -- end

        self._sfxSub = _G.event_bus.subscribe("enemy_sfx", function(data)
            if not data then return end
            local audio = GetComponent(data.entityId, "AudioComponent")
            local t = data.sfxType
            if t == "hurt" then
                AudioHelper.PlayRandomSFX(audio, self.enemyHurtSFX)
            elseif t == "death" then
                AudioHelper.PlayRandomSFX(audio, self.enemyDeathSFX)
            elseif t == "alert" then
                local cd = self._alertCooldowns[data.entityId] or 0
                if cd > 0 then return end
                self._alertCooldowns[data.entityId] = 30
                AudioHelper.PlayRandomSFX(audio, self.enemyAlertSFX, 0.4)
            elseif t == "meleeAttack" then
                AudioHelper.PlayRandomSFX(audio, self.enemyMeleeAttackSFX)
            elseif t == "rangedAttack" then
                AudioHelper.PlayRandomSFXPitched(audio, self.enemyRangedAttackSFX,0.7)
            elseif t == "meleeHit" then
                AudioHelper.PlayRandomSFX(audio, self.enemyMeleeHitSFX)
            elseif t == "rangedHit" then
                AudioHelper.PlayRandomSFX(audio, self.enemyRangedHitSFX)
            elseif t == "footstep" then
                AudioHelper.PlayRandomSFX(audio, self.enemyFootstepSFX, 0.6)
            elseif t == "groundSlam" then
                AudioHelper.PlayRandomSFX(audio, self.enemyGroundSlamSFX)
            end
        end)
    end,

    Update = function(self, dt)
        if not next(self._alertCooldowns) then return end
        local dtSec = dt or 0
        if dtSec > 1.0 then dtSec = dtSec * 0.001 end
        for id, cd in pairs(self._alertCooldowns) do
            local remaining = cd - dtSec
            if remaining <= 0 then
                self._alertCooldowns[id] = nil
            else
                self._alertCooldowns[id] = remaining
            end
        end
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe and self._sfxSub then
            _G.event_bus.unsubscribe(self._sfxSub)
            self._sfxSub = nil
        end
    end,
}
