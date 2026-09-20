require("extension.engine_bootstrap")

local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local FlyingHookedState = require("Gameplay.FlyingHookedState")

local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },
    
    fields = {
        -- Height above the floor, so the crack does not fight the floor for
        -- depth and shimmer as the camera moves
        FloorOffset = 0.02,
        -- The crack goes when the slammed enemy stands up, and after this many
        -- seconds whatever else happens: an enemy the slam or a hit kills
        -- never stands up
        MaxSeconds = 2.5,
    },


    SpawnGroundSlamVFX = function(self, x,y,z)
        --print("Spawning Ground Slam VFX at: ", x,y,z)
        --Set VFX AT Slammed location
        self._transform.localPosition.x = x
        self._transform.localPosition.y = y + (self.FloorOffset or 0)
        self._transform.localPosition.z = z
        self._transform.isDirty = true

        if self.model then
            ModelRenderComponent.SetVisible(self.model, true)
        end
        self._shownFor = 0
    end,

    _Hide = function(self)
        if self.model then
            ModelRenderComponent.SetVisible(self.model, false)
        end
        self._trackedEnemyAnim = nil
        self._shownFor = nil
    end,


    Start = function(self)

        self._transform = self:GetComponent("Transform")    
        self.model = self:GetComponent("ModelRenderComponent")
        -- Initial Visibility
        if self.model then 
            ModelRenderComponent.SetVisible(self.model, false) 
        end

        self._trackedEnemyAnim = nil
        self._BeginSlamDownSub = event_bus.subscribe("SlammedDown", function(payload)
            if payload and payload.targetId then
                --GET ENEMY ANIMATION, POSITION, MIGHT NEED TO BE ON UPDATE
                local enemyId = payload.targetId
                self._trackedEnemyAnim = GetComponent(enemyId, "AnimationComponent")

                self:SpawnGroundSlamVFX(payload.posX, payload.posY, payload.posZ)       --Land Position of enemy
            end
        end)
    end,
    
    Update = function(self, dt)
        if not self._shownFor then return end
        self._shownFor = self._shownFor + (dt or 0)
        local stoodUp = self._trackedEnemyAnim ~= nil
            and self._trackedEnemyAnim:GetCurrentState() == "Stand Up"
        if stoodUp or self._shownFor >= (self.MaxSeconds or 2.5) then
            self:_Hide()
        end
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._BeginSlamDownSub then
                pcall(function()
                    _G.event_bus.unsubscribe(self._BeginSlamDownSub)
                end)
                self._BeginSlamDownSub = nil
            end
        end
        self._trackedEnemyAnim = nil
    end,
}