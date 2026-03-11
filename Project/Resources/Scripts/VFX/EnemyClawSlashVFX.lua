require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },
    
    fields = {
        Lifetime = 0.35,
        BaseScale = 2.0,
        SpawnDelay = 0.0 -- Simple global delay if needed
    },
    
    Start = function(self)
        self.model = self:GetComponent("ModelRenderComponent")
        self.active = false
        self.isWaiting = false
        self.timer = 0
        self.currentDelay = 0
        
        if self.model then self.model.isVisible = false end

        -- SUBSCRIBE TO THE EVENT BUS
        if _G.event_bus then
            self._slashSub = _G.event_bus.subscribe("onClawSlashTrigger", function(data)
                self:OnEventReceived(data)
            end)
        end
    end,

    OnEventReceived = function(self, data)
        if self.active or data.claimed then return end

        data.claimed = true
        self.pendingData = data
        self.active = true
        self.timer = 0
        
        self.currentDelay = self.SpawnDelay
        if self.currentDelay > 0 then
            self.isWaiting = true
            if self.model then self.model.isVisible = false end
        else
            self.isWaiting = false
            self:ActivateSlash(data)
        end
    end,

    ActivateSlash = function(self, data)
        -- Set Transform based on the event data
        self:SetPosition(data.pos.x, data.pos.y, data.pos.z)
        self:SetRotation(data.rot.w, data.rot.x, data.rot.y, data.rot.z)

        -- Basic Scale Logic
        local s = self.BaseScale

        self:SetScale(s, s, s)

        self.timer = 0
        if self.model then self.model.isVisible = true end
    end,

    Update = function(self, dt)
        if not self.active then return end
        local dtSec = dt or 0

        -- Delay Phase
        if self.isWaiting then
            self.currentDelay = self.currentDelay - dtSec
            if self.currentDelay <= 0 then
                self.isWaiting = false
                self:ActivateSlash(self.pendingData)
            end
            return 
        end
        
        -- Lifetime Phase
        self.timer = self.timer + dtSec
        if self.timer >= self.Lifetime then
            self.active = false
            if self.model then self.model.isVisible = false end
        end
    end,

    OnDisable = function(self)
        if _G.event_bus and self._slashSub then
            _G.event_bus.unsubscribe(self._slashSub)
        end
    end
}