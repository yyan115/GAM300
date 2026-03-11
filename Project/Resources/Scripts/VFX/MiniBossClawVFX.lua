require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },
    
    fields = {
    },
    
    Start = function(self)
        self.model = self:GetComponent("ModelRenderComponent")
        self.active = false
        self.isWaiting = false
        self.timer = 0
        
        if self.model then self.model.isVisible = false end

        if _G.event_bus then
            self._slashSub = _G.event_bus.subscribe("miniboss_vfx", function(data)
                self:OnEventReceived(data)
            end)
        end
    end,

    OnEventReceived = function(self, data)
        if self.active then return end 

        self._MiniBossEntityId = data.entityId
        self._MiniBossAnim = GetComponent(self._MiniBossEntityId, "AnimationComponent")
        self._pendingData = data

        self.active = true
        self.IsWaiting = true
        self._hasTriggered = false
    end,

    Update = function(self, dt)
        if self._MiniBossAnim then
            local CurrentState = self._MiniBossAnim:GetCurrentState()
            if CurrentState ~= "Melee Attack" then return end
            print("CurrentState is ", self._MiniBossAnim:GetCurrentState())
        end
    end,


    OnDisable = function(self)
        if _G.event_bus and self._slashSub then
            _G.event_bus.unsubscribe(self._slashSub)
        end
    end
}