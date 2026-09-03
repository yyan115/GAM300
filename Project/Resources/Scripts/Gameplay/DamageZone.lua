-- Resources/Scripts/Gameplay/DamageZone.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus
local Time = _G.Time

local function nowSec()
    if Time and Time.time then return Time.time end
    if Time and Time.GetTime then
        local ok, t = pcall(Time.GetTime, Time)
        if ok and type(t) == "number" then return t end
    end
    return os.clock()
end

local function toDtSec(dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return 0 end
    if dtSec > 0.05 then dtSec = 0.05 end
    return dtSec
end

return Component {
    mixins = { TransformMixin },

    fields = {
        Damage = 1,
        CooldownPerEnemy = 8.75, -- seconds
        EventName = "enemy_damage",
        Debug = true,

        -- Fallback if we can't read collider extents from component:
        HalfExtentsX = 0.54,
        HalfExtentsY = 2.44,
        HalfExtentsZ = 14.13,
    },

    Awake = function(self)
        self._lastHit = {}     -- [entityId] = timeSec
    end,

    Start = function(self)
        -- Try to read extents from ColliderComponent if your engine exposes it
        self._col = self:GetComponent("ColliderComponent")
        if self._col then
            -- Best-effort: some engines expose halfExtents as a table or userdata
            local ok, he = pcall(function() return self._col.halfExtents end)
            if ok and type(he) == "table" then
                self.HalfExtentsX = he.x or self.HalfExtentsX
                self.HalfExtentsY = he.y or self.HalfExtentsY
                self.HalfExtentsZ = he.z or self.HalfExtentsZ
            end
        end
    end,

    _TryDamageEntity = function(self, entityId)
        local t = nowSec()
        local cd = self.CooldownPerEnemy or 0.5
        local last = self._lastHit[entityId] or -1e9
        if (t - last) < cd then return end
        self._lastHit[entityId] = t

        if self.Debug then
            --print(string.format("[DamageZone] DAMAGE -> entity=%s dmg=%s", tostring(entityId), tostring(self.Damage)))
        end

        if event_bus and event_bus.publish then
            event_bus.publish(self.EventName, {
                entityId = entityId,
                dmg = self.Damage,
                src = "DamageZone",
                hitType = "MELEE_TEST",
                zone = self.entityId,
            })
        end
    end,

    _DamageMatching = function(self, scriptName, x, y, z)
        local entities = Engine.FindEntitiesWithScriptInAABB(
            scriptName, x, y, z,
            self.HalfExtentsX, self.HalfExtentsY, self.HalfExtentsZ)
        for i = 1, #entities do
            self:_TryDamageEntity(entities[i])
        end
    end,

    Update = function(self, dt)
        dt = toDtSec(dt)
        if dt == 0 then return end

        local x, y, z = self:GetPosition()
        self:_DamageMatching("EnemyAI.lua", x, y, z)
        self:_DamageMatching("MinibossAI.lua", x, y, z)
    end,
}
