require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local controller = nil
return Component {
    mixins = { TransformMixin },
        
    Awake = function(self)
    end,
    
    Start = function(self) 
        self._collider = self:GetComponent("ColliderComponent")
        self._transform = self:GetComponent("Transform")

        controller = CharacterController.new()
        CharacterController.Initialise(controller, self._collider, self._transform)

    end,
    
    Update = function(self, dt) 
        if self._collider == nil or self._transform == nil then
            print("collider or transform is nil")
        end

        -- CharacterController.Update(controller, dt)

        -- if Input.GetKey(Input.Key.W) then 
        CharacterController.Move(controller, 0,1,0)
    end
}

--  Animation clip indices:
--  0 = IDLE
--  1 = ATTACK
--  2 = TAKEDAMAGE
--  3 = DEATH