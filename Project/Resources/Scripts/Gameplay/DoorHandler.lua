require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus
local Engine = _G.Engine

local function distSqXZ(ax, az, bx, bz)
    local dx = ax - bx
    local dz = az - bz
    return dx*dx + dz*dz
end

return Component {

    fields = {
        Radius = 1.2
    },

    Start = function(self)
        print("[PedestalTrigger] Start. entityId =", self.entityId)

        self._inside = false
        self._playerPos = nil

        if event_bus and event_bus.subscribe then
            self._sub = event_bus.subscribe("player_position", function(pos)
                self._playerPos = pos
            end)
        end
    end,

    Update = function(self, dt)
        if not self._playerPos then return end
        if not Engine or not Engine.GetEntityPosition then return end

        -- pedestal world position (THIS IS THE IMPORTANT LINE)
        local pedPos = Engine.GetEntityPosition(self.entityId)
        if not pedPos then
            return
        end

        local px = self._playerPos.x
        local pz = self._playerPos.z

        local tx = pedPos.x
        local tz = pedPos.z

        local r = self.Radius
        local inside = distSqXZ(px, pz, tx, tz) <= (r * r)

        if inside and not self._inside then
            print("Player collided with pedestal")
        end

        self._inside = inside
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe and self._sub then
            event_bus.unsubscribe(self._sub)
        end
    end,
}
