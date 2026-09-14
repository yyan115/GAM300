-- Resources/Scripts/Gameplay/HitStop.lua
-- Attach to any persistent entity (e.g. Player or GameManager).
--
-- Drives camera effects on hit — chromatic aberration, vignette, screen shake,
-- and a brief colour desaturation — scaled continuously by damage dealt.
-- No longer touches Time.SetTimeScale directly; slow-mo on heavy hits is
-- delegated to camera_effects via fx_time_scale so effects survive time dilation
-- and don't conflict with the slam/dodge slow-mo already published elsewhere.
--
-- Effect tiers (all values lerp between min and max across the damage range):
--   Light  (damage < LightHitThreshold)  : chromatic spike + shake
--   Medium (damage >= LightHitThreshold) : + vignette pulse
--   Heavy  (damage >= HeavyHitThreshold) : + colour desaturation + brief slow-mo
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

return Component {
    fields = {
        -- === Damage thresholds ===
        -- t = 0 at LightHitThreshold, t = 1 at HeavyHitThreshold.
        -- All effect values are lerped between their Min and Max across this range.
        LightHitThreshold = 10,    -- minimum damage that produces any visible effect
        HeavyHitThreshold = 30,    -- damage at which effects are fully maxed out

        -- === Chromatic aberration ===
        -- Fires on every hit above LightHitThreshold.
        ChromaticIntensityMin = 0.25,
        ChromaticIntensityMax = 0.80,
        ChromaticDurationMin  = 0.15,
        ChromaticDurationMax  = 0.40,

        -- === Camera shake ===
        -- Uses the camera_shake event (handled by the camera follow / shake script).
        ShakeIntensityMin = 0.08,
        ShakeIntensityMax = 0.55,
        ShakeDurationMin  = 0.10,
        ShakeDurationMax  = 0.35,
        ShakeFrequency    = 22.0,

        -- === Vignette pulse ===
        -- Fires on medium and heavy hits (damage >= LightHitThreshold).
        VignetteIntensity  = 0.78,
        VignetteSmoothness = 0.30,
        VignetteDurationMin = 0.25,
        VignetteDurationMax = 0.50,

        -- === Color desaturation (heavy hits only, damage past midpoint) ===
        -- Snaps saturation down then springs back — gives a "bone-crunch" feel.
        HeavySaturation       = 0.40,
        HeavyColorDurationMin = 0.12,
        HeavyColorDurationMax = 0.25,

        -- === Slow-mo ===
        -- Brief camera_effects-managed time dilation — does NOT conflict with
        -- slam or dodge slow-mo since camera_effects resolves the highest caller.
        --
        -- Every hit that clears LightHitThreshold gets a dip, lerped from the
        -- Light values to the Heavy ones across the damage range. The dip used
        -- to be gated at t >= 0.5, meaning damage >= 20, so the 10, 12, 14 and
        -- 15 damage hits — five of the nine attacks in the combo tree, and the
        -- ones a player lands most — produced shake and colour and no pause at
        -- all. A short pause on impact is the main thing that makes a hit read
        -- as having connected with something solid, and it was switched off for
        -- the majority of hits.
        --
        -- Durations are in frames at 60 Hz: 0.05 is three frames, 0.12 is seven.
        -- Light hits want to be felt without interrupting the flow of a combo;
        -- the heavy ones want to land.
        LightTimeScale         = 0.55,
        LightTimeScaleDuration = 0.05,
        HeavyTimeScale         = 0.25,
        HeavyTimeScaleDuration = 0.12,
    },

    Start = function(self)
        self._subDmg = nil
        self._subDied = nil
        self._subRespawn = nil
        -- Entities that have already died. AttackHitbox tests the target's tag
        -- and whether it was hit this swing, and nothing else, so a corpse
        -- still carries the Enemy tag and its collider and still publishes
        -- deal_damage_to_entity when the weapon passes through it. EnemyHealth
        -- ignores that because it keeps its own _isDead; every other
        -- subscriber, this one included, took it at face value and played the
        -- full impact effect for a hit on a body lying on the floor.
        self._dead = {}

        if event_bus and event_bus.subscribe then
            self._subDied = event_bus.subscribe("enemy_died", function(payload)
                if payload and payload.entityId then
                    self._dead[payload.entityId] = true
                end
            end)

            -- The table outlives a scene, so ids from the previous run would
            -- suppress hitstop on whatever reused them.
            self._subRespawn = event_bus.subscribe("respawnPlayer", function()
                self._dead = {}
            end)

            self._subDmg = event_bus.subscribe("deal_damage_to_entity", function(payload)
                if not payload or not payload.damage or payload.damage <= 0 then return end
                if payload.entityId and self._dead[payload.entityId] then return end
                self:_trigger(payload.damage, payload.hitType)
            end)
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe then
            for _, key in ipairs({"_subDmg", "_subDied", "_subRespawn"}) do
                if self[key] then
                    event_bus.unsubscribe(self[key])
                    self[key] = nil
                end
            end
        end
        self._dead = {}
    end,

    -- No Update needed — all effects are owned by camera_effects and the shake system.

    _trigger = function(self, damage, hitType)
        local light = tonumber(self.LightHitThreshold) or 10
        local heavy = tonumber(self.HeavyHitThreshold) or 30

        -- Skip sub-threshold hits entirely (chip damage, DoT ticks, etc.)
        if damage < light then return end

        -- t: 0.0 = just reached light threshold, 1.0 = at or above heavy threshold
        local t = math.min(1.0, (damage - light) / math.max(heavy - light, 1))

        local function lerp(a, b, x) return a + (b - a) * x end
        local eb = event_bus
        if not (eb and eb.publish) then return end

        -- ── Chromatic aberration ─────────────────────────────────────────────
        eb.publish("fx_chromatic", {
            intensity = lerp(
                tonumber(self.ChromaticIntensityMin) or 0.25,
                tonumber(self.ChromaticIntensityMax) or 0.80, t),
            duration  = lerp(
                tonumber(self.ChromaticDurationMin)  or 0.15,
                tonumber(self.ChromaticDurationMax)  or 0.40, t),
        })

        -- ── Camera shake ─────────────────────────────────────────────────────
        eb.publish("camera_shake", {
            intensity = lerp(
                tonumber(self.ShakeIntensityMin) or 0.08,
                tonumber(self.ShakeIntensityMax) or 0.55, t),
            duration  = lerp(
                tonumber(self.ShakeDurationMin)  or 0.10,
                tonumber(self.ShakeDurationMax)  or 0.35, t),
            frequency = tonumber(self.ShakeFrequency) or 22.0,
        })

        -- ── Vignette pulse ───────────────────────────────────────────────────
        eb.publish("fx_vignette", {
            intensity  = tonumber(self.VignetteIntensity)  or 0.78,
            smoothness = tonumber(self.VignetteSmoothness) or 0.30,
            duration   = lerp(
                tonumber(self.VignetteDurationMin) or 0.25,
                tonumber(self.VignetteDurationMax) or 0.50, t),
        })

        -- ── Time dip, on every hit, scaled with damage ───────────────────────
        -- Unscaled timer inside camera_effects, so it survives its own dilation.
        eb.publish("fx_time_scale", {
            scale    = lerp(
                tonumber(self.LightTimeScale)         or 0.55,
                tonumber(self.HeavyTimeScale)         or 0.25, t),
            duration = lerp(
                tonumber(self.LightTimeScaleDuration) or 0.05,
                tonumber(self.HeavyTimeScaleDuration) or 0.12, t),
        })

        -- ── Heavy-hit extras (past the halfway point between thresholds) ─────
        if t >= 0.5 then
            -- Colour desaturation snap
            eb.publish("fx_color_grading", {
                saturation = tonumber(self.HeavySaturation) or 0.40,
                duration   = lerp(
                    tonumber(self.HeavyColorDurationMin) or 0.12,
                    tonumber(self.HeavyColorDurationMax) or 0.25,
                    (t - 0.5) * 2.0),
            })
        end
    end,
}