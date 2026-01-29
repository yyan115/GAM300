require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

-- Animation States
local HurtTrigger = "Hurt"

local function PlayerTakeDmg(self, dmg)
    print("[PlayerTakeDmg] Animator set trigger Hurt")
    self._animator:SetTrigger(HurtTrigger)

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
        Health = 10
    },

    Awake = function(self)
        print("[PlayerHealth] Health initialized to ", self.Health)

        if event_bus and event_bus.subscribe then
            print("[PlayerHealth] Subscribing to isKnifeHitPlayer")
            self._knifeHitPlayerSub = event_bus.subscribe("isKnifeHitPlayer", function(hit)
                if hit then
                    self._knifeHitPlayer = hit
                end
            end)
            print("[PlayerHealth] Subscription token: " .. tostring(self._knifeHitPlayerSub))

            print("[PlayerHealth] Subscribing to knifeHitPlayerDmg")
            self._knifeHitPlayerDmgSub = event_bus.subscribe("knifeHitPlayerDmg", function(dmg)
                if dmg then
                    self._knifeHitPlayerDmg = dmg
                end
            end)
            print("[PlayerHealth] Subscription token: " .. tostring(self._knifeHitPlayerSub))
        else
            print("[PlayerHealth] ERROR: event_bus not available!")
        end
    end,

    Start = function(self)
        self._animator  = self:GetComponent("AnimationComponent")

        self._maxHealth = self.Health
        self._currentHealth = self._maxHealth

        if event_bus and event_bus.publish then
            event_bus.publish("playerMaxhealth", self._maxHealth)
            event_bus.publish("playerCurrentHealth", self._currentHealth)
        end
    end,

    Update = function(self, dt)
        if not self._animator then 
            return
        end

        if self._knifeHitPlayer == true then
            local dmg = self._knifeDmg or 1
            PlayerTakeDmg(self, dmg)
        end
    end,

    OnDisable = function(self)

    end,
}