require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus
local Physics = _G.Physics

return Component {
    mixins = { TransformMixin },

    fields = {
        Radius = 1.0
    },

    Awake = function(self)
        self._playerPos = nil
        self._inside = false

        -- already provided by your PlayerMovement script
        if event_bus and event_bus.subscribe then
            self._sub = event_bus.subscribe("player_position", function(pos)
                self._playerPos = pos
            end)
        end
    end,

    Update = function(self, dt)
        if not self._playerPos then return end
        if not Physics or not Physics.CheckDistance then return end

        local p = self:GetComponent("Transform").localPosition

        -- CheckDistance(x1,y1,z1, x2,y2,z2, radius)
        local inside = Physics.CheckDistance(
            self._playerPos.x, self._playerPos.y, self._playerPos.z,
            p.x, p.y, p.z,
            self.Radius
        )

        if inside and not self._inside then
            print("Player collided with trigger!")
        end

        self._inside = inside
    end,
}
