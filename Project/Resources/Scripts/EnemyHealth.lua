-- EnemyHealth.lua
require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    -- Exposed in inspector
    fields = {
        maxHealth      = 50,
        debugPrintHits = true,
    },

    Awake = function(self)
        self.health        = self.maxHealth or 50
        self._sub          = nil
        self._isDead       = false -- stop reacting after death
        self._myEntityId   = self.entityId  -- cache our own entity ID

        if event_bus and event_bus.subscribe then
            self._sub = event_bus.subscribe("deal_damage_to_entity", function(payload)
                self:_onDamage(payload)
            end)
        else
            print("[LUA][EnemyHealth] WARNING: event_bus not available; will never get damage events")
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe and self._sub then
            event_bus.unsubscribe(self._sub)
            self._sub = nil
        end
    end,

    _onDamage = function(self, payload)
        if not payload then return end
        if self._isDead then return end

        -- Only apply if this event targets our entity
        if payload.entityId ~= self._myEntityId then return end

        local dmg = payload.damage or 1

        self.health = self.health - dmg

        if self.debugPrintHits then
            print(string.format(
                "[LUA][EnemyHealth] Took %d damage from trigger. HP = %d",
                dmg, self.health
            ))
        end

        if self.health <= 0 then
            self._isDead = true

            if self.debugPrintHits then
                print("[LUA][EnemyHealth] Died - destroying entity")
            end

            -- Disable model visibility
            if self.GetComponent then
                local model = self:GetComponent("ModelRenderComponent")
                if model then
                    if model.SetVisible then
                        model:SetVisible(false)
                    else
                        model.isVisible = false
                    end
                end
            end

            -- Disable collider & rigidbody
            local collider = self:GetComponent("ColliderComponent")
            if collider then
                if collider.SetEnabled then collider:SetEnabled(false)
                elseif collider.enabled ~= nil then collider.enabled = false end
            end

            local rb = self:GetComponent("RigidBodyComponent")
            if rb then
                if rb.SetEnabled then rb:SetEnabled(false)
                elseif rb.enabled ~= nil then rb.enabled = false end
            end

            if self.Destroy then
                self:Destroy()
            end
        end
    end,
}
