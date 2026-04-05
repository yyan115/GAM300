require("extension.engine_bootstrap")

local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },
    
    fields = {
        Target = "Kusane_Player_LeftHand",
        EmitDuration = 0.5 -- How long the sparks stay on (in seconds)
    },

    SpawnChainSparkVFX = function(self, x, y, z)
        self._transform.localPosition.x = x
        self._transform.localPosition.y = y
        self._transform.localPosition.z = z
        self._transform.isDirty = true
        
        if self._sparkParticle then
            self._sparkParticle.isEmitting = true
            -- Set the timer to the particle's lifetime or your custom EmitDuration
            self._emitTimer = self.EmitDuration 
        end
    end,

    Start = function(self)
        self._inputInterpreter = _G.InputInterpreter
        if not self._inputInterpreter then return end

        self._chainThrown = false
        self._emitTimer = 0 -- Initialize timer at 0

        self.Target = self.Target:gsub('"', '')
        self._transform = self:GetComponent("Transform")
        self._sparkParticle = self:GetComponent("ParticleComponent")
        
        if self._sparkParticle then
            self._sparkParticle.isEmitting = false
        end
        
        self._targetTransform = Engine.FindTransformByName(self.Target)
    end,

    Update = function(self, dt)
        -- Always track hand position
        if self._targetTransform then
            self._transform.localPosition.x = self._targetTransform.worldPosition.x
            self._transform.localPosition.y = self._targetTransform.worldPosition.y
            self._transform.localPosition.z = self._targetTransform.worldPosition.z
            self._transform.isDirty = true
        end

        -- Handle Input Trigger
        local input = self._inputInterpreter
        if _G.playerHasWeapon and not _G.player_is_dashing then
            if input:IsChainJustPressed() then
                if self._sparkParticle then
                    self._sparkParticle.isEmitting = true
                    self._emitTimer = self.EmitDuration -- Reset the countdown
                end
            end
        end

        -- --- TIMER LOGIC ---
        if self._emitTimer > 0 then
            self._emitTimer = self._emitTimer - dt
            if self._emitTimer <= 0 then
                if self._sparkParticle then
                    self._sparkParticle.isEmitting = false
                end
                self._emitTimer = 0
            end
        end
    end,

    OnDisable = function(self)
        if self._sparkParticle then
            self._sparkParticle.isEmitting = false
        end
        self._emitTimer = 0
        self._chainThrown = false
    end,
}