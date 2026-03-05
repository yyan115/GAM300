require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    fields = {
        AliveDuration = 3.0,
    },

    Awake = function(self)

    end,

    Start = function(self)
        self._aliveDuration = self.AliveDuration
    end,

    Update = function(self, dt)
        self._aliveDuration = self._aliveDuration - dt
        if self._aliveDuration <= 0 then
            Engine.DestroyEntity(self.entityId)
        end
    end,

    OnDisable = function(self)

    end,

    OnCollisionEnter = function(self, otherEntityId)
        local otherEntityLayer = Engine.GetEntityLayer(otherEntityId)
        if otherEntityLayer == "Ground" then
            print("[FeatherSkillRemnant] Collided with ground")
            self._rb = self:GetComponent("RigidBodyComponent")
            self._collider = self:GetComponent("ColliderComponent")

            self._rb.motionID = 1
            self._rb.motion_dirty = true
            self._rb.isTrigger = true
            self._rb.gravityFactor = 0.0
            self._collider.enabled = false
        end
    end,
}