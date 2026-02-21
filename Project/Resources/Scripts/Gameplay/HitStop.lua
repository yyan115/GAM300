-- Resources/Scripts/Gameplay/HitStop.lua
-- Attach to any persistent entity (e.g. Player or GameManager).
-- Triggers a brief freeze-frame on hit. Duration scales with damage dealt.
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

return Component {
    fields = {
        -- Time scale during hitstop (0 = full freeze, 0.05 = nearly frozen)
        -- 0.05 is recommended — full freeze can cause physics glitches
        timeScale      = 0.0,

        -- Duration scales with damage: duration = damage * scalar, clamped to [min, max]
        -- With default damage=10: 10 * 0.008 = 0.08s (~5 frames) — light hit feel
        -- With damage=25:         25 * 0.008 = 0.2s  (~12 frames) — heavy hit feel
        durationScalar = 0.008,
        minDuration    = 0.05,   -- ~3 frames  — minimum so every hit registers
        maxDuration    = 1.0,    -- TEST: 1 second so it's very obvious
    },

    Start = function(self)
        self._timer  = 0
        self._active = false
        self._subDmg = nil

        print("[HitStop] Started, listening for deal_damage_to_entity")

        if event_bus and event_bus.subscribe then
            self._subDmg = event_bus.subscribe("deal_damage_to_entity", function(payload)
                print("[HitStop] deal_damage_to_entity received, damage=" .. tostring(payload and payload.damage))
                if not payload or not payload.damage or payload.damage <= 0 then return end
                self:_trigger(payload.damage)
            end)
        else
            print("[HitStop] ERROR: event_bus not available!")
        end
    end,

    OnDisable = function(self)
        if self._active then
            Time.SetTimeScale(1.0)
            self._active = false
        end
        if event_bus and event_bus.unsubscribe and self._subDmg then
            event_bus.unsubscribe(self._subDmg)
            self._subDmg = nil
        end
    end,

    Update = function(self, dt)
        if not self._active then return end

        -- Unscaled dt so the timer ticks even while time is frozen
        local udt = Time.GetUnscaledDeltaTime and Time.GetUnscaledDeltaTime() or dt
        self._timer = self._timer - udt

        if self._timer <= 0 then
            Time.SetTimeScale(1.0)
            self._active = false
            print("[HitStop] Freeze ended, time restored")
        end
    end,

    _trigger = function(self, damage)
        local scalar = self.durationScalar or 0.008
        local mn     = self.minDuration    or 0.05
        local mx     = self.maxDuration    or 0.2
        local dur    = math.max(mn, math.min(mx, damage * scalar))

        -- Don't restart if already in a longer hitstop
        if self._active and self._timer >= dur then return end

        self._timer  = dur
        self._active = true
        Time.SetTimeScale(self.timeScale or 0.0)

        print(string.format("[HitStop] FREEZE START — %.0f dmg → %.3fs, timeScale=%.2f", damage, dur, self.timeScale or 0.0))
    end,
}
