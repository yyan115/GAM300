-- Resources/Scripts/Gameplay/PhysicsHitReact.lua
-- Attach to any object with a dynamic RigidBodyComponent.
-- When AttackHitbox hits this entity, an impulse is applied away from
-- the player in a direction derived from the attack type.
--
-- Requires:
--   RigidBodyComponent  (motionID = 2, i.e. dynamic)
--
-- Attack type → impulse behaviour:
--   COMBO / FEATHER  : horizontal push away from player
--   LIFT             : mostly upward with a small outward kick
--   AIR              : horizontal push (player is above, attack comes down at angle)
--   SLAM             : strong downward + outward burst
--   KNOCKUP          : straight up
--   default          : horizontal push away from player

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

return Component {
    fields = {
        -- Base impulse magnitude. Scaled by the hit's knockback value if non-zero.
        -- Set to 0 to rely purely on per-attack knockback from ComboManager.
        BaseImpulse       = 6.0,

        -- Extra upward component added for LIFT hits (0–1 blended with outward dir).
        LiftUpwardBias    = 0.8,

        -- Downward force factor for SLAM hits (applied as negative Y).
        SlamDownwardBias  = 1.2,

        -- Seconds of invincibility after a hit so rapid combo hits don't
        -- stack impulses and send the object flying too far.
        ImpulseIFrame     = 0.15,

        -- Name of the Player entity used to compute push direction.
        PlayerName        = "Player",
    },

    Awake = function(self)
        self._rb           = nil
        self._hitLockTimer = 0
        self._sub          = nil
        self._playerTr     = nil
    end,

    Start = function(self)
        self._rb = self:GetComponent("RigidBodyComponent")
        if not self._rb then
            print("[PhysicsHitReact] WARNING: No RigidBodyComponent found on entity " .. tostring(self.entityId))
        end

        self._playerTr = Engine.FindTransformByName(self.PlayerName)

        if event_bus and event_bus.subscribe then
            self._sub = event_bus.subscribe("deal_damage_to_entity", function(payload)
                if not payload then return end
                if payload.entityId ~= self.entityId then return end
                self:_onHit(payload)
            end)
        end
    end,

    Update = function(self, dt)
        if self._hitLockTimer > 0 then
            local dtSec = dt or 0
            if dtSec > 1.0 then dtSec = dtSec * 0.001 end  -- ms → s guard
            self._hitLockTimer = math.max(0, self._hitLockTimer - dtSec)
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe and self._sub then
            event_bus.unsubscribe(self._sub)
            self._sub = nil
        end
    end,

    OnDestroy = function(self)
        if event_bus and event_bus.unsubscribe and self._sub then
            event_bus.unsubscribe(self._sub)
            self._sub = nil
        end
    end,

    -- ── Internal ──────────────────────────────────────────────────────────

    _onHit = function(self, payload)
        if self._hitLockTimer > 0 then return end
        if not self._rb then return end

        local hitType = string.upper(payload.hitType or "COMBO")
        local knockback = tonumber(payload.knockback) or 0

        -- Resolve push magnitude: prefer per-attack knockback, fall back to BaseImpulse.
        local magnitude = (knockback > 0) and knockback or (tonumber(self.BaseImpulse) or 6.0)

        -- Direction away from the player in XZ.
        local outX, outZ = self:_getOutwardDir()

        local ix, iy, iz = self:_buildImpulse(hitType, outX, outZ, magnitude)

        print(string.format("[PhysicsHitReact] entity=%s hitType=%s impulse=(%.2f, %.2f, %.2f)",
            tostring(self.entityId), hitType, ix, iy, iz))

        pcall(function()
            self._rb.impulseApplied = { x = ix, y = iy, z = iz }
        end)

        self._hitLockTimer = tonumber(self.ImpulseIFrame) or 0.15
    end,

    -- Returns the normalised XZ direction pointing away from the player.
    -- Falls back to world +X if the player transform is unavailable.
    _getOutwardDir = function(self)
        if not self._playerTr then
            self._playerTr = Engine.FindTransformByName(self.PlayerName)
        end

        local ex, _, ez = self:GetPosition()
        if ex == nil then return 1, 0 end

        if self._playerTr then
            local pp = Engine.GetTransformPosition(self._playerTr)
            if pp then
                local dx = ex - pp[1]
                local dz = ez - pp[3]
                local len = math.sqrt(dx * dx + dz * dz)
                if len > 1e-4 then
                    return dx / len, dz / len
                end
            end
        end

        return 1, 0  -- fallback: push along world +X
    end,

    -- Builds the (x, y, z) impulse vector from hitType, outward dir, and magnitude.
    _buildImpulse = function(self, hitType, outX, outZ, magnitude)
        local ix, iy, iz

        if hitType == "LIFT" then
            -- Upward launch with a small outward kick.
            local upBias = tonumber(self.LiftUpwardBias) or 0.8
            local hScale = 1.0 - upBias
            ix = outX * magnitude * hScale
            iy = magnitude * upBias
            iz = outZ * magnitude * hScale

        elseif hitType == "SLAM" then
            -- Downward smash: strong downward + outward burst.
            local downBias = tonumber(self.SlamDownwardBias) or 1.2
            ix = outX * magnitude
            iy = -magnitude * downBias
            iz = outZ * magnitude

        elseif hitType == "KNOCKUP" then
            -- Pure vertical.
            ix = 0
            iy = magnitude
            iz = 0

        elseif hitType == "AIR" then
            -- Player attacked from above; push outward and slightly downward.
            ix = outX * magnitude
            iy = -magnitude * 0.3
            iz = outZ * magnitude

        else
            -- COMBO, FEATHER, or any other type: horizontal push away from player.
            ix = outX * magnitude
            iy = 0
            iz = outZ * magnitude
        end

        return ix, iy, iz
    end,
}
