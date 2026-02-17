-- Resources/Scripts/Gameplay/AttackHitbox.lua
-- Attach to the weapon collider entity (child of weapon bone).
-- Requires: RigidBodyComponent (isKinematic=true, isTrigger=true) + ColliderComponent
--
-- Listens to ComboManager events to know when attacks are active.
-- Uses C++ OnTriggerEnter for hit detection instead of distance checks.
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
    end,

    -- Use Start instead of Awake for subscriptions (Awake can be called multiple times)
    Start = function(self)
        -- Guard against double subscription
        if self._subscribed then return end
        self._subscribed = true

        -- Cache the player entity ID so we can filter it out in OnTriggerEnter
        if Engine and Engine.GetEntityByName then
            self._playerEntityId = Engine.GetEntityByName("Player")
        end

        if event_bus and event_bus.subscribe then
            -- Activate on attack (from ComboManager)
            self._subAttack = event_bus.subscribe("attack_performed", function(data)
                self._active = true
                self._hitThisSwing = {}
                self._currentDamage = (data and data.damage) or self.damage or 10
            end)

            -- Deactivate when returning to idle
            self._subState = event_bus.subscribe("combat_state_changed", function(data)
                if data and data.state == "idle" then
                    self._active = false
                end
            end)
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe then
            if self._subAttack then event_bus.unsubscribe(self._subAttack) end
            if self._subState then event_bus.unsubscribe(self._subState) end
        end
        self._subAttack = nil
        self._subState = nil
        self._active = false
        self._subscribed = false
    end,

    -- Called by C++ when this trigger overlaps another collider
    OnTriggerEnter = function(self, otherEntityId)
        if not self._active then return end

        -- Resolve bone entity to root entity (hurtboxes are on bone children)
        local targetId = otherEntityId
        if Engine and Engine.GetParentEntity then
            while true do
                local parentId = Engine.GetParentEntity(targetId)
                if not parentId or parentId < 0 then break end
                targetId = parentId
            end
        end

        -- Skip the player entity (weapon is child of player, always overlaps)
        if self._playerEntityId and targetId == self._playerEntityId then return end

        -- Deduplicate: only hit each entity once per swing (by root entity ID)
        if self._hitThisSwing[targetId] then return end
        self._hitThisSwing[targetId] = true

        if event_bus and event_bus.publish then
            event_bus.publish("deal_damage_to_entity", {
                entityId = targetId,
                damage   = self._currentDamage,
            })
            print("[AttackHitbox] Dealt " .. tostring(self._currentDamage)
                .. " damage to entity " .. tostring(targetId))
        end
    end,
}
