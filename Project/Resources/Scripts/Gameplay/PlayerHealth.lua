require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local Input = _G.Input

-- Animation States
local HurtTrigger = "Hurt"

local function PlayerTakeDmg(self, dmg)
    print("[PlayerTakeDmg] Animator set trigger Hurt")
    self._animator:SetTrigger(HurtTrigger)
    self._hurtTriggered = true

    self._currentHealth = self._currentHealth - dmg
    print(string.format("[PlayerTakeDmg] Player took %d damage. Remaining health: %d", dmg, self._currentHealth))
    
    if self._currentHealth <= 0 then
        self._currentHealth = 0
        self._animator:SetBool("IsDead", true)

        event_bus.publish("playerDead", true)
    end

    if event_bus and event_bus.publish then
        event_bus.publish("playerMaxhealth", self._maxHealth)
        event_bus.publish("playerCurrentHealth", self._currentHealth)
    end

    self._knifeHitPlayer = false
end

return Component {
    mixins = { TransformMixin },

    fields = {
        Health = 10,
        IFrameDuration = 1.0,
    },

    Awake = function(self)
        self._iFrameDuration = self.IFrameDuration
        self._animator  = self:GetComponent("AnimationComponent")
        self._maxHealth = self.Health
        self._currentHealth = self._maxHealth

        if event_bus and event_bus.subscribe then
            print("[PlayerHealth] Subscribing to knifeHitPlayerDmg")
            self._knifeHitPlayerDmgSub = event_bus.subscribe("knifeHitPlayerDmg", function(dmg)
                if dmg then
                    if self._isIFrame == false then
                        PlayerTakeDmg(self, dmg)
                    end

                    self._isIFrame = true
                end
            end)
            print("[PlayerHealth] Subscription token: " .. tostring(self._knifeHitPlayerSub))
        else
            print("[PlayerHealth] ERROR: event_bus not available!")
        end
    end,

    Start = function(self)
        if event_bus and event_bus.publish then
            event_bus.publish("playerMaxhealth", self._maxHealth)
            event_bus.publish("playerCurrentHealth", self._currentHealth)
        end
    end,

    Update = function(self, dt)
        if self._hurtTriggered then
            if event_bus and event_bus.publish then
                print("[PlayerHealth] playerHurtTriggered published")
                event_bus.publish("playerHurtTriggered", true)
                self._hurtTriggered = false
            end
        end

        if self._isIFrame == true then
            self._iFrameDuration = self._iFrameDuration - dt
            if self._iFrameDuration <= 0 then
                self._iFrameDuration = self.IFrameDuration
                self._isIFrame = false
            end
        end
    end,

    OnDisable = function(self)

    end,
}