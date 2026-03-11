require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },
    
    fields = {
        Lifetime = 0.35,
        BaseScale = 2.0,
        SpawnDelay = 0.0,
        ForwardOffset = 0.5, -- Adjust this: +ve is forward, -ve is backward
        HeightOffset = 0.0,  
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
        -- 1. Extract base data
        local px, py, pz = data.pos.x, data.pos.y, data.pos.z
        local qw, qx, qy, qz = data.rot.w, data.rot.x, data.rot.y, data.rot.z

        -- 2. Calculate Forward Direction Vector
        local fx = 2 * (qx * qz + qw * qy)
        local fy = 2 * (qy * qz - qw * qx)
        local fz = 1 - 2 * (qx * qx + qy * qy)

        -- 3. Apply Offsets
        -- We add (ForwardVector * Offset) to the base position
        local finalX = px + (fx * self.ForwardOffset)
        local finalY = py + (fy * self.ForwardOffset) + self.HeightOffset
        local finalZ = pz + (fz * self.ForwardOffset)

        -- 4. Set Transform
        self:SetPosition(finalX, finalY, finalZ)
        self:SetRotation(qw, qx, qy, qz)
        
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