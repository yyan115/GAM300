require("extension.engine_bootstrap")

local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local FlyingHookedState = require("Gameplay.FlyingHookedState")

local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },
    
    fields = {

    },
    
    SpawnGroundSlamVFX = function(self, x,y,z)
        print("Spawning Ground Slam VFX at: ", x,y,z)
        --Set VFX AT Slammed location
        self._transform.localPosition.x = x
        self._transform.localPosition.y = y
        self._transform.localPosition.z = z 
        self._transform.isDirty = true

        if self.model then
            ModelRenderComponent.SetVisible(self.model, true)
        end
    end,


    Start = function(self)

        self._transform = self:GetComponent("Transform")    
        self.model = self:GetComponent("ModelRenderComponent")
        -- Initial Visibility
        if self.model then 
            ModelRenderComponent.SetVisible(self.model, false) 
        end

        self._BeginSlamDownSub = event_bus.subscribe("SlammedDown", function(payload)
            if payload and payload.targetId then
                --GET ENEMY ANIMATION, POSITION, MIGHT NEED TO BE ON UPDATE
                local enemyId = payload.targetId
                local pulledEnemyAnim = GetComponent(enemyId, "AnimationComponent")

                self:SpawnGroundSlamVFX(payload.posX, payload.posY, payload.posZ)       --Land Position of enemy
            end
        end)
    end,
    
    Update = function(self, dt)
    end,

    OnDisable = function(self)
    -- if _G.event_bus and _G.event_bus.unsubscribe then
    --     if self._chainAimSub then
    --         pcall(function()
    --             _G.event_bus.unsubscribe(self._chainAimSub)
    --         end)
    --         self._chainAimSub = nil
        end

}