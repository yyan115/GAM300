require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },
    
    fields = {
        Lifetime      = 0.25,
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
        self.slashTriggered = true
        if self.model then self.model.isVisible = true end
    end,

    Update = function(self, dt)
        if not self.active or not self._enemyAnim then return end
        local dtSec = dt or 0
        local anim = self._enemyAnim
        local currentState = anim:GetCurrentState()
        local normalizedTime = anim:GetNormalizedTime()
        -- STAGE 1: WAITING FOR THE ATTACK TO HIT THE TRIGGER FRAME
        if not self.slashTriggered then
            if currentState == "Melee Attack" then
                if normalizedTime >= self.TriggerNormalizedTime then
                    self:ActivateSlash()
                end
            end
        -- STAGE 2: VFX IS VISIBLE, HANDLING LIFETIME AND INTERRUPTS
        else
            -- interrupted (Hurt/Death) or the animation finished early.
            if currentState ~= "Melee Attack" then
                self:Deactivate()
                return 
            end
            -- Standard lifetime countdown
            self.timer = self.timer + dtSec
            if self.timer >= self.Lifetime then
                self:Deactivate()
            end
        end
    end,

    -- Helper function to clean up and hide
    Deactivate = function(self)
        self.active = false
        self.slashTriggered = false
        if self.model then 
            self.model.isVisible = false 
        end
    end,

    OnDisable = function(self)
        if _G.event_bus and self._slashSub then
            _G.event_bus.unsubscribe(self._slashSub)
        end
        -- Reset state
        self.active         = false
        self.slashTriggered = false
        self.timer          = 0
        self.pendingData    = nil

        if self.model then self.model.isVisible = false end
    end
}
