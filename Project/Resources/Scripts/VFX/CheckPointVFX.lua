--This script should handle all CheckPoint VFX (PLAYER + SHRINE)

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        -- ===Start Color===
        startColorX = 1.0,
        startColorY = 0.82,
        startColorZ = 0,
        startAlpha  = 0.050,
        -- ===End Color===
        endColorX   = 1.0,
        endColorY   = 0.82,
        endColorZ   = 0,
        endAlpha    = 0.1,
    },
    Start = function(self)
        if _G.event_bus then
            -- CHECK ACTIVATED CHECKPOINT EVENT
            _G.event_bus.subscribe("activatedCheckpoint", function(data)
                print("PARENT ENTITY IS: " .. tostring(data.parent))
                self._cpEnt = data.parent
                --CHECK FOR PARTICLE Component
                self._cpParticle = GetComponent(self._cpEnt, "ParticleComponent") 
                if self._cpParticle then
                    self:ActivateCheckPointVFX()
                end
        end)
        else
            print("event_bus NOT FOUND")
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

    --TO DO PLAYER HEALING VFX (HEALS WHEN CHECKPOINT ACTIVATED)



}
