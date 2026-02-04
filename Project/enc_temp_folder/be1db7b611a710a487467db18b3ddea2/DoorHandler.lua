require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local Engine = _G.Engine
local Physics = _G.Physics

local function distSqXZ(ax, az, bx, bz)
    local dx = ax - bx
    local dz = az - bz
    return dx*dx + dz*dz
end

return Component {
    mixins = { TransformMixin },

    fields = {
        Radius = 1.0,
        PlayerName = "Player",
    },

    Start = function(self)
                    print("Player fdsafdasfs collided with pedestal!")
        print("[PedestalTrigger] Start entityId=", tostring(self.entityId))

    self._inside = false

    self._myTr = self:GetComponent("Transform")
    print("[PedestalTrigger] my Transform is", self._myTr and "OK" or "NIL")

    self._playerTr = Engine.FindTransformByName(self.PlayerName)
    print("[PedestalTrigger] player Transform is", self._playerTr and "OK" or "NIL", " name=", self.PlayerName)
    end,

    Update = function(self, dt)
        if not self._playerTr or not self._myTr then return end
                    print("Player NOT collided with pedestal!")

        -- player position (MinibossAI style)
        local pp = Engine.GetTransformPosition(self._playerTr)
        if not pp then return end

        -- pedestal position: use THIS entity's transform
        -- IMPORTANT: use world position via Engine helper if available; fallback to localPosition.
        local tp = nil
        if Engine.GetTransformPosition then
            tp = Engine.GetTransformPosition(self._myTr)
        end
        if not tp then
            local p = self._myTr.localPosition
            tp = { p.x, p.y, p.z }
        end

        -- ✅ recommended: XZ-only so pedestal height doesn't break it
        local inside = distSqXZ(pp[1], pp[3], tp[1], tp[3]) <= (self.Radius * self.Radius)

        -- (Optional) if you insist on CheckDistance 3D, use this instead:
        -- local inside = Physics and Physics.CheckDistance and Physics.CheckDistance(pp[1], pp[2], pp[3], tp[1], tp[2], tp[3], self.Radius) or false

        if inside and not self._inside then
            print("Player collided with pedestal!")
        end

        self._inside = inside
    end,
}
