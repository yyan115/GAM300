require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local Input = _G.Input

-- Animation States
local HurtTrigger = "Hurt"

local function PlayerTakeDmg(self, dmg)
    if self.CurrentHealth <= 0 then return end
    
    self._animator:SetTrigger(HurtTrigger)
    self._hurtTriggered = true

    self.CurrentHealth = self.CurrentHealth - dmg
    
    if self.CurrentHealth <= 0 then
        self.CurrentHealth = 0
        self._animator:SetBool("IsDead", true)
        event_bus.publish("playerDead", true)
    end

    if event_bus and event_bus.publish then
        event_bus.publish("playerMaxhealth", self.MaxHealth)
        event_bus.publish("playerCurrentHealth", self.CurrentHealth)
    end

    self._knifeHitPlayer = false
end

local function clamp(v, a, b)
    if v < a then return a end
    if v > b then return b end
    return v
end

local function playerCellFromXZ(px, pz, cx, cz, step)
    step = step or 4.0
    cx = cx or 0.0
    cz = cz or 0.0

    local ix = math.floor((px - cx)/step + 0.5)
    local iz = math.floor((pz - cz)/step + 0.5)
    ix = clamp(ix, -1, 1)
    iz = clamp(iz, -1, 1)

    local map = {
        ["-1,-1"]=1, ["0,-1"]=2, ["1,-1"]=3,
        ["-1,0"]=4,  ["0,0"]=5,  ["1,0"]=6,
        ["-1,1"]=7,  ["0,1"]=8,  ["1,1"]=9,
    }
    return map[tostring(ix)..","..tostring(iz)] or 5
end

-- ============================================================================
-- Dodge i-frame helper
-- Called at every damage entry point BEFORE PlayerTakeDmg.
-- Returns true if the hit was dodged (damage blocked, dodge_success published).
-- attackType: string identifying the attack for dodge_success consumers.
-- payload: original event payload (forwarded so consumers can react to specifics).
-- ============================================================================
local function checkDodge(self, attackType, payload)
    if not self._isDashIFrame then return false end

    if event_bus and event_bus.publish then
        event_bus.publish("dodge_success", {
            attackType = attackType,
            payload    = payload,
        })
    end
    print(string.format("[PlayerHealth] Dodge! Blocked '%s' during dash i-frame", attackType))
    return true
end

return Component {
    mixins = { TransformMixin },

    fields = {
        MaxHealth      = 30,
        CurrentHealth  = 30,
        IFrameDuration = 1.0,    -- Post-hit i-frame duration (seconds).
        YDeathThreshold = -3.0,
        -- Must match PlayerMovement's DashDuration so the i-frame window is accurate.
        -- TO CHANGE dash duration: update both this and DashDuration in PlayerMovement.
        DashIFrameDuration = 0.7,
    },

    Awake = function(self)
        self._iFrameDuration = self.IFrameDuration
        self._animator  = self:GetComponent("AnimationComponent")
        self._transform = self:GetComponent("Transform")
        self.CurrentHealth = self.MaxHealth
        self._yDeathThreshold = self.YDeathThreshold
        self._playerDeathTriggered = false

        -- Dash i-frame state (separate from post-hit i-frame)
        self._isDashIFrame     = false
        self._dashIFrameTimer  = 0

        if event_bus and event_bus.subscribe then

            -- ── Dash i-frame ─────────────────────────────────────────────────
            -- Starts the moment a dash fires. Lasts exactly DashIFrameDuration.
            -- Separate from _isIFrame so a post-hit i-frame doesn't block dodge
            -- detection, and a dodge doesn't reset the post-hit i-frame timer.
            self._dashPerformedSub = event_bus.subscribe("dash_performed", function()
                self._isDashIFrame    = true
                self._dashIFrameTimer = self.DashIFrameDuration or 0.7
            end)

            -- ── Damage subscriptions ──────────────────────────────────────────
            -- Every handler checks dodge first, then post-hit i-frame, then deals damage.

            self._knifeHitPlayerDmgSub = event_bus.subscribe("knifeHitPlayerDmg", function(dmg)
                if not dmg then return end
                if checkDodge(self, "knife", { dmg = dmg }) then return end
                if self._isIFrame then return end
                PlayerTakeDmg(self, dmg)
                self._isIFrame = true
            end)

            self._meleeHitPlayerDmgSub = event_bus.subscribe("meleeHitPlayerDmg", function(payload)
                if not payload then return end
                if checkDodge(self, "melee", payload) then return end
                if self._isIFrame then return end
                local dmg = type(payload) == "table" and payload.dmg or payload
                if dmg then
                    PlayerTakeDmg(self, dmg)
                    self._isIFrame = true
                end
            end)

            self._minibossSlashSub = event_bus.subscribe("miniboss_slash", function(payload)
                if not payload then return end
                if checkDodge(self, "miniboss_slash", payload) then return end
                if self._isIFrame then return end

                local px, py, pz = self:GetPosition()
                if not px then return end

                local dx = px - (payload.x or 0)
                local dz = pz - (payload.z or 0)
                local r  = payload.radius or 1.4

                if (dx*dx + dz*dz) <= (r*r) then
                    PlayerTakeDmg(self, payload.dmg or 1)

                    if event_bus and event_bus.publish then
                        event_bus.publish("miniboss_melee_hit_confirmed", { entityId = payload.entityId })
                    end

                    local strength = payload.kbStrength or 0
                    if strength > 0 and event_bus and event_bus.publish then
                        local kbx, kbz = dx, dz
                        local len = math.sqrt(kbx*kbx + kbz*kbz)
                        if len > 1e-4 then kbx = kbx/len; kbz = kbz/len
                        else kbx, kbz = 0, 1 end
                        event_bus.publish("player_knockback", { x=kbx, z=kbz, strength=strength })
                    end

                    self._isIFrame = true
                end
            end)

            self._bossShoutAoeSub = event_bus.subscribe("boss_shout_aoe", function(payload)
                if not payload then return end
                if checkDodge(self, "boss_shout_aoe", payload) then return end
                if self._isIFrame then return end

                local px, py, pz = self:GetPosition()
                if not px then return end

                local dx = px - (payload.x or 0)
                local dz = pz - (payload.z or 0)
                local r  = payload.radius or 5.5

                if (dx*dx + dz*dz) <= (r*r) then
                    PlayerTakeDmg(self, payload.dmg or 1)
                    self._isIFrame = true

                    local strength = payload.kb or 0
                    if strength > 0 and event_bus and event_bus.publish then
                        local kbx, kbz = dx, dz
                        local len = math.sqrt(kbx*kbx + kbz*kbz)
                        if len > 1e-4 then kbx = kbx/len; kbz = kbz/len
                        else kbx, kbz = 0, 1 end
                        event_bus.publish("player_knockback", {
                            x=kbx, z=kbz, strength=strength,
                            src="boss_shout_aoe", enemyEntityId=payload.entityId,
                        })
                    end
                end
            end)

            self._bossRainExplosivesSub = event_bus.subscribe("boss_rain_explosives", function(payload)
                if not payload then return end
                if self.CurrentHealth <= 0 then return end
                if checkDodge(self, "boss_rain_explosives", payload) then return end
                if self._isIFrame then return end

                local px, py, pz = self:GetPosition()
                if not px then return end

                local cell = playerCellFromXZ(px, pz, payload.cx or 0, payload.cz or 0, payload.step or 4.0)
                local cells = payload.cells or {}
                local onDanger = false
                for i = 1, #cells do
                    if cells[i] == cell then onDanger = true; break end
                end

                if onDanger then
                    PlayerTakeDmg(self, payload.dmg or 1)
                    self._isIFrame = true
                end
            end)

            self._bossDiveImpactSub = event_bus.subscribe("boss_dive_impact", function(payload)
                if not payload then return end
                if checkDodge(self, "boss_dive_impact", payload) then return end
                if self._isIFrame then return end

                local px, py, pz = self:GetPosition()
                if not px then return end

                local dx = px - (payload.x or 0)
                local dz = pz - (payload.z or 0)
                local r  = payload.radius or 1.4

                if (dx*dx + dz*dz) <= (r*r) then
                    PlayerTakeDmg(self, payload.dmg or 1)
                    self._isIFrame = true
                end
            end)

            self._playerHealSub = event_bus.subscribe("playerHeal", function(amount)
                if not amount or self.CurrentHealth <= 0 then return end
                self.CurrentHealth = math.min(self.CurrentHealth + amount, self.MaxHealth)
                if event_bus and event_bus.publish then
                    event_bus.publish("playerMaxhealth", self.MaxHealth)
                    event_bus.publish("playerCurrentHealth", self.CurrentHealth)
                end
            end)

            self._respawnPlayerSub = event_bus.subscribe("respawnPlayer", function(respawn)
                if respawn then self._respawnPlayer = true end
            end)

            -- TO ADD new attack type: subscribe here, call checkDodge first,
            -- then _isIFrame check, then PlayerTakeDmg. Follow the pattern above.

        else
            print("[PlayerHealth] ERROR: event_bus not available!")
        end
    end,

    Start = function(self)
        if event_bus and event_bus.publish then
            event_bus.publish("playerMaxhealth", self.MaxHealth)
            event_bus.publish("playerCurrentHealth", self.CurrentHealth)
        end
    end,

    RespawnPlayer = function(self)
        self.CurrentHealth = self.MaxHealth
        self._isDashIFrame    = false
        self._dashIFrameTimer = 0
        self._isIFrame        = false
        self._iFrameDuration  = self.IFrameDuration
        if event_bus and event_bus.publish then
            event_bus.publish("playerMaxhealth", self.MaxHealth)
            event_bus.publish("playerCurrentHealth", self.CurrentHealth)
        end
        self._respawnPlayer        = false
        self._playerDeathTriggered = false
    end,

    Update = function(self, dt)
        if self._respawnPlayer then
            self:RespawnPlayer()
            return
        end

        if self._hurtTriggered then
            if event_bus and event_bus.publish then
                event_bus.publish("playerHurtTriggered", true)
                self._hurtTriggered = false
            end
        end

        -- Post-hit i-frame timer
        if self._isIFrame then
            self._iFrameDuration = self._iFrameDuration - dt
            if self._iFrameDuration <= 0 then
                self._iFrameDuration = self.IFrameDuration
                self._isIFrame = false
            end
        end

        -- Dash i-frame timer (independent of post-hit i-frame)
        if self._isDashIFrame then
            self._dashIFrameTimer = self._dashIFrameTimer - dt
            if self._dashIFrameTimer <= 0 then
                self._isDashIFrame = false
            end
        end

        -- Fall out of map → death
        if self._transform then
            local y = self._transform.worldPosition.y
            if y <= self._yDeathThreshold and not self._playerDeathTriggered then
                self.CurrentHealth = 0
                self._animator:SetBool("IsDead", true)
                event_bus.publish("playerDead", true)
                event_bus.publish("playerMaxhealth", self.MaxHealth)
                event_bus.publish("playerCurrentHealth", self.CurrentHealth)
                self._playerDeathTriggered = true
            end
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe then
            local subs = {
                "_dashPerformedSub", "_knifeHitPlayerDmgSub", "_meleeHitPlayerDmgSub",
                "_minibossSlashSub", "_bossShoutAoeSub", "_bossRainExplosivesSub",
                "_bossDiveImpactSub", "_playerHealSub", "_respawnPlayerSub",
            }
            for _, key in ipairs(subs) do
                if self[key] then event_bus.unsubscribe(self[key]); self[key] = nil end
            end
        end
        self._isDashIFrame = false
        self._isIFrame     = false
    end,
}