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

        print("[PlayerHealth] Subscribing to meleeHitPlayerDmg")
        self._meleeHitPlayerDmgSub = event_bus.subscribe("meleeHitPlayerDmg", function(payload)
            if not payload then return end
            if self._isIFrame then return end

            local dmg = payload
            if type(payload) == "table" then
                dmg = payload.dmg
            end

            if dmg ~= nil then
                PlayerTakeDmg(self, dmg)
                self._isIFrame = true
            end
        end)
        print("[PlayerHealth] Subscription token (melee): " .. tostring(self._meleeHitPlayerDmgSub))

        print("[PlayerHealth] Subscribing to miniboss_slash")
            self._minibossSlashSub = event_bus.subscribe("miniboss_slash", function(payload)
            if not payload then return end
            if self._isIFrame then return end

            -- payload: x,y,z,radius,dmg,entityId
            local px, py, pz = self:GetPosition()
            if not px then return end

            local dx = (px - (payload.x or 0))
            local dz = (pz - (payload.z or 0))
            local r  = (payload.radius or 1.4)

            -- simple XZ circle hit
            if (dx*dx + dz*dz) <= (r*r) then
                PlayerTakeDmg(self, payload.dmg or 1)
                self._isIFrame = true
            end
        end)
        print("[PlayerHealth] Subscribed to miniboss_slash: " .. tostring(self._minibossSlashSub))

        print("[PlayerHealth] Subscribing to respawnPlayer")
        self._respawnPlayerSub = event_bus.subscribe("respawnPlayer", function(respawn)
            if respawn then
                self._respawnPlayer = true
            end
        end)

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

    RespawnPlayer = function(self)
        self._currentHealth = self._maxHealth
        if event_bus and event_bus.publish then
            event_bus.publish("playerMaxhealth", self._maxHealth)
            event_bus.publish("playerCurrentHealth", self._currentHealth)
        end

        self._respawnPlayer = false
    end,

    Update = function(self, dt)
        if self._respawnPlayer then
            self.RespawnPlayer(self)
        end

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