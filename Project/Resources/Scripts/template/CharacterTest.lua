require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local hasMoved = false

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
        print("Start in Lua is being called")
        -- Get the components for initialisation
        local collider = self:GetComponent("ColliderComponent")
        local Transform = self:GetComponent("Transform")
        
        -- Initialise CharacterController
        CharacterController.Initialise(self.controller, collider, Transform)

    end,  

    Update = function(self, dt)
        if not self.controller then return end
        --update the internal jolt
        CharacterController.Update(self.controller, dt)

        if not hasMoved then
            CharacterController.Move(self.controller, 0, -1, 0)
            hasMoved = true
        end
        --get Position from Jolt and pass it to ECS
        local position = CharacterController.GetPosition(self.controller)
        self:SetPosition(position.x, position.y, position.z)


        if CharacterController.IsGrounded(self.controller) then
            CharacterController.Jump(self.controller, 2.0)
        end





        --VERIFIED WORKING
        local gravity = CharacterController.GetGravity(self.controller)
        print("Gravity is ", gravity.y)
        local vel = CharacterController.GetVelocity(self.controller)
        print("Velocity for x is ",vel.x)
        print("Velocity for y is ",vel.y)
        print("Velocity for z is ",vel.z)


    end,

    OnDisable = function(self)
        print("[LUA] OnDisable called - cleaning up controller")
        if self.controller then
            CharacterController.Destroy(self.controller)
        end
            self.controller = nil
    end
}