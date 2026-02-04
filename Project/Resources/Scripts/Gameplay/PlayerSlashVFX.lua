local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },

    fields = {
        Speed    = 500,
        Lifetime = 0.2,   
        StartRot = -60,   
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
        -- TO BE REPLACED WITH THE SLASH LOGIC
        if not self.active and Input.IsPointerJustPressed() then
            self.active = true
            self.age = 0
            if self.model then
                ModelRenderComponent.SetVisible(self.model, true)
            end
        end

        if self.active then
            self.age = self.age + dt

            local progress = math.min(self.age / self.Lifetime, 1.0)

            -- Speed now represents total degrees to sweep
            local currentYaw = self.StartRot + (self.Speed * progress)
            
            -- Convert to Quaternion
            local halfAngle = math.rad(currentYaw) * 0.5
            local w = math.cos(halfAngle)
            local y = math.sin(halfAngle)
            
            self:SetRotation(w, 0, y, 0)

            --RESET
            if self.age >= self.Lifetime then
                self.active = false
                if self.model then
                    ModelRenderComponent.SetVisible(self.model, false)
                end
            end
        end
    end,
}