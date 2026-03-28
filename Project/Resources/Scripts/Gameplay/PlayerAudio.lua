--[[
================================================================================
PLAYER AUDIO
================================================================================
PURPOSE:
    Reacts to player movement and feather skill events and plays the appropriate
    SFX. Follows the same pattern as CombatAudio: one system publishes, this
    system listens and plays sounds.

SINGLE RESPONSIBILITY: Play player SFX. Nothing else.

EVENTS CONSUMED:
    playerDead           → play playerDeadSFX
    playerHurtTriggered  → play playerHurtSFX
    player_jumped        → play playerJumpSFX
    player_landed        → play playerLandSFX
    player_dashed        → play playerDashSFX
    player_footstep      → play playerFootstepSFX at FootstepVolume
    featherCollected     → play featherPickupSFX
    feather_skill_start  → play featherSkillStartSFX
    feather_skill_release→ play featherSkillReleaseSFX
    playerHeal           → play playerHealSFX

FIELDS (populate clip arrays in editor with audio GUIDs):
    playerFootstepSFX    — footstep sounds while running
    playerHurtSFX        — impact sounds when player is hit
    playerJumpSFX        — sounds on jump
    playerLandSFX        — sounds on landing
    playerDeadSFX        — sounds on player death
    playerDashSFX        — sounds on dash
    featherPickupSFX     — sounds when a feather is collected
    featherSkillStartSFX — sounds when feather skill is activated
    featherSkillReleaseSFX — sounds when feather skill releases
    playerHealSFX        — sounds when player heals at a checkpoint
================================================================================
--]]

require("extension.engine_bootstrap")
local Component   = require("extension.mono_helper")
local AudioHelper = require("extension.audio_helper")

-- ─────────────────────────────────────────────────────────────────────────────

return Component {

    fields = {
        playerFootstepSFX    = {},
        playerHurtSFX        = {},
        playerJumpSFX        = {},
        playerLandSFX        = {},
        playerDeadSFX        = {},
        playerDashSFX        = {},
        featherPickupSFX     = {},
        featherSkillStartSFX = {},
        featherSkillReleaseSFX = {},
        playerHealSFX        = {},

        FootstepVolume  = 0.5,
    },

    Awake = function(self)
        self._audio = nil

        if not (_G.event_bus and _G.event_bus.subscribe) then
            print("[PlayerAudio] WARNING: event_bus not available in Awake")
            return
        end

        -- ── Movement ──────────────────────────────────────────────────────────
        self._deadSub = _G.event_bus.subscribe("playerDead", function(data)
            if data then
                AudioHelper.PlayRandomSFX(self._audio, self.playerDeadSFX)
            end
        end)

        self._hurtSub = _G.event_bus.subscribe("playerHurtTriggered", function(data)
            if data then
                AudioHelper.PlayRandomSFX(self._audio, self.playerHurtSFX)
            end
        end)

        self._jumpedSub = _G.event_bus.subscribe("player_jumped", function(_)
            AudioHelper.PlayRandomSFX(self._audio, self.playerJumpSFX)
        end)

        self._landedSub = _G.event_bus.subscribe("player_landed", function(_)
            AudioHelper.PlayRandomSFX(self._audio, self.playerLandSFX)
        end)

        self._dashedSub = _G.event_bus.subscribe("player_dashed", function(_)
            AudioHelper.PlayRandomSFX(self._audio, self.playerDashSFX)
        end)

        self._footstepSub = _G.event_bus.subscribe("player_footstep", function(_)
            AudioHelper.PlayRandomSFX(self._audio, self.playerFootstepSFX, self.FootstepVolume)
        end)

        -- ── Feather skill ─────────────────────────────────────────────────────
        self._featherPickupSub = _G.event_bus.subscribe("featherCollected", function(data)
            if data then
                AudioHelper.PlayRandomSFX(self._audio, self.featherPickupSFX)
            end
        end)

        self._featherSkillStartSub = _G.event_bus.subscribe("feather_skill_start", function(_)
            AudioHelper.PlayRandomSFX(self._audio, self.featherSkillStartSFX)
        end)

        self._featherSkillReleaseSub = _G.event_bus.subscribe("feather_skill_release", function(_)
            AudioHelper.PlayRandomSFX(self._audio, self.featherSkillReleaseSFX)
        end)

        -- ── Checkpoint heal ───────────────────────────────────────────────────
        self._healSub = _G.event_bus.subscribe("playerHeal", function(data)
            if data then
                AudioHelper.PlayRandomSFX(self._audio, self.playerHealSFX)
            end
        end)
    end,

    Start = function(self)
        local playerEntityId = Engine.GetEntityByName("Player")
        if playerEntityId then
            self._audio = GetComponent(playerEntityId, "AudioComponent")
        end
        if not self._audio then
            print("[PlayerAudio] WARNING: no AudioComponent found on Player entity")
        end
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            local subs = {
                "_deadSub", "_hurtSub", "_jumpedSub",
                "_landedSub", "_dashedSub", "_footstepSub",
                "_featherPickupSub", "_featherSkillStartSub", "_featherSkillReleaseSub",
                "_healSub",
            }
            for _, key in ipairs(subs) do
                if self[key] then _G.event_bus.unsubscribe(self[key]); self[key] = nil end
            end
        end
    end,
}
