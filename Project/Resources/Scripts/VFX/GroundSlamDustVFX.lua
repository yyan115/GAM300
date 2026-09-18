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
        self._rockTransform.localPosition.x = x

        self._transform.localPosition.y = y
        self._rockTransform.localPosition.y = y

        self._transform.localPosition.z = z
        self._rockTransform.localPosition.z = z 

        self._transform.isDirty = true
        self._rockTransform.isDirty = true

        if self._dustParticle and self._rockParticle then
            self._dustParticle.isEmitting = true
            self._rockParticle.isEmitting = true
            self._emitTimer = self._dustParticle.particleLifetime
        end
    end,


    -- A warning rather than an impact: the same dust and rock the slam uses,
    -- held for as long as the warning lasts instead of a single burst. Calling
    -- it again moves the effect, which is how the dive mark follows the boss.
    WarnAt = function(self, x, y, z, seconds)
        if not (self._transform and self._dustParticle) then return end

        self._transform.localPosition.x = x
        self._transform.localPosition.y = y
        self._transform.localPosition.z = z
        self._transform.isDirty = true

        if self._rockTransform then
            self._rockTransform.localPosition.x = x
            self._rockTransform.localPosition.y = y
            self._rockTransform.localPosition.z = z
            self._rockTransform.isDirty = true
        end

        self._dustParticle.isEmitting = true
        if self._rockParticle then
            self._rockParticle.isEmitting = true
        end
        local hold = tonumber(seconds) or 0.6
        self._emitTimer = math.max(hold, self._emitTimer or 0)
    end,

    Start = function(self)

        self._transform = self:GetComponent("Transform")    
        self._dustParticle = self:GetComponent("ParticleComponent")

        local rockEntity = Engine.GetEntityByName("RockParticlesVFX")
        self._rockParticle = GetComponent(rockEntity,"ParticleComponent")
        self._rockTransform = GetComponent(rockEntity, "Transform")

        self._rockParticle.isEmitting = false
        self._dustParticle.isEmitting = false


        self._trackedEnemyAnim = nil
        self._BeginSlamDownSub = event_bus.subscribe("SlammedDown", function(payload)
            if payload and payload.targetId then
                self:SpawnGroundDustVFX(payload.posX, payload.posY, payload.posZ)       
            end
        end)

        -- The miniboss's own landing. It does not publish SlammedDown, because
        -- GroundSlamVFX listens to that too and would put down its crack decal.
        self._minibossSlamSub = event_bus.subscribe("miniboss_slammed", function(payload)
            if payload and payload.targetId then
                self:SpawnGroundDustVFX(payload.posX, payload.posY, payload.posZ)
            end
        end)

        -- The miniboss hovers above the player for about a second before it
        -- drops, and that second reads as nothing without a mark. Dust and rock
        -- mark the ground it is coming down on for as long as the wait lasts.
        self._slamWarningSub = event_bus.subscribe("miniboss_slam_warning", function(payload)
            if payload and payload.targetId then
                self:WarnAt(payload.posX, payload.posY, payload.posZ, payload.seconds)
            end
        end)

        -- Its charged slash crosses the room. Dust and rock at its feet while it
        -- winds up are the difference between a surprise and an attack dodged.
        self._chargeWarningSub = event_bus.subscribe("miniboss_charge_warning", function(payload)
            if payload and payload.targetId then
                self:WarnAt(payload.posX, payload.posY, payload.posZ, payload.seconds)
            end
        end)
    end,
    Update = function(self, dt)
        if self._emitTimer then
            self._emitTimer = self._emitTimer - dt
            if self._emitTimer <= 0 then
                self._dustParticle.isEmitting = false
                self._rockParticle.isEmitting = false
                self._emitTimer = nil
            end
        end
    end,
    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            for _, key in ipairs({"_BeginSlamDownSub", "_minibossSlamSub", "_slamWarningSub", "_chargeWarningSub"}) do
                if self[key] then
                    pcall(function() _G.event_bus.unsubscribe(self[key]) end)
                    self[key] = nil
                end
            end
        end
        self._trackedEnemyAnim = nil
    end,
}