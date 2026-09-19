-- The miniboss's charged slash wind up. Purple light gathers into its chest,
-- red feathers shake loose and drift up turning purple, and glints snap off
-- it, all building until the dash, when the glints burst outward. One
-- instance sits in the level and moves to the boss on each warning.
require("extension.engine_bootstrap")

local Component = require("extension.mono_helper")

-- The effect's child emitters, by entity name
local GATHER   = "ChargeGather"
local FEATHERS = "ChargeFeathers"
local GLINTS   = "ChargeGlints"

local function lerp(a, b, t)
    return a + (b - a) * t
end

return Component {
    fields = {
        -- Emission rates at the start and the end of the wind up
        FeatherRateStart = 10.0,
        FeatherRateEnd   = 30.0,
        GlintRateStart   = 12.0,
        GlintRateEnd     = 40.0,
        -- Glow of the gathering light at the start and the end
        GatherBloomStart = 0.5,
        GatherBloomEnd   = 2.5,
        -- How far the glints fly when they burst at the dash
        GlintBurstSpread = 5.0,
    },

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        self._gather   = self:_Emitter(GATHER, true)
        self._feathers = self:_Emitter(FEATHERS)
        self._glints   = self:_Emitter(GLINTS)
        for _, e in ipairs({ self._gather, self._feathers, self._glints }) do
            e.particle.isEmitting = false
        end
        if self._glints.particle then
            self._glintSpread = self._glints.particle.velocityRandomness
        end

        if _G.event_bus then
            self._chargeSub = _G.event_bus.subscribe("miniboss_charge_warning", function(payload)
                if payload and payload.posX then
                    self:Begin(payload.posX, payload.posY, payload.posZ, payload.seconds)
                end
            end)
        end
    end,

    -- A child's particle component, and its bloom when it has one. A missing
    -- child gives a stub whose writes go nowhere, so one bad name in the
    -- prefab loses only that layer of the effect.
    _Emitter = function(self, name, withBloom)
        local id = Engine.FindChildByName(self.entityId, name)
        if not id or id < 0 then
            return { particle = {}, bloom = nil }
        end
        return {
            particle = GetComponent(id, "ParticleComponent") or {},
            bloom = withBloom and GetComponent(id, "BloomComponent") or nil,
        }
    end,

    Begin = function(self, x, y, z, seconds)
        self._transform.localPosition.x = x
        self._transform.localPosition.y = y
        self._transform.localPosition.z = z
        self._transform.isDirty = true

        self._duration = math.max(0.1, tonumber(seconds) or 1.0)
        self._elapsed = 0
        self._burstFrames = nil
        if self._glintSpread then
            self._glints.particle.velocityRandomness = self._glintSpread
        end

        -- Anything left from the last wind up would come out as one burst
        for _, e in ipairs({ self._gather, self._feathers, self._glints }) do
            e.particle.timeSinceEmission = 0
            e.particle.isEmitting = true
        end
        self:_Ramp(0)
    end,

    _Ramp = function(self, t)
        self._feathers.particle.emissionRate = lerp(self.FeatherRateStart, self.FeatherRateEnd, t)
        self._glints.particle.emissionRate = lerp(self.GlintRateStart, self.GlintRateEnd, t)
        if self._gather.bloom then
            self._gather.bloom.bloomIntensity = lerp(self.GatherBloomStart, self.GatherBloomEnd, t)
        end
    end,

    Update = function(self, dt)
        -- The burst needs one frame of emission, then everything stops
        if self._burstFrames then
            self._burstFrames = self._burstFrames - 1
            if self._burstFrames <= 0 then
                for _, e in ipairs({ self._gather, self._feathers, self._glints }) do
                    e.particle.isEmitting = false
                    e.particle.timeSinceEmission = 0
                end
                self._burstFrames = nil
            end
            return
        end

        if not self._duration then return end
        self._elapsed = self._elapsed + (dt or 0)
        local t = math.min(1, self._elapsed / self._duration)
        self:_Ramp(t)

        if t >= 1 then
            -- The dash: the gathered light goes out and the glints scatter.
            -- A full second of backlog spawns every glint the emitter can
            -- hold in the one frame it still emits.
            self._duration = nil
            self._gather.particle.isEmitting = false
            self._feathers.particle.isEmitting = false
            self._glints.particle.velocityRandomness = self.GlintBurstSpread
            self._glints.particle.timeSinceEmission = 1.0
            self._burstFrames = 1
        end
    end,

    OnDisable = function(self)
        if self._chargeSub and _G.event_bus and _G.event_bus.unsubscribe then
            pcall(function() _G.event_bus.unsubscribe(self._chargeSub) end)
            self._chargeSub = nil
        end
    end,
}
