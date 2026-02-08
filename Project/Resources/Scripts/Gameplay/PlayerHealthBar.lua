require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    fields = {
        followDelay = 0.5,        -- Delay before HealthBarFill starts following (seconds)
        followSpeed = 2.0,        -- Speed of the smooth follow (higher = faster)
        healthBarFillName = "HealthBarFill"  -- Name of the entity to follow
    },

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
        self._transform = self:GetComponent("Transform")

        -- Get the HealthBarFill entity
        self._healthBarFillEntity = Engine.GetEntityByName(self.healthBarFillName)
        if self._healthBarFillEntity then
            self._healthBarFillTransform = GetComponent(self._healthBarFillEntity, "Transform")
            if self._healthBarFillTransform then
                -- Initialize the fill bar to match current health
                self._currentFillScale = self._healthBarFillTransform.localScale.x
            end
        else
            print("[PlayerHealthBar] WARNING: Could not find entity: " .. self.healthBarFillName)
        end

        self._delayTimer = 0.0
        self._previousHealthPercentage = 1.0
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

        -- Update HealthBarFillFollow (this entity) immediately
        self._transform.localScale.x = healthPercentage
        self._transform.localPosition.x = -(1.0 - healthPercentage) * 0.5
        self._transform.isDirty = true

        -- Update HealthBarFill with delay and smooth follow
        if self._healthBarFillTransform then
            -- Check if health changed (decreased)
            if healthPercentage < self._previousHealthPercentage then
                self._delayTimer = 0.0  -- Reset delay timer on health decrease
            end

            self._previousHealthPercentage = healthPercentage

            -- Only update fill bar after delay
            if self._delayTimer >= self.followDelay then
                -- Smoothly interpolate toward target scale
                local targetScale = healthPercentage
                self._currentFillScale = self._currentFillScale or targetScale

                -- Lerp toward target
                self._currentFillScale = self._currentFillScale + (targetScale - self._currentFillScale) * (self.followSpeed * dt)

                -- Clamp to prevent overshooting
                if math.abs(self._currentFillScale - targetScale) < 0.001 then
                    self._currentFillScale = targetScale
                end

                self._healthBarFillTransform.localScale.x = self._currentFillScale
                self._healthBarFillTransform.localPosition.x = -(1.0 - self._currentFillScale) * 0.5
                self._healthBarFillTransform.isDirty = true
            else
                self._delayTimer = self._delayTimer + dt
            end
        end
    end,

    OnDisable = function(self)

    end,
}