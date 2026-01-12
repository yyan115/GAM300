-- Scripts/AI/EnemyAI.lua
require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local StateMachine = require("Gameplay.StateMachine")
local GroundIdleState    = require("Gameplay.GroundIdleState")
local GroundAttackState  = require("Gameplay.GroundAttackState")
local GroundDeathState   = require("Gameplay.GroundDeathState")

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
        MaxHealth = 5,

        -- Config
        DetectionRange = 3.0,
        AttackCooldown = 1.0,

        -- Clip indices
        ClipIdle   = 0,
        ClipAttack = 1,
        ClipHurt   = 2,
        ClipDeath  = 3,

        PlayerName = "Player",
    },

    Awake = function(self)
        self.dead = false
        self.health = self.MaxHealth

        self.fsm = StateMachine.new(self)

        self.states = {
            Idle   = GroundIdleState,
            Attack = GroundAttackState,
            Death  = GroundDeathState,
            -- Hurt/Hooked/Patrol later
        }

        self.clips = {
            Idle   = self.ClipIdle,
            Attack = self.ClipAttack,
            Hurt   = self.ClipHurt,
            Death  = self.ClipDeath,
        }

        self.config = {
            DetectionRange = self.DetectionRange,
            AttackCooldown = self.AttackCooldown,
        }
    end,

    Start = function(self)
        self._animator = self:GetComponent("AnimationComponent")
        self._audio    = self:GetComponent("AudioComponent")
        self._collider = self:GetComponent("ColliderComponent")

        self._playerTr = Engine.FindTransformByName(self.PlayerName)

        self.fsm:Change("Idle", self.states.Idle)
    end,

    Update = function(self, dt)
        if self.dead then
            self.fsm:Update(dt) -- DeathState might handle despawn timing
            return
        end

        -- Hard rule: if health hits 0, go death (any state)
        if self.health <= 0 then
            self.fsm:Change("Death", self.states.Death)
            return
        end

        self.fsm:Update(dt)
    end,

    -- ========= Common helpers states call =========

    PlayClip = function(self, clipIndex, loop)
        if self._animator then
            self._animator:PlayClip(clipIndex, loop and true or false)
        end
    end,

    IsPlayerInRange = function(self, range)
        if not self._playerTr then return false end

        local pp = Engine.GetTransformPosition(self._playerTr)
        local px, pz = pp[1], pp[3]

        local ex, _, ez = self:GetPosition()
        local dx, dz = (px - ex), (pz - ez)
        return (dx*dx + dz*dz) <= (range * range)
    end,

    FacePlayer = function(self)
        if not self._playerTr then return end
        local pp = Engine.GetTransformPosition(self._playerTr)
        local px, pz = pp[1], pp[3]

        local ex, _, ez = self:GetPosition()
        local dx, dz = px - ex, pz - ez

        local yaw = math.deg(atan2(dx, dz))
        local q = eulerToQuat(0, yaw, 0)
        self:SetRotation(q.w, q.x, q.y, q.z)
    end,

    DoAttack = function(self)
        -- This is the single “attack action hook”.
        -- Later: enable hitbox for N frames, spawn knife, emit event, etc.
        -- For now: just placeholder print
        -- print("[EnemyAI] Attack triggered")
    end,

    SpawnKnife = function(self)
        local knife = KnifePool.Request()
        if not knife then return end

        local ex, ey, ez = self:GetPosition()
        if not self._playerTr then return end

        local pp = Engine.GetTransformPosition(self._playerTr)
        local px, py, pz = pp[1], pp[2] + 0.5, pp[3]

        local spawnX = ex - 0.5
        local spawnY = ey + 1.0
        local spawnZ = ez

        knife:Launch(spawnX, spawnY, spawnZ, px, py, pz)
    end,

    DisableCombat = function(self)
        -- One place to turn off hurtbox/hitbox/collider etc.
        -- if self._collider then self._collider.enabled = false end
    end,

    -- ========= Combat entry point =========
    ApplyHit = function(self, dmg, hitType)
        if self.dead then return end
        self.health = self.health - (dmg or 1)
        if self.health <= 0 then
            self.health = 0
            self.fsm:Change("Death", self.states.Death)
        else
            -- Priority 3: route to Hurt/Hooked state here later
            -- self.fsm:Change("Hurt", self.states.Hurt)
        end
    end,
}
