-- Resources/Scripts/Gameplay/PickupHover.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

local function FloatMotionLoop(self, dt)
    self._time = self._time + dt
    local offsetY = math.sin(self._time * self.floatSpeed) * self.floatHeight
    self:SetPosition(self._startX, self._startY + offsetY, self._startZ)
end

local function YawQuatFromDegrees(deg)
    local rad = math.rad(deg)
    local half = rad * 0.5
    return math.cos(half), 0, math.sin(half), 0
end

return Component {
    mixins = { TransformMixin },

    fields = {
        floatSpeed  = 0.3,
        floatHeight = 0.1,
        rotateSpeed = 90.0,
    },

    Awake = function(self)
        self._disableHover = false

        --print("[PickupHover] Subscribing to picked_up_weapon")
            self._pickedUpWeaponSub = event_bus.subscribe("picked_up_weapon", function(payload)
                if payload then
                    self._disableHover = true
                end
            end)
    end,

    Start = function(self)
        self._startX, self._startY, self._startZ = self:GetPosition()
        self._time = 0
    end,

    Update = function(self, dt)
        if self._disableHover then return end

        FloatMotionLoop(self, dt)

        self._currentRotY = (self._currentRotY or 0) + self.rotateSpeed * dt
        local w, x, y, z = YawQuatFromDegrees(self._currentRotY)
        self:SetRotation(w, x, y, z)
    end
}
