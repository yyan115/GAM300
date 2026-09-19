-- The mark under the miniboss's phase 3 dive. A glowing purple disc closes
-- in on the spot it will land, with motes rising from it toward the boss and
-- a glint at its centre, flickering once the drop is close. The boss drops
-- straight down, so the mark follows the boss rather than the warning's
-- position, until it lands. One instance sits in the level.
require("extension.engine_bootstrap")

local Component = require("extension.mono_helper")

-- The effect's children, by entity name
local DISC  = "DiveMarkDisc"
local MOTES = "DiveMarkMotes"
local CORE  = "DiveMarkCore"

local function lerp(a, b, t)
    return a + (b - a) * t
end

return Component {
    fields = {
        -- Seconds for the mark to build up to full strength
        RampSeconds = 1.0,
        -- Disc width at the start and at full strength
        DiscScaleStart = 2.4,
        DiscScaleEnd   = 1.2,
        -- Disc opacity at the start and at full strength
        DiscOpacityStart = 0.35,
        DiscOpacityEnd   = 0.8,
        -- Disc glow at full strength. The disc is dark, and this much turns it
        -- a deep violet; around 1 it glares a flat neon over the floor.
        DiscBloomEnd = 0.2,
        -- Mote emission at the start and at full strength
        MoteRateStart = 20.0,
        MoteRateEnd   = 55.0,
        -- Past this share of the ramp the disc flickers, this many times a second
        FlickerFrom = 0.8,
        FlickerHz   = 12.0,
        -- Height above the floor, so the disc does not fight the floor for depth
        FloorOffset = 0.02,
        -- Seconds without a warning after which the mark goes out by itself,
        -- in case the dive never lands
        SafetySeconds = 3.0,
    },

    Start = function(self)
        self._transform = self:GetComponent("Transform")

        local discId = Engine.FindChildByName(self.entityId, DISC)
        if discId and discId >= 0 then
            self._discId = discId
            self._discTransform = GetComponent(discId, "Transform")
            self._discModel = GetComponent(discId, "ModelRenderComponent")
            self._discBloom = GetComponent(discId, "BloomComponent")
        end
        self._motes = self:_Particle(MOTES)
        self._core = self:_Particle(CORE)

        self:_Stop()

        if _G.event_bus then
            self._warningSub = _G.event_bus.subscribe("miniboss_slam_warning", function(payload)
                if payload and payload.targetId then
                    self:_OnWarning(payload)
                end
            end)
            self._slammedSub = _G.event_bus.subscribe("miniboss_slammed", function()
                self:_Stop()
            end)
        end
    end,

    -- A child's particle component. A missing child gives a stub whose writes
    -- go nowhere, so one bad name in the prefab loses only that layer.
    _Particle = function(self, name)
        local id = Engine.FindChildByName(self.entityId, name)
        if not id or id < 0 then return {} end
        return GetComponent(id, "ParticleComponent") or {}
    end,

    _OnWarning = function(self, payload)
        self._sinceWarning = 0
        -- Later warnings only move the landing spot, which the mark already
        -- follows through the boss
        if self._active then return end

        self._active = true
        self._elapsed = 0
        self._targetTransform = GetComponent(payload.targetId, "Transform")
        self._floorY = (tonumber(payload.posY) or 0) + self.FloorOffset
        self:_PlaceAt(payload.posX, payload.posZ)

        for _, p in ipairs({ self._motes, self._core }) do
            p.timeSinceEmission = 0
            p.isEmitting = true
        end
        if self._discModel then
            ModelRenderComponent.SetVisible(self._discModel, true)
        end
        self:_Ramp(0, 0)
    end,

    _PlaceAt = function(self, x, z)
        if not x or not z then return end
        self._transform.localPosition.x = x
        self._transform.localPosition.y = self._floorY
        self._transform.localPosition.z = z
        self._transform.isDirty = true
    end,

    _Ramp = function(self, t, elapsed)
        self._motes.emissionRate = lerp(self.MoteRateStart, self.MoteRateEnd, t)

        if self._discTransform then
            local s = lerp(self.DiscScaleStart, self.DiscScaleEnd, t)
            self._discTransform.localScale.x = s
            self._discTransform.localScale.z = s
            self._discTransform.isDirty = true
        end
        if self._discBloom then
            self._discBloom.bloomIntensity = lerp(0, self.DiscBloomEnd, t)
        end
        if self._discId then
            local opacity = lerp(self.DiscOpacityStart, self.DiscOpacityEnd, t)
            if t >= self.FlickerFrom then
                -- Square wave between full and 60 percent of it
                local on = math.floor(elapsed * self.FlickerHz * 2) % 2 == 0
                opacity = on and self.DiscOpacityEnd or self.DiscOpacityEnd * 0.6
            end
            Engine.SetModelOpacity(self._discId, opacity)
        end
    end,

    _Stop = function(self)
        self._active = false
        self._targetTransform = nil
        for _, p in ipairs({ self._motes, self._core }) do
            p.isEmitting = false
            p.timeSinceEmission = 0
        end
        if self._discModel then
            ModelRenderComponent.SetVisible(self._discModel, false)
        end
    end,

    Update = function(self, dt)
        if not self._active then return end
        dt = dt or 0

        self._sinceWarning = self._sinceWarning + dt
        if self._sinceWarning >= self.SafetySeconds then
            self:_Stop()
            return
        end

        if self._targetTransform then
            local p = self._targetTransform.worldPosition
            self:_PlaceAt(p.x, p.z)
        end

        self._elapsed = self._elapsed + dt
        local t = math.min(1, self._elapsed / math.max(0.05, self.RampSeconds))
        self:_Ramp(t, self._elapsed)
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            for _, key in ipairs({ "_warningSub", "_slammedSub" }) do
                if self[key] then
                    pcall(function() _G.event_bus.unsubscribe(self[key]) end)
                    self[key] = nil
                end
            end
        end
    end,
}
