--[[
================================================================================
ENVIRONMENT AUDIO
================================================================================
PURPOSE:
    Reacts to environment events (weapon pickup, checkpoint heal, door open) and
    plays the appropriate SFX. Follows the same pattern as PlayerAudio: one system
    publishes, this system listens and plays sounds.

SINGLE RESPONSIBILITY: Play environment SFX. Nothing else.

EVENTS CONSUMED:
    picked_up_weapon  → stop pickupAuraSFX looping on the WeaponPickup entity
    env_weapon_caught → play pickupSFX      (weapon physically reaches player's hand)
    env_door_opened   → play doorOpenSFX    (a door begins to open)
    game_paused       → Pause/UnPause the WeaponPickup hover audio

FIELDS (populate in editor):
    weaponPickupEntityName — name of the WeaponPickup entity (for positional hover audio)
    pickupSFX              — sound when weapon is physically caught by the player
    doorOpenSFX            — sound when a door opens
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
    },

    Awake = function(self)
        self._audio              = nil
        self._pickupAudio        = nil
        self._pickupAudioStopped = false

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

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            local subs = {
                "_pickupAuraSub", "_pickupSub", "_doorSub", "_gamePausedSub",
            }
            for _, key in ipairs(subs) do
                if self[key] then _G.event_bus.unsubscribe(self[key]); self[key] = nil end
            end
        end
    end,
}
