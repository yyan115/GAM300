require("extension.engine_bootstrap")

local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local FlyingHookedState = require("Gameplay.FlyingHookedState")

local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },
    
    fields = {
    },


    SpawnGroundDustVFX = function(self, x,y,z)
        --Set VFX AT Slammed location
        self._transform.localPosition.x = x
        self._transform.localPosition.y = y
        self._transform.localPosition.z = z 
        self._transform.isDirty = true

        if self._dustParticle then
            self._dustParticle.isEmitting = true
            self._emitTimer = self._dustParticle.particleLifetime
        end
    end,


    Start = function(self)

        self._transform = self:GetComponent("Transform")    
        self._dustParticle = self:GetComponent("ParticleComponent")
        self._dustParticle.isEmitting = false


        self._trackedEnemyAnim = nil
        self._BeginSlamDownSub = event_bus.subscribe("SlammedDown", function(payload)
            if payload and payload.targetId then
                self:SpawnGroundDustVFX(payload.posX, payload.posY, payload.posZ)       
            end
        end)
    end,
    Update = function(self, dt)
        if self._emitTimer then
            self._emitTimer = self._emitTimer - dt
            if self._emitTimer <= 0 then
                self._dustParticle.isEmitting = false
                self._emitTimer = nil
            end
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