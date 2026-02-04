local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },

    fields = {
        Speed    = 15.0,  
        Lifetime = 0.5,   -- Total time the slash is visible
        StartRot = -60,   
        EndRot   = 60,    
    },

    Start = function(self)
        self.model = self:GetComponent("ModelRenderComponent")
        self.active = false
        self.age = 0
        
        if self.model then
            ModelRenderComponent.SetVisible(self.model, false)
        end
    end,

    Update = function(self, dt)

        --REPLACE THIS WITH IF SLASHING LOGIC 
        --Only trigger if not already slashing
        if not self.active and Input.IsPointerJustPressed() then
            self.active = true
            self.age = 0
            if self.model then
                ModelRenderComponent.SetVisible(self.model, true)
            end
        end

        --If currently slashing
        if self.active then
            self.age = self.age + dt
            local progress = math.min(self.age / self.Lifetime, 1.0)

            -- Calculate Y-axis sweep
            local currentYaw = self.StartRot + (self.EndRot - self.StartRot) * progress
            local halfAngle = math.rad(currentYaw) * 0.5
            
            -- Apply rotation 
            self:SetRotation(math.cos(halfAngle), 0, math.sin(halfAngle), 0)

            -- when finished
            if self.age >= self.Lifetime then
                self.active = false
                if self.model then
                    ModelRenderComponent.SetVisible(self.model, false)
                end
            end
        end
    end,
}