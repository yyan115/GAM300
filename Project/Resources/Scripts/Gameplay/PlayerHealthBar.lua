require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    Awake = function(self)
        if event_bus and event_bus.subscribe then
            print("[PlayerHealthBar] Subscribing to playerMaxhealth")
            self._playerMaxHealthSub = event_bus.subscribe("playerMaxhealth", function(maxHealth)
                if maxHealth then
                    self._maxHealth = maxHealth
                end
            end)
            print("[PlayerHealthBar] Subscription token: " .. tostring(self._playerMaxHealthSub))

            print("[PlayerHealthBar] Subscribing to playerCurrentHealth")
            self._playerCurrentHealthSub = event_bus.subscribe("playerCurrentHealth", function(currentHealth)
                if currentHealth then
                    self._currentHealth = currentHealth
                end
            end)
            print("[PlayerHealthBar] Subscription token: " .. tostring(self._playerCurrentHealthSub))
        else
            print("[PlayerHealthBar] ERROR: event_bus not available!")
        end
    end,

    Start = function(self)
        self._transform  = self:GetComponent("Transform")
    end,

    Update = function(self, dt)
        if not self._transform then 
            return
        end

        local maxHealth = self._maxHealth or 10
        local currentHealth = self._currentHealth or 10
        local healthPercentage = currentHealth / maxHealth
        if currentHealth == 0 then
            healthPercentage = 0.0
        end
        self._transform.localScale.x = healthPercentage
        self._transform.localPosition.x = -(1.0 - healthPercentage) * 0.5
        self._transform.isDirty = true
    end,

    OnDisable = function(self)

    end,
}