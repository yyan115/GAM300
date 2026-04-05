-- This script should handle all CheckPoint VFX (PLAYER + SHRINE)

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        -- === Start Color ===
        startColorX = 1.0,
        startColorY = 0.82,
        startColorZ = 0,
        startAlpha  = 0.050,
        -- === End Color ===
        endColorX   = 1.0,
        endColorY   = 0.82,
        endColorZ   = 0,
        endAlpha    = 0.1,
        -- === Healing VFX Duration ===
        healingVFXDuration = 2.5,   -- seconds before emitters turn off
    },

    Start = function(self)
        self._playerTr     = Engine.FindTransformByName("Player")
        self._healingVFXTr = Engine.FindTransformByName("HealingVFX")
        self._healingVFX   = Engine.GetEntityByName("HealingVFX")
        self._healingTimer = 0
        self._healingActive = false

        if not self._playerTr then
            --print("PlayerHealingVFX: Player transform not found")
        end

        if not self._healingVFXTr then
            --print("PlayerHealingVFX: HealingVFX transform not found")
        else
            self:SetHealingEmitters(false)  -- ensure off at start
        end

        if not _G.event_bus then
            --print("event_bus NOT FOUND")
            return
        end

        _G.event_bus.subscribe("activatedCheckpoint", function(data)
            --print("PARENT ENTITY IS: " .. tostring(data.parent))
            self._cpEnt = data.parent

            self._cpParticle = GetComponent(self._cpEnt, "ParticleComponent")
            if self._cpParticle then
                self:ActivateCheckPointVFX()
            end

            self:ActivateHealingVFX()
        end)
    end,

    Update = function(self, dt)
        -- Continuously snap healing VFX to player every frame
        if self._playerTr and self._healingVFXTr then
            self._healingVFXTr.localPosition.x = self._playerTr.localPosition.x
            self._healingVFXTr.localPosition.y = self._playerTr.localPosition.y
            self._healingVFXTr.localPosition.z = self._playerTr.localPosition.z
            self._healingVFXTr.isDirty = true
        end

        -- shut off emitters after duration
        if self._healingActive then
            self._healingTimer = self._healingTimer - dt
            if self._healingTimer <= 0 then
                self._healingActive = false
                self:SetHealingEmitters(false)
            end
        end
    end,

    ActivateCheckPointVFX = function(self)
        local startColor = self._cpParticle.startColor
        startColor.y = self.startColorY
        startColor.z = self.startColorZ
        self._cpParticle.startColor = startColor

        local endColor = self._cpParticle.endColor
        endColor.y = self.endColorY
        endColor.z = self.endColorZ
        self._cpParticle.endColor = endColor

        self._cpParticle.startColorAlpha = self.startAlpha
        self._cpParticle.endColorAlpha   = self.endAlpha
    end,

    ActivateHealingVFX = function(self)
        if not Engine.GetChildrenEntities or not self._healingVFX then
            return
        end

        self:SetHealingEmitters(true)
        self._healingTimer  = self.healingVFXDuration  -- reset timer every activation
        self._healingActive = true
    end,

    -- Shared helper to toggle all child emitters on/off
    SetHealingEmitters = function(self, state)
        if not self._healingVFX then return end
        local children = Engine.GetChildrenEntities(self._healingVFX)
        for _, childId in ipairs(children) do
            local particle = GetComponent(childId, "ParticleComponent")
            if particle then
                particle.isEmitting = state
            end
        end
    end,
}