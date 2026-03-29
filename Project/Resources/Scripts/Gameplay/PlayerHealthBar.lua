require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

return Component {
    fields = {
        followDelay = 0.5,       -- Seconds before ghost bar starts following
        followSpeed = 2.0,       -- Lerp speed of ghost bar (higher = faster)
        healthBarFillName = "HealthBarFill",  -- Name of the ghost/delayed bar entity
    },

    Awake = function(self)
        if event_bus and event_bus.subscribe then
            self._playerMaxHealthSub = event_bus.subscribe("playerMaxhealth", function(maxHealth)
                if maxHealth then self._maxHealth = maxHealth end
            end)
            self._playerCurrentHealthSub = event_bus.subscribe("playerCurrentHealth", function(currentHealth)
                if currentHealth then self._currentHealth = currentHealth end
            end)
        else
            print("[PlayerHealthBar] ERROR: event_bus not available!")
        end
    end,

    Start = function(self)
        -- This entity is the immediate health bar fill
        self._sprite = self:GetComponent("SpriteRenderComponent")

        -- Ghost bar entity (delayed, lerps behind)
        local ghostEnt = Engine.GetEntityByName(self.healthBarFillName)
        if ghostEnt then
            self._ghostSprite = GetComponent(ghostEnt, "SpriteRenderComponent")
        else
            print("[PlayerHealthBar] WARNING: Could not find ghost bar: " .. self.healthBarFillName)
        end

        self._delayTimer = 0.0
        self._ghostFill  = 1.0
        self._previousHealthPct = 1.0
    end,

    Update = function(self, dt)
        local maxHealth = self._maxHealth or 10
        local currentHealth = self._currentHealth or maxHealth
        local pct = math.max(0.0, math.min(1.0, currentHealth / maxHealth))

        -- Immediate bar: just set fillValue directly
        if self._sprite then
            self._sprite.fillValue = pct
        end

        -- Ghost bar: wait for delay, then lerp toward current health
        if self._ghostSprite then
            if pct < self._previousHealthPct then
                self._delayTimer = 0.0  -- Reset delay when health drops
            end
            self._previousHealthPct = pct

            if self._delayTimer >= self.followDelay then
                self._ghostFill = self._ghostFill + (pct - self._ghostFill) * (self.followSpeed * dt)
                if math.abs(self._ghostFill - pct) < 0.001 then
                    self._ghostFill = pct
                end
                self._ghostSprite.fillValue = self._ghostFill
            else
                self._delayTimer = self._delayTimer + dt
            end
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe then
            if self._playerMaxHealthSub then event_bus.unsubscribe(self._playerMaxHealthSub) end
            if self._playerCurrentHealthSub then event_bus.unsubscribe(self._playerCurrentHealthSub) end
        end
    end,
}
