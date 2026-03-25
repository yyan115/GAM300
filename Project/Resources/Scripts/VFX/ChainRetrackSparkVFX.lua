require("extension.engine_bootstrap")

local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },
    
    fields = {
        Target = "Kusane_Player_LeftHand"
    },


    SpawnChainSparkVFX = function(self, x,y,z)
        --Set VFX AT Slammed location
        self._transform.localPosition.x = x
        self._transform.localPosition.y = y
        self._transform.localPosition.z = z
        self._transform.isDirty = true
        if self._dustParticle and self._rockParticle then
            self._dustParticle.isEmitting = true
            self._emitTimer = self._dustParticle.particleLifetime
        end
    end,

-- Engine.GetTransformWorldPosition(playerTransform)
    Start = function(self)

        self._inputInterpreter = _G.InputInterpreter
        if not self._inputInterpreter then
            print("ERROR: InputInterpreter not found!")
            return
        end

        self._chainThrown = false

        -- [TEMP FIX] 
        self.Target = self.Target:gsub('"', '')
        self._transform = self:GetComponent("Transform")
        self._sparkParticle = self:GetComponent("ParticleComponent")
        self._sparkParticle.isEmitting = false
        self._targetTransform = Engine.FindTransformByName(self.Target)

    end,
    Update = function(self, dt)
        -- Always track hand position
        self._transform.localPosition.x = self._targetTransform.worldPosition.x
        self._transform.localPosition.y = self._targetTransform.worldPosition.y
        self._transform.localPosition.z = self._targetTransform.worldPosition.z
        self._transform.isDirty = true

        local input = self._inputInterpreter
        if _G.playerHasWeapon and not _G.player_is_dashing and event_bus then
            if input:IsChainJustPressed() then
                self._sparkParticle.isEmitting = not self._sparkParticle.isEmitting
            end
        end
    end,


    OnDisable = function(self)
        if self._sparkParticle then
            self._sparkParticle.isEmitting = false
        end
        self._chainThrown = false
    end,
}