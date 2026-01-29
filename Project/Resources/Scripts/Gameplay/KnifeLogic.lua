-- Resources/Scripts/GamePlay/KnifeLogic.lua
require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local KnifePool = require("Gameplay.KnifePool")
local event_bus = _G.event_bus

local function atan2(y, x)
    local ok, v = pcall(math.atan, y, x) -- 2-arg if supported
    if ok and type(v) == "number" then return v end
    if x > 0 then return math.atan(y / x) end
    if x < 0 and y >= 0 then return math.atan(y / x) + math.pi end
    if x < 0 and y < 0 then return math.atan(y / x) - math.pi end
    if x == 0 and y > 0 then return math.pi / 2 end
    if x == 0 and y < 0 then return -math.pi / 2 end
    return 0
end

local function eulerToQuat(pitch, yaw, roll)
    local p = math.rad(pitch or 0) * 0.5
    local y = math.rad(yaw or 0)   * 0.5
    local r = math.rad(roll or 0)  * 0.5

    local sinP, cosP = math.sin(p), math.cos(p)
    local sinY, cosY = math.sin(y), math.cos(y)
    local sinR, cosR = math.sin(r), math.cos(r)

    return {
        w = cosP * cosY * cosR + sinP * sinY * sinR,
        x = sinP * cosY * cosR - cosP * sinY * sinR,
        y = cosP * sinY * cosR + sinP * cosY * sinR,
        z = cosP * cosY * sinR - sinP * sinY * cosR
    }
end

-- Convert dt to seconds (handles engines that pass ms), clamp to avoid huge leaps
local function toDtSec(dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end -- likely ms
    if dtSec <= 0 then return 0 end
    if dtSec > 0.05 then dtSec = 0.05 end -- clamp for stability
    return dtSec
end

local function randRange(minv, maxv)
    return minv + (maxv - minv) * math.random()
end

return Component {
    mixins = { TransformMixin },

    fields = {
        Speed    = 8.0,
        Lifetime = 1.2,
        Damage   = 1,

        -- ===== HIT DETECTION TUNING =====
        HitRadius  = 0.50,
        ArmDelay   = 0.05,

        -- ===== DESYNC / POOL BEHAVIOR =====
        -- Makes knives expire at slightly different times so they don't all reset together.
        LifeJitter = 0.25, -- +/- seconds (0 disables)

        -- Resets knife after travelling this distance (0 disables). Helps "individual" recycle.
        MaxRange   = 10.0,
    },

    Start = function(self)
        self.model    = self:GetComponent("ModelRenderComponent")
        self.collider = self:GetComponent("ColliderComponent")
        self.rb       = self:GetComponent("RigidBodyComponent")

        self.active = false
        self.reserved = false
        self.age = 0
        self.dirX, self.dirY, self.dirZ = 0,0,0

        -- per-launch lifetime + travel tracking
        self._life = self.Lifetime
        self._sx, self._sy, self._sz = 0,0,0

        self._playerPos = nil
        self._playerPosSub = nil

        if event_bus and event_bus.subscribe then
            self._playerPosSub = event_bus.subscribe("player_position", function(pos)
                if pos then
                    self._playerPos = pos
                end
            end)
        end

        if self.model then
            ModelRenderComponent.SetVisible(self.model, false)
        end

        if self.collider then
            self.collider.enabled = false
        end
        if self.rb then
            self.rb.enabled = false
        end

        KnifePool.Register(self)
    end,

    _GetPlayerPos = function(self)
        local p = self._playerPos
        if not p then return nil end

        if (type(p) == "userdata" or type(p) == "table") and p.x ~= nil then
            return { x = p.x, y = p.y, z = p.z }
        end

        return nil
    end,

    _CheckHitPlayer = function(self)
        if not self.active then return false end
        local p = self:_GetPlayerPos()
        if not p then return false end

        local kx, ky, kz = self:GetPosition()
        local dx, dy, dz = p.x - kx, p.y - ky, p.z - kz

        local r = self.HitRadius
        if (dx*dx + dy*dy + dz*dz) <= (r*r) then
            return true
        end
        return false
    end,

    Update = function(self, dt)
        if not self.active then return end

        dt = toDtSec(dt)
        if dt == 0 then return end

        self.age = self.age + dt
        if self.age >= (self._life or self.Lifetime) then
            self:Reset("LIFETIME")
            return
        end

        -- Move knife
        self:Move(self.dirX * self.Speed * dt,
                  self.dirY * self.Speed * dt,
                  self.dirZ * self.Speed * dt)

        -- Range-based recycle (prevents synchronized "batch" resets)
        local r = self.MaxRange or 0
        if r > 0 then
            local x,y,z = self:GetPosition()
            local dx = x - (self._sx or x)
            local dy = y - (self._sy or y)
            local dz = z - (self._sz or z)
            if (dx*dx + dy*dy + dz*dz) >= (r*r) then
                self:Reset("RANGE")
                return
            end
        end

        -- Arm + hit check
        self._armedTimer = (self._armedTimer or 0) + dt
        if self._armedTimer >= self.ArmDelay then
            if self:_CheckHitPlayer() then
                --print("[Knife] HIT PLAYER -> reset")
                self:Reset("HIT")
                return
            end
        end

        -- optional debug pacing (kept but still off)
        -- if self.active then
        --     self._dbg = (self._dbg or 0) + dt
        --     if self._dbg > 0.5 then
        --         self._dbg = 0
        --         local x,y,z = self:GetPosition()
        --         --print(string.format("[Knife] active at (%.2f, %.2f, %.2f)", x,y,z))
        --     end
        -- end
    end,

    Launch = function(self, spawnX, spawnY, spawnZ, targetX, targetY, targetZ, token, slot)
        -- already flying -> can't relaunch
        if self.active then
            -- debug
            -- print(string.format("[Knife] Launch FAIL (already active) slot=%s", tostring(slot)))
            return false
        end

        -- If we're reserved, we must match the token that reserved us.
        if self.reserved then
            if token == nil or self._reservedToken ~= token then
                -- debug
                -- print(string.format("[Knife] Launch FAIL (token mismatch) slot=%s selfTok=%s tok=%s",
                --     tostring(slot), tostring(self._reservedToken), tostring(token)))
                return false
            end
        end

        -- consume reservation
        self.reserved = false
        self._reservedToken = nil

        -- per-launch lifetime jitter (prevents batch resets)
        local life = self.Lifetime or 1.2
        local jitter = self.LifeJitter or 0
        if jitter > 0 then
            life = life + randRange(-jitter, jitter)
            if life < 0.05 then life = 0.05 end
        end
        self._life = life

        -- store spawn for range recycling
        self._sx, self._sy, self._sz = spawnX, spawnY, spawnZ

        self:SetPosition(spawnX, spawnY, spawnZ)

        -- print(string.format("[Knife] LAUNCH slot=%s tok=%s spawn=(%.2f, %.2f, %.2f) target=(%.2f, %.2f, %.2f)",
        --     tostring(slot), tostring(token),
        --     spawnX, spawnY, spawnZ, targetX, targetY, targetZ))

        local dx = targetX - spawnX
        local dy = targetY - spawnY
        local dz = targetZ - spawnZ

        local dist = math.sqrt(dx*dx + dy*dy + dz*dz)
        if dist < 0.001 then dist = 1 end

        self.dirX = dx / dist
        self.dirY = dy / dist
        self.dirZ = dz / dist

        local yaw = math.deg(atan2(self.dirX, self.dirZ))
        local q = eulerToQuat(0, yaw, 0)
        self:SetRotation(q.w, q.x, q.y, q.z)

        self.active = true
        self.age = 0
        self._armedTimer = 0

        if self.model then
            ModelRenderComponent.SetVisible(self.model, true)
            local v = self.model.isVisible
            --print(string.format("[Knife] VIS slot=%s tok=%s isVisible=%s", tostring(slot), tostring(token), tostring(v)))
        end
        if self.collider then self.collider.enabled = false end

        return true
    end,

    Reset = function(self, reason)
        if self.active then
            --print(string.format("[Knife] RESET reason=%s", tostring(reason)))
        end
        self.active = false
        self.reserved = false
        self._reservedToken = nil

        self.age = 0
        self.dirX, self.dirY, self.dirZ = 0,0,0
        self._armedTimer = 0

        if self.model then
            ModelRenderComponent.SetVisible(self.model, false)
        end
        if self.collider then self.collider.enabled = false end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe and self._playerPosSub then
            event_bus.unsubscribe(self._playerPosSub)
            self._playerPosSub = nil
        end
    end,
}