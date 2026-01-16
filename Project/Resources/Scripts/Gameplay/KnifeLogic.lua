-- Resources/Scripts/Gameplay/KnifeLogic.lua
require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local KnifePool = require("Gameplay.KnifePool")

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

return Component {
    mixins = { TransformMixin },

    fields = {
        Speed    = 10.0,
        Lifetime = 1.2,
        Damage   = 1,
    },

    Start = function(self)
        self.model    = self:GetComponent("ModelRenderComponent")
        self.collider = self:GetComponent("ColliderComponent")
        self.rb       = self:GetComponent("RigidBodyComponent") -- if present

        self.active = false
        self.age = 0
        self.dirX, self.dirY, self.dirZ = 0,0,0

        if self.model then
            self.model.isVisible = false
        end

        -- HARD: disable physics push
        if self.collider then
            self.collider.enabled = false
        end
        if self.rb then
            self.rb.enabled = false         -- strongest "no push"
            self.rb.isTrigger = true        -- harmless even if enabled later
        end

        KnifePool.Register(self)
    end,

    Update = function(self, dt)
        if not self.active then return end

        self.age = self.age + dt
        if self.age >= self.Lifetime then
            self:Reset()
            return
        end

        self:Move(self.dirX * self.Speed * dt,
                  self.dirY * self.Speed * dt,
                  self.dirZ * self.Speed * dt)
        
        -- print("[Knife]", self:GetPosition())
    end,

    Launch = function(self, spawnX, spawnY, spawnZ, targetX, targetY, targetZ)
        if self.active then return end

        self:SetPosition(spawnX, spawnY, spawnZ)

        local dx = targetX - spawnX
        local dy = targetY - spawnY
        local dz = targetZ - spawnZ

        local dist = math.sqrt(dx*dx + dy*dy + dz*dz)
        if dist < 0.001 then dist = 1 end

        self.dirX = dx / dist
        self.dirY = dy / dist
        self.dirZ = dz / dist

        -- Face travel direction (XZ)
        local yaw = math.deg(atan2(self.dirX, self.dirZ))
        local q = eulerToQuat(0, yaw, 0)
        self:SetRotation(q.w, q.x, q.y, q.z)

        self.active = true
        self.age = 0

        if self.model then
            ModelRenderComponent.SetVisible(self.model, true)
        end

        -- IMPORTANT: keep these off while diagnosing shove
        if self.collider then self.collider.enabled = false end
        if self.rb then self.rb.enabled = false end
    end,

    Reset = function(self)
        self.active = false
        self.age = 0
        self.dirX, self.dirY, self.dirZ = 0,0,0
        if self.model then
            ModelRenderComponent.SetVisible(self.model, false)
        end

        if self.collider then self.collider.enabled = false end
        if self.rb then self.rb.enabled = false end
    end,
}
