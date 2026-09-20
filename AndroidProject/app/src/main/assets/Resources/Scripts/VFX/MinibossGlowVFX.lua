-- The miniboss's own glow. A colour builds on its body through the wind up of
-- the charged slash and goes out after the dash: yellow for the slower slash of
-- phases 1 and 2, red for phase 3's fast one. Purple pulses on it while it
-- cannot be hooked, in phase 3 in the air. It sits on the boss and is the only
-- thing that drives the boss's bloom.
require("extension.engine_bootstrap")

local Component = require("extension.mono_helper")

local function lerp(a, b, t)
    return a + (b - a) * t
end

return Component {
    fields = {
        -- The slower charged slash's yellow, the fast one's red, and the glow
        -- at the end of either wind up
        SlowChargeR = 1.0,
        SlowChargeG = 0.72,
        SlowChargeB = 0.08,
        FastChargeR = 1.0,
        FastChargeG = 0.06,
        FastChargeB = 0.02,
        -- The shader multiplies this by how brightly lit the fragment already
        -- is, so a dark model in a dark arena needs a large number before the
        -- glow reads at all. At 0.6 nothing showed on screen.
        ChargeIntensityEnd = 12.0,
        -- Seconds the colour takes to go out after the dash
        ChargeFadeSeconds = 0.3,
        -- The purple while it cannot be hooked, and how it pulses
        UnhookableR = 0.55,
        UnhookableG = 0.08,
        UnhookableB = 1.0,
        UnhookableIntensity = 7.0,
        UnhookablePulseDepth = 0.4,
        UnhookablePulseHz = 1.2,
    },

    Start = function(self)
        self._bloom = self:GetComponent("BloomComponent")
        self._clock = 0
        self._unhookable = false
        self:_Off()

        if _G.event_bus then
            self._chargeSub = _G.event_bus.subscribe("miniboss_charge_warning", function(payload)
                if payload and payload.targetId == self.entityId then
                    self._chargeDur = math.max(0.1, tonumber(payload.seconds) or 1.0)
                    self._chargeT = 0
                    self._fadeT = nil
                    if payload.fast then
                        self._chargeR, self._chargeG, self._chargeB =
                            self.FastChargeR, self.FastChargeG, self.FastChargeB
                    else
                        self._chargeR, self._chargeG, self._chargeB =
                            self.SlowChargeR, self.SlowChargeG, self.SlowChargeB
                    end
                end
            end)
            self._unhookableSub = _G.event_bus.subscribe("miniboss_unhookable", function(payload)
                if payload and payload.entityId == self.entityId then
                    self._unhookable = payload.active == true
                end
            end)
        end
    end,

    Update = function(self, dt)
        dt = dt or 0
        self._clock = self._clock + dt

        -- The wind up's colour takes priority over everything else
        if self._chargeDur then
            self._chargeT = self._chargeT + dt
            local t = math.min(1, self._chargeT / self._chargeDur)
            self:_Glow(self.ChargeIntensityEnd * t * t, self._chargeR, self._chargeG, self._chargeB)
            if t >= 1 then
                self._chargeDur = nil
                self._fadeT = 0
            end
            return
        end
        if self._fadeT then
            self._fadeT = self._fadeT + dt
            local t = math.min(1, self._fadeT / math.max(0.01, self.ChargeFadeSeconds))
            self:_Glow(lerp(self.ChargeIntensityEnd, 0, t), self._chargeR, self._chargeG, self._chargeB)
            if t >= 1 then self._fadeT = nil end
            return
        end

        if self._unhookable then
            local wave = 0.5 * (1 + math.sin(2 * math.pi * self.UnhookablePulseHz * self._clock))
            local pulse = 1 - self.UnhookablePulseDepth * wave
            self:_Glow(self.UnhookableIntensity * pulse, self.UnhookableR, self.UnhookableG, self.UnhookableB)
            return
        end

        self:_Off()
    end,

    _Glow = function(self, intensity, r, g, b)
        if not self._bloom then return end
        if intensity <= 0 then
            self:_Off()
            return
        end
        self._bloom.enabled = true
        self._bloom.bloomIntensity = intensity
        self._bloom:SetColor(r, g, b)
    end,

    _Off = function(self)
        if self._bloom then self._bloom.enabled = false end
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            for _, key in ipairs({ "_chargeSub", "_unhookableSub" }) do
                if self[key] then
                    pcall(function() _G.event_bus.unsubscribe(self[key]) end)
                    self[key] = nil
                end
            end
        end
    end,
}
