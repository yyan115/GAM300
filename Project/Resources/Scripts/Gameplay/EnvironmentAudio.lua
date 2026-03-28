--[[
================================================================================
ENVIRONMENT AUDIO
================================================================================
PURPOSE:
    Reacts to environment events (weapon pickup, checkpoint heal, door open) and
    plays the appropriate SFX. Also manages BGM transitions for boss encounters.
    Follows the same pattern as PlayerAudio: one system publishes, this system
    listens and plays sounds.

SINGLE RESPONSIBILITY: Play environment SFX and manage BGM transitions.

EVENTS CONSUMED:
    picked_up_weapon       → stop pickupAuraSFX looping on the WeaponPickup entity
    env_weapon_caught      → play pickupSFX      (weapon physically reaches player's hand)
    env_door_opened        → play doorOpenSFX    (a door begins to open)
    game_paused            → Pause/UnPause the WeaponPickup hover audio
    boss_narrative_started → fade BGM bus down to BGMNarrativeVolume
    boss_narrative_ended   → stop BGM1, play BossBGM, fade BGM bus back to full
    boss_killed            → stop BossBGM, unmute+play BGM1, fade BGM bus back to full
    playerDead             → stop BossBGM, restore BGM bus instantly (death screen takes over)
    respawnPlayer          → stop BossBGM, unmute+play BGM1, fade BGM bus back to full

FIELDS (populate in editor):
    weaponPickupEntityName — name of the WeaponPickup entity (for positional hover audio)
    pickupSFX              — sound when weapon is physically caught by the player
    doorOpenSFX            — sound when a door opens
    BGMFadeDuration        — seconds to fade BGM bus in/out (default 2.0)
    BGMNarrativeVolume     — target BGM bus volume during narrative (default 0.1)
================================================================================
--]]

require("extension.engine_bootstrap")
local Component   = require("extension.mono_helper")
local AudioHelper = require("extension.audio_helper")

-- ─────────────────────────────────────────────────────────────────────────────

return Component {

    fields = {
        weaponPickupEntityName = "LowPolyFeatherChainPickUp",
        pickupSFX              = {},
        doorOpenSFX            = {},
        BGMFadeDuration        = 2.0,
        BGMNarrativeVolume     = 0.1,
    },

    Awake = function(self)
        self._audio              = nil
        self._pickupAudio        = nil
        self._pickupAudioStopped = false

        -- BGM fade state
        self._bgmFadeActive = false
        self._bgmFadeTimer  = 0.0
        self._bgmFadeFrom   = 1.0
        self._bgmFadeTo     = 1.0

        if not (_G.event_bus and _G.event_bus.subscribe) then
            print("[EnvironmentAudio] WARNING: event_bus not available in Awake")
            return
        end

        -- Pickup aura (looping hover) — stop it once the weapon starts flying
        self._pickupAuraSub = _G.event_bus.subscribe("picked_up_weapon", function(data)
            if data and self._pickupAudio then
                self._pickupAudio:Stop()
                self._pickupAudioStopped = true
            end
        end)

        -- Pause/unpause the looping hover audio with the game
        self._gamePausedSub = _G.event_bus.subscribe("game_paused", function(paused)
            if not self._pickupAudio or self._pickupAudioStopped then return end
            if paused then
                self._pickupAudio:Pause()
            else
                self._pickupAudio:UnPause()
            end
        end)

        -- Weapon caught — fires when the weapon physically reaches the player's hand
        self._pickupSub = _G.event_bus.subscribe("env_weapon_caught", function(_)
            AudioHelper.PlayRandomSFX(self._audio, self.pickupSFX)
        end)

        -- Door opened
        self._doorSub = _G.event_bus.subscribe("env_door_opened", function(_)
            AudioHelper.PlayRandomSFX(self._audio, self.doorOpenSFX)
        end)

        -- Boss narrative started — fade BGM bus down to narrative volume
        self._narrativeStartedSub = _G.event_bus.subscribe("boss_narrative_started", function(_)
            self._bgmFadeFrom   = Audio.GetBusVolume("BGM")
            self._bgmFadeTo     = self.BGMNarrativeVolume or 0.1
            self._bgmFadeTimer  = 0.0
            self._bgmFadeActive = true
        end)

        -- Boss narrative ended — stop BGM1, play BossBGM, fade bus back up
        self._narrativeEndedSub = _G.event_bus.subscribe("boss_narrative_ended", function(_)
            -- Stop BGM1 while bus is still faded (inaudible transition). 
            local bgm1Ent = Engine.GetEntityByName("BGM1")
            if bgm1Ent then
                local bgm1Audio = GetComponent(bgm1Ent, "AudioComponent")
                if bgm1Audio then
                    bgm1Audio:Stop()
                    bgm1Audio:SetMute(true)
                end
            end

            -- Start BossBGM (has Loop=true, PlayOnAwake=false)
            local bossBGMEnt = Engine.GetEntityByName("BossBGM")
            if bossBGMEnt then
                local bossBGMAudio = GetComponent(bossBGMEnt, "AudioComponent")
                if bossBGMAudio then bossBGMAudio:Play() end
            end

            -- Fade BGM bus back to full — BossBGM fades in
            self._bgmFadeFrom   = Audio.GetBusVolume("BGM")
            self._bgmFadeTo     = 1.0
            self._bgmFadeTimer  = 0.0
            self._bgmFadeActive = true
        end)

        -- Boss killed — restore BGM1, fade bus back up
        self._bossKilledSub = _G.event_bus.subscribe("boss_killed", function(_)
            self:_restoreBGM1()
        end)

        -- Player died — stop BossBGM and restore bus instantly so DeathScreenBGM plays cleanly
        self._playerDeadSub = _G.event_bus.subscribe("playerDead", function(dead)
            if not dead then return end
            self._bgmFadeActive = false
            local bossBGMEnt = Engine.GetEntityByName("BossBGM")
            if bossBGMEnt then
                local bossBGMAudio = GetComponent(bossBGMEnt, "AudioComponent")
                if bossBGMAudio then bossBGMAudio:Stop() end
            end
            Audio.SetBusVolume("BGM", 1.0)
        end)

        -- Respawn — restore BGM1 fully (mute state persists across respawn)
        self._respawnPlayerSub = _G.event_bus.subscribe("respawnPlayer", function(respawn)
            if not respawn then return end
            self:_restoreBGM1()
        end)
    end,

    -- Shared helper: stop BossBGM, unmute+restart BGM1, fade bus to full
    _restoreBGM1 = function(self)
        local bossBGMEnt = Engine.GetEntityByName("BossBGM")
        if bossBGMEnt then
            local bossBGMAudio = GetComponent(bossBGMEnt, "AudioComponent")
            if bossBGMAudio then bossBGMAudio:Stop() end
        end

        local bgm1Ent = Engine.GetEntityByName("BGM1")
        if bgm1Ent then
            local bgm1Audio = GetComponent(bgm1Ent, "AudioComponent")
            if bgm1Audio then
                bgm1Audio:SetMute(false)
                bgm1Audio:Play()
            end
        end

        self._bgmFadeFrom   = Audio.GetBusVolume("BGM")
        self._bgmFadeTo     = 1.0
        self._bgmFadeTimer  = 0.0
        self._bgmFadeActive = true
    end,

    Start = function(self)
        -- AudioEnvManager's own AudioComponent (for pickup + door SFX)
        self._audio = self:GetComponent("AudioComponent")
        if not self._audio then
            print("[EnvironmentAudio] WARNING: no AudioComponent found on AudioEnvManager entity")
        end

        -- WeaponPickup entity's AudioComponent (positional looping hover SFX)
        local pickupEnt = Engine.GetEntityByName(self.weaponPickupEntityName)
        if pickupEnt then
            self._pickupAudio = GetComponent(pickupEnt, "AudioComponent")
            if self._pickupAudio then
                self._pickupAudio:Play()
            else
                print("[EnvironmentAudio] WARNING: no AudioComponent on " .. self.weaponPickupEntityName)
            end
        else
            print("[EnvironmentAudio] WARNING: entity not found: " .. self.weaponPickupEntityName)
        end
    end,

    Update = function(self, dt)
        if self._bgmFadeActive then
            self._bgmFadeTimer = self._bgmFadeTimer + dt
            local t = math.min(self._bgmFadeTimer / (self.BGMFadeDuration or 2.0), 1.0)
            local vol = self._bgmFadeFrom + (self._bgmFadeTo - self._bgmFadeFrom) * t
            Audio.SetBusVolume("BGM", vol)
            if t >= 1.0 then
                self._bgmFadeActive = false
            end
        end
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            local subs = {
                "_pickupAuraSub", "_pickupSub", "_doorSub", "_gamePausedSub",
                "_narrativeStartedSub", "_narrativeEndedSub",
                "_bossKilledSub", "_playerDeadSub", "_respawnPlayerSub",
            }
            for _, key in ipairs(subs) do
                if self[key] then _G.event_bus.unsubscribe(self[key]); self[key] = nil end
            end
        end
    end,
}
