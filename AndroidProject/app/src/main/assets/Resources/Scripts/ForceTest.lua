-- PlayerAttack.lua
require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus
local Input     = _G.Input
_G.player_is_attacking = _G.player_is_attacking or false

local function clamp(x, minv, maxv)
    if x < minv then return minv end
    if x > maxv then return maxv end
    return x
end

return Component {
    mixins = { TransformMixin },

    Awake = function(self)
    end,

    Start = function(self)
        self._rigidbody = self:GetComponent("RigidBodyComponent")
    end,

    OnDisable = function(self)
    end,

    ----------------------------------------------------------------------
    -- Main update
    ----------------------------------------------------------------------
    Update = function(self, dt)
        if not self._rigidbody then return
        end
        -- self._rigidbody:AddForce(0,500,0)
        self._rigidbody:AddImpulse(0,500,0)
    end,



}
