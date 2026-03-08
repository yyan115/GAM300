require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local Input = _G.Input

-- Animation States
local HurtTrigger = "Hurt"

local function PlayerTakeDmg(self, dmg)
    if self.CurrentHealth <= 0 then return end
    
    --print("[PlayerTakeDmg] Animator set trigger Hurt")
    self._animator:SetTrigger(HurtTrigger)
    self._hurtTriggered = true

    self.CurrentHealth = self.CurrentHealth - dmg
    --print(string.format("[PlayerTakeDmg] Player took %d damage. Remaining health: %d", dmg, self.CurrentHealth))
    
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

return Component {
    mixins = { TransformMixin },

    fields = {
        MaxHealth = 30,
        CurrentHealth = 30,
        IFrameDuration = 1.0,
        YDeathThreshold = -3.0,
    },

    Awake = function(self)
        self._iFrameDuration = self.IFrameDuration
        self._animator  = self:GetComponent("AnimationComponent")
        self._transform = self:GetComponent("Transform")
        self.CurrentHealth = self.MaxHealth
        self._yDeathThreshold = self.YDeathThreshold
        self._playerDeathTriggered = false

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
            print("[PlayerHealth] Subscription token: " .. tostring(self._knifeHitPlayerDmgSub))

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

                    -- Publish hit confirmation for MiniBoss to play hit SFX
                    if event_bus and event_bus.publish then
                        event_bus.publish("miniboss_melee_hit_confirmed", { entityId = payload.entityId })
                    end

                    if event_bus and event_bus.publish then
                        local strength = payload.kbStrength or 0
                        if strength > 0 then
                            local kbx, kbz = dx, dz
                            local len = math.sqrt(kbx*kbx + kbz*kbz)
                            if len > 1e-4 then
                                kbx = kbx / len
                                kbz = kbz / len
                            else
                                kbx, kbz = 0, 1
                            end

                            event_bus.publish("player_knockback", {
                                x = kbx,
                                z = kbz,
                                strength = strength,
                            })
                        end
                    end
                    self._isIFrame = true
                    -- KNOCKBACK (NEW) - push player away from slash center
                    local strength = payload.kbStrength or 0
                    if strength > 0 then
                        local kbx, kbz = dx, dz
                        local len = math.sqrt(kbx*kbx + kbz*kbz)
                        if len > 1e-4 then
                            kbx = kbx / len
                            kbz = kbz / len
                        else
                            -- edge case: player exactly at center
                            kbx, kbz = 0, 1
                        end

                        -- If you have a CharacterController on player:
                        if self._controller then
                            CharacterController.Move(self._controller, kbx * strength, 0, kbz * strength)
                        else
                            -- fallback: if you have RB
                            local rb = self:GetComponent("RigidBodyComponent")
                            if rb then
                                local v = rb.linearVel or {x=0,y=0,z=0}
                                v.x = (v.x or 0) + kbx * strength
                                v.z = (v.z or 0) + kbz * strength
                                rb.linearVel = v
                            end
                        end
                    end
                end
            end)
            print("[PlayerHealth] Subscribed to miniboss_slash: " .. tostring(self._minibossSlashSub))

            print("[PlayerHealth] Subscribing to boss_shout_aoe")
            self._bossShoutAoeSub = event_bus.subscribe("boss_shout_aoe", function(payload)
                if not payload then return end
                if self._isIFrame then return end

                -- payload: x,y,z,radius,dmg,kb,entityId
                local px, py, pz = self:GetPosition()
                if not px then return end

                local dx = px - (payload.x or 0)
                local dz = pz - (payload.z or 0)
                local r  = payload.radius or 5.5

                -- inside AOE radius?
                if (dx*dx + dz*dz) <= (r*r) then
                    -- damage
                    PlayerTakeDmg(self, payload.dmg or 1)
                    self._isIFrame = true

                    -- knockback via your existing pipeline
                    local strength = payload.kb or 0
                    if strength > 0 and event_bus and event_bus.publish then
                        local kbx, kbz = dx, dz
                        local len = math.sqrt(kbx*kbx + kbz*kbz)
                        if len > 1e-4 then
                            kbx = kbx / len
                            kbz = kbz / len
                        else
                            kbx, kbz = 0, 1
                        end

                        event_bus.publish("player_knockback", {
                            x = kbx,
                            z = kbz,
                            strength = strength,
                            src = "boss_shout_aoe",
                            enemyEntityId = payload.entityId,
                        })
                    end
                end
            end)
            print("[PlayerHealth] Subscribed to boss_shout_aoe: " .. tostring(self._bossShoutAoeSub))

            print("[PlayerHealth] Subscribing to boss_rain_explosives (INSTANT)")
            self._bossRainExplosivesSub = event_bus.subscribe("boss_rain_explosives", function(payload)
                if not payload then return end
                if self.CurrentHealth <= 0 then return end
                if self._isIFrame then return end

                -- payload expected:
                -- cells (array), dmg, step, cx, cz
                local px, py, pz = self:GetPosition()
                if not px then return end

                local step = payload.step or 4.0
                local cx   = payload.cx or 0.0
                local cz   = payload.cz or 0.0

                local cell = playerCellFromXZ(px, pz, cx, cz, step)

                local cells = payload.cells or {}
                local onDanger = false
                for i = 1, #cells do
                    if cells[i] == cell then
                        onDanger = true
                        break
                    end
                end

                print(string.format(
                    "[PlayerHealth] rain_explosives INSTANT check: playerCell=%d dmg=%s onDanger=%s cellsCount=%d",
                    cell, tostring(payload.dmg), tostring(onDanger), #cells
                ))

                if onDanger then
                    PlayerTakeDmg(self, payload.dmg or 1)
                    self._isIFrame = true
                end
            end)
            print("[PlayerHealth] Subscribed to boss_rain_explosives: " .. tostring(self._bossRainExplosivesSub))

            print("[PlayerHealth] Subscribing to boss_dive_impact")
            self._bossDiveImpactSub = event_bus.subscribe("boss_dive_impact", function(payload)
                if not payload then return end
                if self._isIFrame then return end

                local px, py, pz = self:GetPosition()
                if not px then return end

                local dx = px - (payload.x or 0)
                local dz = pz - (payload.z or 0)
                local r  = (payload.radius or 1.4)

                if (dx*dx + dz*dz) <= (r*r) then
                    PlayerTakeDmg(self, payload.dmg or 1)
                    self._isIFrame = true
                end
            end)
            print("[PlayerHealth] Subscribed to boss_dive_impact: " .. tostring(self._bossDiveImpactSub))

            print("[PlayerHealth] Subscribing to playerHeal")
            self._playerHealSub = event_bus.subscribe("playerHeal", function(amount)
                if not amount or self.CurrentHealth <= 0 then return end
                self.CurrentHealth = math.min(self.CurrentHealth + amount, self.MaxHealth)
                if event_bus and event_bus.publish then
                    event_bus.publish("playerMaxhealth", self.MaxHealth)
                    event_bus.publish("playerCurrentHealth", self.CurrentHealth)
                end
            end)

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
            event_bus.publish("playerMaxhealth", self.MaxHealth)
            event_bus.publish("playerCurrentHealth", self.CurrentHealth)
        end
    end,

    RespawnPlayer = function(self)
        self.CurrentHealth = self.MaxHealth
        if event_bus and event_bus.publish then
            event_bus.publish("playerMaxhealth", self.MaxHealth)
            event_bus.publish("playerCurrentHealth", self.CurrentHealth)
        end

        self._respawnPlayer = false
        self._playerDeathTriggered = false
    end,

    Update = function(self, dt)
        if self._respawnPlayer then
            self.RespawnPlayer(self)
            return
        end

        if self._hurtTriggered then
            if event_bus and event_bus.publish then
                --print("[PlayerHealth] playerHurtTriggered published")
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

        -- Fall out of map -> death
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

    end,
}