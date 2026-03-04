-- Resources/Scripts/Gameplay/AttackHitbox.lua
-- Attach to the weapon collider entity (child of weapon bone).
-- Requires: RigidBodyComponent (isKinematic=true, isTrigger=true) + ColliderComponent
--
-- The RigidBody is disabled by default and only enabled during the active hit window.
-- This guarantees OnTriggerEnter fires fresh every swing, even if the enemy is
-- already inside the weapon collider (since removing + re-adding the body resets contact state).
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

return Component {
    fields = {
        damage = 10,
    },

    Awake = function(self)
        self._active = false
        self._hitThisSwing = {}
        self._currentDamage = self.damage or 10
        self._subscribed = false
        self._playerEntityId = nil
        self._rb = nil
    end,

    Start = function(self)
        if self._subscribed then return end
        self._subscribed = true

        -- Cache RigidBody and disable it — weapon has no physics presence until attacking
        self._rb = self:GetComponent("RigidBodyComponent")
        if self._rb then
            self._rb:SetEnabled(false)
        end

        if Engine and Engine.GetEntityByName then
            self._playerEntityId = Engine.GetEntityByName("Player")
        end

        if event_bus and event_bus.subscribe then
            -- Hit window opens: drop the body into the physics world
            self._subAttack = event_bus.subscribe("attack_performed", function(data)
                self._active = true
                self._hitThisSwing = {}
                self._currentDamage = (data and data.damage) or self.damage or 10
                if self._rb then self._rb:SetEnabled(true) end
            end)

            -- Hit window closes: pull the body out on every state change — clears all contact state.
            -- This means attack→attack transitions also cycle the RB off, so OnTriggerEnter
            -- fires fresh for the next swing even if the enemy is already inside the collider.
            self._subState = event_bus.subscribe("combat_state_changed", function(data)
                self._active = false
                if self._rb then self._rb:SetEnabled(false) end
            end)
        end
    end,

    OnDisable = function(self)
        self._active = false
        if self._rb then self._rb:SetEnabled(false) end

        if event_bus and event_bus.unsubscribe then
            if self._subAttack then event_bus.unsubscribe(self._subAttack) end
            if self._subState  then event_bus.unsubscribe(self._subState)  end
        end
        self._subAttack  = nil
        self._subState   = nil
        self._subscribed = false
    end,

    -- Walk up the hierarchy to find the root entity
    _toRoot = function(self, entityId)
        local targetId = entityId
        if Engine and Engine.GetParentEntity then
            while true do
                local parentId = Engine.GetParentEntity(targetId)
                if not parentId or parentId < 0 then break end
                targetId = parentId
            end
        end
        return targetId
    end,

    OnTriggerEnter = function(self, otherEntityId)
        if not self._active then return end

        local targetId = self:_toRoot(otherEntityId)

        print("[AttackHitbox] HIT targetId=", tostring(targetId), "from otherEntityId=", tostring(otherEntityId))

        if self._playerEntityId and targetId == self._playerEntityId then return end
        if self._hitThisSwing[targetId] then return end
        self._hitThisSwing[targetId] = true

        if event_bus and event_bus.publish then
            event_bus.publish("deal_damage_to_entity", {
                entityId = targetId,
                damage   = self._currentDamage,
                hitType  = "COMBO",
            })
            print("[AttackHitbox] Dealt " .. tostring(self._currentDamage)
                .. " damage to entity " .. tostring(targetId))
        end
    end,
}
