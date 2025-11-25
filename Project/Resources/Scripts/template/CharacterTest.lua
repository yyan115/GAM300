local Component = require("mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {

    mixins = {TransformMixin},

    fields = {
        name = "CharacterControllerTest",
    },

    Awake = function(self)
        print("[LUA] CharacterControllerTest Awake - Creating Controller")
        
        -- Step 1: Create controller
        self.controller = CharacterController.new()
        
        if self.controller == nil then
            print("[LUA ERROR] Failed to create CharacterController!")
            return
        end
        print("[LUA] CharacterController created successfully!")
    end,

    Start = function(self)
        -- Get the components for initialisation
        local collider = self:GetComponent("ColliderComponent")
        local transform = self:GetComponent("Transform")

        -- if not collider then
        --     print("Collider is not getting")
        -- else
        --     print("Collider is getting")
        -- end

        -- if not transform then
        --     print("transform is not getting")
        -- else
        --     print("transform is getting")
        -- end
        -- Initialise CharacterController
        CharacterController.Initialise(self.controller, collider, transform)
    end,  

    Update = function(self, dt)
        if not self.controller then return end
        
        CharacterController.Move(self.controller, 0, 0, 1)
        CharacterController.Update(self.controller, dt)

        local position = CharacterController.GetPosition(self.controller)
        if not position then
            print("position is screwed")
        else
            self:SetPosition(position.x, position.y, position.z)
        end
    end,

    -- Try ALL possible cleanup methods
    OnDisable = function(self)
        print("[LUA] OnDisable called - cleaning up controller")
        if self.controller then
            CharacterController.Destroy(self.controller)
            self.controller = nil
        end
    end
}