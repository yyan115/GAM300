-- Resources/Scripts/Gameplay/AttackHitbox.lua
-- Attach to the weapon collider entity (child of weapon bone).
-- Requires: RigidBodyComponent (isKinematic=true, isTrigger=true) + ColliderComponent
--
-- The RigidBody is disabled by default and only enabled during the active hit window.
-- This guarantees OnTriggerEnter fires fresh every swing, even if the enemy is
-- already inside the weapon collider (since removing + re-adding the body resets contact state).
--
-- For LIFT / AIR / SLAM hit types only: the collider is temporarily expanded at the
-- start of the hit window, then snapped back to its baseline after ExpandDuration
-- seconds (intended to be ~half the attack window). All other hit types use the
-- collider as-is with no expansion.
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

local JUGGLE_TYPES = { LIFT = true, AIR = true, SLAM = true }

return Component {
    fields = {
        -- Seconds the collider stays expanded before snapping back to normal.
        -- Set this to roughly half your lift/air/slam attack animation window.
        ExpandDuration        = 0.500,
        -- How much to add to capsule/sphere/cylinder radius during the expand window.
        ActiveRadiusBonus     = 2.400,
        -- How much to add to capsule/cylinder half-height during the expand window.
        ActiveHalfHeightBonus = 1.600,
    },

    Awake = function(self)
        self._active = false
        self._hitThisSwing     = {}
        self._currentDamage    = 0
        self._currentKnockback = 0
        self._subscribed       = false
        self._playerEntityId   = nil
        self._rb               = nil
        self._collider         = nil
        self._currentHitType   = "COMBO"

        -- Expand timer state.
        self._expandTimer = 0
        self._isExpanded  = false

        -- Saved baseline collider dimensions (read once in Start).
        self._baseCapR  = nil
        self._baseCapHH = nil
        self._baseSphR  = nil
        self._baseCylR  = nil
        self._baseCylHH = nil
    end,

    Start = function(self)
        if self._subscribed then return end
        self._subscribed = true

        -- Cache RigidBody and disable it — weapon has no physics presence until attacking.
        self._rb = self:GetComponent("RigidBodyComponent")
        if self._rb then
            self._rb:SetEnabled(false)
        end

        -- Cache collider and snapshot baseline dimensions so we can always restore them.
        self._collider = self:GetComponent("ColliderComponent")
        if self._collider then
            pcall(function()
                self._baseCapR  = self._collider.capsuleRadius
                self._baseCapHH = self._collider.capsuleHalfHeight
                self._baseSphR  = self._collider.sphereRadius
                self._baseCylR  = self._collider.cylinderRadius
                self._baseCylHH = self._collider.cylinderHalfHeight
            end)
        end

        if Engine and Engine.GetEntityByName then
            self._playerEntityId = Engine.GetEntityByName("Player")
        end

        if event_bus and event_bus.subscribe then
            -- Hit window opens.
            self._subAttack = event_bus.subscribe("attack_performed", function(data)
                self._active       = true
                self._hitThisSwing = {}
                self._currentDamage    = (data and data.damage)    or 10
                self._currentKnockback = (data and data.knockback) or 0

                if data and data.isSlam then
                    self._currentHitType = "SLAM"
                elseif data and (data.state == "lift_attack") then
                    self._currentHitType = "LIFT"
                elseif data and data.isAerial then
                    self._currentHitType = "AIR"
                else
                    self._currentHitType = "COMBO"
                end

                -- Only expand for juggle hit types (LIFT / AIR / SLAM).
                if JUGGLE_TYPES[self._currentHitType] then
                    self:_expandCollider()
                    self._expandTimer = 0
                    self._isExpanded  = true
                end

                print(string.format("[AttackHitbox] HIT WINDOW OPEN: hitType=%s damage=%d knockback=%.1f",
                    self._currentHitType, self._currentDamage, self._currentKnockback))

                if self._rb then self._rb:SetEnabled(true) end
            end)

            -- Hit window closes: disable RB first (destroys physics body), then
            -- restore collider dims so the next RB enable picks up baseline shape.
            self._subState = event_bus.subscribe("combat_state_changed", function(data)
                self._active = false
                if self._rb then self._rb:SetEnabled(false) end
                if self._isExpanded then
                    self:_restoreCollider()
                    self._isExpanded  = false
                    self._expandTimer = 0
                end
            end)
        end
    end,

    Update = function(self, dt)
        if not self._isExpanded then return end

        local dtSec = dt or 0
        if dtSec > 1.0 then dtSec = dtSec * 0.001 end  -- ms → s guard

        self._expandTimer = self._expandTimer + dtSec

        -- Collapse back to baseline after ExpandDuration seconds.
        -- Disable RB first so the physics body is destroyed before dims are reset,
        -- then re-enable so the next overlap uses the baseline shape.
        if self._expandTimer >= (tonumber(self.ExpandDuration) or 0.15) then
            if self._rb then self._rb:SetEnabled(false) end
            self:_restoreCollider()
            self._isExpanded  = false
            self._expandTimer = 0
            if self._active and self._rb then self._rb:SetEnabled(true) end
        end
    end,

    OnDisable = function(self)
        self._active = false
        if self._isExpanded then
            self:_restoreCollider()
            self._isExpanded  = false
            self._expandTimer = 0
        end
        if self._rb then self._rb:SetEnabled(false) end

        if event_bus and event_bus.unsubscribe then
            if self._subAttack then event_bus.unsubscribe(self._subAttack) end
            if self._subState  then event_bus.unsubscribe(self._subState)  end
        end
        self._subAttack  = nil
        self._subState   = nil
        self._subscribed = false
    end,

    -- Expand capsule / sphere / cylinder by the Active*Bonus fields.
    -- Values are written BEFORE the RigidBody is enabled. The RB enable creates
    -- a fresh physics body that reads the current collider dimensions at that moment,
    -- so no collider toggle is needed — toggling it while the RB is off causes
    -- the trigger to stop firing entirely.
    _expandCollider = function(self)
        if not self._collider then return end
        local rBonus  = tonumber(self.ActiveRadiusBonus)     or 0
        local hhBonus = tonumber(self.ActiveHalfHeightBonus) or 0
        pcall(function()
            if self._baseCapR  ~= nil then self._collider.capsuleRadius      = self._baseCapR  + rBonus  end
            if self._baseCapHH ~= nil then self._collider.capsuleHalfHeight  = self._baseCapHH + hhBonus end
            if self._baseSphR  ~= nil then self._collider.sphereRadius       = self._baseSphR  + rBonus  end
            if self._baseCylR  ~= nil then self._collider.cylinderRadius     = self._baseCylR  + rBonus  end
            if self._baseCylHH ~= nil then self._collider.cylinderHalfHeight = self._baseCylHH + hhBonus end
        end)
    end,

    -- Restore all collider dimensions to the baseline snapshotted in Start.
    -- Called after the RB is disabled, so the next RB enable picks up baseline dims.
    _restoreCollider = function(self)
        if not self._collider then return end
        pcall(function()
            if self._baseCapR  ~= nil then self._collider.capsuleRadius      = self._baseCapR  end
            if self._baseCapHH ~= nil then self._collider.capsuleHalfHeight  = self._baseCapHH end
            if self._baseSphR  ~= nil then self._collider.sphereRadius       = self._baseSphR  end
            if self._baseCylR  ~= nil then self._collider.cylinderRadius     = self._baseCylR  end
            if self._baseCylHH ~= nil then self._collider.cylinderHalfHeight = self._baseCylHH end
        end)
    end,

    -- Walk up the hierarchy to find the root entity.
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

        print(string.format("[AttackHitbox] OnTriggerEnter: targetId=%s hitType=%s active=%s",
            tostring(targetId), tostring(self._currentHitType), tostring(self._active)))

        if self._playerEntityId and targetId == self._playerEntityId then return end
        if self._hitThisSwing[targetId] then return end
        self._hitThisSwing[targetId] = true

        if event_bus and event_bus.publish then
            event_bus.publish("deal_damage_to_entity", {
                entityId  = targetId,
                damage    = self._currentDamage,
                hitType   = self._currentHitType or "COMBO",
                knockback = self._currentKnockback or 0,
            })
            print(string.format("[AttackHitbox] deal_damage_to_entity published: entity=%s hitType=%s dmg=%d knockback=%.1f",
                tostring(targetId), tostring(self._currentHitType), self._currentDamage, self._currentKnockback or 0))
        end
    end,
}