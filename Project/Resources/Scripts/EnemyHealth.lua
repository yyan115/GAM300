-- EnemyHealth.lua
require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

local function sqr(x) return x * x end

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
        self._lastHitId    = nil   -- to avoid double-damage from same swing
        self._isDead       = false -- stop reacting after death

        if event_bus and event_bus.subscribe then
            self._sub = event_bus.subscribe("player_attack_hitbox", function(payload)
                self:_onHitbox(payload)
            end)
        else
            print("[LUA][EnemyHealth] WARNING: event_bus not available; will never get hitbox events")
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe and self._sub then
            event_bus.unsubscribe(self._sub)
            self._sub = nil
        end
    end,

    _onHitbox = function(self, payload)
        if not payload then return end
        if not self.GetPosition then return end
        if self._isDead then return end

        -- Optional per-attack hit id
        local hitId = payload.hitId or payload.attackId or payload.stepId
        if hitId ~= nil and self._lastHitId == hitId then
            -- We already took damage from this exact swing
            return
        end

        ----------------------------------------------------------------
        -- 1) Enemy position
        ----------------------------------------------------------------
        local ex, ey, ez = self:GetPosition()

        -- *** NEW: bail out if position is invalid ***
        if ex == nil or ey == nil or ez == nil then
            if self.debugPrintHits then
                print("[LUA][EnemyHealth] WARNING: GetPosition() returned nil; likely no Transform on this entity")
            end
            return
        end

        ----------------------------------------------------------------
        -- 2) Decode hitbox center from payload (defensive)
        ----------------------------------------------------------------
        local center = payload.center or payload.hitboxCenter or payload

        local cx = (center and (center.x or center[1])) or payload.x or payload[1] or 0.0
        local cy = (center and (center.y or center[2])) or payload.y or payload[2] or 0.0
        local cz = (center and (center.z or center[3])) or payload.z or payload[3] or 0.0

        ----------------------------------------------------------------
        -- 3) Radius + damage
        ----------------------------------------------------------------
        local r     = payload.radius or payload.hitboxRadius or 1.0
        local dmg   = payload.damage or 1

        ----------------------------------------------------------------
        -- 4) Simple sphere hit check
        ----------------------------------------------------------------
        local dx = ex - cx
        local dy = ey - cy
        local dz = ez - cz
        local distSq = sqr(dx) + sqr(dy) + sqr(dz)

        if distSq > r * r then
            -- outside hitbox
            return
        end

        -- Mark that this enemy has already been hit by this swing
        if hitId ~= nil then
            self._lastHitId = hitId
        end

        ----------------------------------------------------------------
        -- 5) Apply damage
        ----------------------------------------------------------------
        self.health = self.health - dmg

        if self.debugPrintHits then
            print(string.format(
                "[LUA][EnemyHealth] Took %d damage from player. HP = %d",
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
                    -- Prefer method, if you expose SetVisible later:
                    if model.SetVisible then
                        model:SetVisible(false)
                    else
                        model.isVisible = false -- direct field fallback
                    end
                end
            end

            -- Disable collider & rigidbody (if needed)
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
