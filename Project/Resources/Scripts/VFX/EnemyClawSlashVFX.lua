require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },
    
    fields = {
        Lifetime      = 0.35,
        BaseScale     = 2.0,
        ForwardOffset = 0.5,
        HeightOffset  = 0.0,
        TriggerNormalizedTime = 0.45, -- tune this to match strike frame
    },

    Start = function(self)
        self.model = self:GetComponent("ModelRenderComponent")
        self.active         = false
        self.slashTriggered = false
        self.timer          = 0

        if self.model then self.model.isVisible = false end

        if _G.event_bus then
            self._slashSub = _G.event_bus.subscribe("onClawSlashTrigger", function(data)
                self:OnEventReceived(data)
            end)
        end
    end,

    OnEventReceived = function(self, data)
        if self.active or data.claimed then return end

        data.claimed        = true
        self.pendingData    = data
        self.active         = true
        self.slashTriggered = false
        self.timer          = 0

        self._enemyAnim = GetComponent(data.entityId, "AnimationComponent")
        self._enemyTransform = Engine.FindTransformByID(data.entityId)

        if self.model then self.model.isVisible = false end
    end,

    ActivateSlash = function(self, data)
    -- Query live position/rotation at the moment of impact
        local pos            = self._enemyTransform.localPosition
        local rot            = self._enemyTransform.localRotation

        local px, py, pz     = pos.x, pos.y, pos.z
        local qw, qx, qy, qz = rot.w, rot.x, rot.y, rot.z

        local fx = 2 * (qx * qz + qw * qy)
        local fy = 2 * (qy * qz - qw * qx)
        local fz = 1 - 2 * (qx * qx + qy * qy)

        self:SetPosition(
            px + (fx * self.ForwardOffset),
            py + (fy * self.ForwardOffset) + self.HeightOffset,
            pz + (fz * self.ForwardOffset)
        )
        self:SetRotation(qw, qx, qy, qz)

        self.timer = 0
        if self.model then self.model.isVisible = true end
    end,

    Update = function(self, dt)
        if not self.active then return end
        local dtSec = dt or 0

        -- Wait for the correct animation state
        local currentState = self._enemyAnim:GetCurrentState()
        if currentState ~= "Melee Attack" then return end

        -- Trigger VFX at the strike frame
        if not self.slashTriggered and self._enemyAnim:GetNormalizedTime() >= self.TriggerNormalizedTime then
            self.slashTriggered = true
            self:ActivateSlash(self.pendingData)
        end

        -- Lifetime countdown after slash appears
        if self.slashTriggered then
            self.timer = self.timer + dtSec
            if self.timer >= self.Lifetime then
                self.active = false
                if self.model then self.model.isVisible = false end
            end
        end
    end,

    OnDisable = function(self)
        if _G.event_bus and self._slashSub then
            _G.event_bus.unsubscribe(self._slashSub)
        end
    end
}