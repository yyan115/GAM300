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
    player_footstep      → play staircaseFootstepSFX if near a Staircase-tagged entity, else playerFootstepSFX
    featherCollected     → play featherPickupSFX
    feather_skill_start  → play featherSkillStartSFX
    feather_skill_release→ play featherSkillReleaseSFX
    playerHeal           → play playerHealSFX

FIELDS (populate clip arrays in editor with audio GUIDs):
    playerFootstepSFX    — footstep sounds while running on regular surfaces
    staircaseFootstepSFX — footstep sounds while running on Staircase-tagged surfaces
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
        staircaseFootstepSFX = {},
        playerHurtSFX        = {},
        playerJumpSFX        = {},
        playerLandSFX        = {},
        playerDeadSFX        = {},
        playerDashSFX        = {},
        featherPickupSFX     = {},
        featherSkillStartSFX = {},
        featherSkillReleaseSFX = {},
        playerHealSFX        = {},

        FootstepVolume       = 0.5,
    },

    -- Raycast downward from the player and walk up the hierarchy checking for
    -- a "Staircase" tag. Returns true when any ancestor of the hit entity is
    -- tagged Staircase (handles prefab hierarchies where the tagged entity is
    -- the parent, not the direct collider owner).
    _isOnStaircase = function(self)
        if not (self._playerTr and Physics and Physics.RaycastGetEntity) then return false end
        local pp = self._playerTr.worldPosition
        if not pp then return false end

        local ok, r1, r2 = pcall(Physics.RaycastGetEntity,
            pp.x, pp.y + 0.1, pp.z,   -- origin just above feet
            0, -1, 0,                   -- straight down
            2.0)                        -- 2 m below feet
        if not ok then return false end

        local hitId = r2 or -1
        if type(r1) == "table" or type(r1) == "userdata" then
            hitId = (r1.entityId or r1[2]) or -1
        end
        if hitId < 0 then return false end

        -- Walk up to 6 levels of hierarchy checking for Staircase tag
        local id = hitId
        for _ = 1, 6 do
            local tagComp = GetComponent(id, "TagComponent")
            if tagComp and Tag.Compare(tagComp.tagIndex, "Staircase") then
                return true
            end
            local parentId = Engine.GetParentEntity(id)
            if not parentId or parentId < 0 then break end
            id = parentId
        end
        return false
    end,

    Awake = function(self)
        self._audio = nil

        -- Guard against double-Awake (hot-reload / stop-play cycle): unsubscribe
        -- any stale tokens before re-subscribing so we never get two listeners.
        if _G.event_bus and _G.event_bus.unsubscribe then
            local stale = {
                "_deadSub", "_hurtSub", "_jumpedSub", "_landedSub", "_dashedSub",
                "_footstepSub", "_featherPickupSub", "_featherSkillStartSub",
                "_featherSkillReleaseSub", "_healSub",
            }
            for _, key in ipairs(stale) do
                if self[key] then _G.event_bus.unsubscribe(self[key]); self[key] = nil end
            end
        end

        -- Enable AudioHelper debug output for dash clips only.
        -- if self.playerFootstepSFX then
        --     self.playerFootstepSFX._debug = true
        -- end

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
            local clips = self.playerFootstepSFX
            if #self.staircaseFootstepSFX > 0 and self:_isOnStaircase() then
                clips = self.staircaseFootstepSFX
            end
            AudioHelper.PlayRandomSFX(self._audio, clips, self.FootstepVolume)
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
            self._audio    = GetComponent(playerEntityId, "AudioComponent")
            self._playerTr = GetComponent(playerEntityId, "Transform")
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
