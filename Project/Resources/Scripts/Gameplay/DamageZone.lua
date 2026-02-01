<<<<<<< HEAD:Project/Resources/Scripts/Gameplay/DamageZone.lua
-- Resources/Scripts/Gameplay/DamageZone.lua
=======
-- Resources/Scripts/GamePlay/DamageZone.lua
>>>>>>> main:Project/Resources/Scripts/GamePlay/DamageZone.lua
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
        self._enemyPos = {}    -- [entityId] = {x,y,z}
        self._posSub = nil
    end,

    Start = function(self)
        -- subscribe to enemy position broadcasts
        if event_bus and event_bus.subscribe then
            self._posSub = event_bus.subscribe("enemy_position", function(p)
                if not p or not p.entityId then return end
                self._enemyPos[p.entityId] = { x = p.x or 0, y = p.y or 0, z = p.z or 0 }
            end)
        end

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

    _IsInsideBox = function(self, ex, ey, ez)
        local zx, zy, zz = self:GetPosition()
        local hx, hy, hz = self.HalfExtentsX, self.HalfExtentsY, self.HalfExtentsZ

        -- AABB check (world-aligned)
        if math.abs(ex - zx) > hx then return false end
        if math.abs(ey - zy) > hy then return false end
        if math.abs(ez - zz) > hz then return false end
        return true
    end,

    _TryDamageEntity = function(self, entityId)
        local t = nowSec()
        local cd = self.CooldownPerEnemy or 0.5
        local last = self._lastHit[entityId] or -1e9
        if (t - last) < cd then return end
        self._lastHit[entityId] = t

        if self.Debug then
            print(string.format("[DamageZone] DAMAGE -> entity=%s dmg=%s", tostring(entityId), tostring(self.Damage)))
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

    Update = function(self, dt)
        dt = toDtSec(dt)
        if dt == 0 then return end
        if not self._enemyPos then return end

        -- Check all known enemies (supports multiple)
        for eid, p in pairs(self._enemyPos) do
            if self:_IsInsideBox(p.x, p.y, p.z) then
                self:_TryDamageEntity(eid)
            end
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe and self._posSub then
            event_bus.unsubscribe(self._posSub)
            self._posSub = nil
        end
    end,
}
