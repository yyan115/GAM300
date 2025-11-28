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
        self._controller = CharacterController.new()
        self._isInitialized = false  -- Track initialization state
        
        if self._controller == nil then
            print("[LUA ERROR] Failed to create CharacterController!")
            return
        end
        
        print("[LUA] CharacterController created successfully!")
    end,
    
    Start = function(self)
        if not self._controller then
            print("[LUA ERROR] Controller is nil in Start!")
            return
        end
        
        -- Get the components for initialisation
        local collider = self:GetComponent("ColliderComponent")
        local transform = self:GetComponent("Transform")
        
        -- Validate components exist
        if not collider then
            print("[LUA ERROR] ColliderComponent not found!")
            return
        end
        
        if not transform then
            print("[LUA ERROR] Transform not found!")
            return
        end
        
        print("[LUA] Initializing CharacterController with collider and transform")
        
        -- Initialise CharacterController
        local success = CharacterController.Initialise(self._controller, collider, transform)
        
        -- Check if initialization succeeded (if Initialise returns a value)
        if success == false then
            print("[LUA ERROR] CharacterController initialization failed!")
            return
        end
        
        self._isInitialized = true
        print("[LUA] CharacterController initialized successfully!")
    end,
    
    Update = function(self, dt)
        -- Safety check: Make sure controller exists AND is initialized
        if not self._controller or not self._isInitialized then
            return
        end
        
        -- Update the internal jolt
        CharacterController.Update(self._controller, dt)
        
        -- Move once
        if not hasMoved then
            print("[LUA] Moving character")
            CharacterController.Move(self._controller, 0, -1, 0)
            hasMoved = true
        end
        
        -- Get Position from Jolt and pass it to ECS
        local position = CharacterController.GetPosition(self._controller)
        if position then
            self:SetPosition(position.x, position.y, position.z)
        else
            print("[LUA WARNING] GetPosition returned nil")
        end
        
        -- Jump when grounded
        if CharacterController.IsGrounded(self._controller) then
            CharacterController.Jump(self._controller, 2.0)
        end
        
        -- -- Debug output 
        -- local gravity = CharacterController.GetGravity(self._controller)
        -- if gravity then
        --     print("Gravity is ", gravity.y)
        -- end
        
        -- local vel = CharacterController.GetVelocity(self._controller)
        -- if vel then
        --     print("Velocity for x is ", vel.x)
        --     print("Velocity for y is ", vel.y)
        --     print("Velocity for z is ", vel.z)
        -- end
    end,
    
    OnDisable = function(self)
        print("[LUA] OnDisable called - cleaning up controller")
        
        if self._controller then
            CharacterController.Destroy(self._controller)
        end
        
        self._controller = nil
        self._isInitialized = false
    end
}