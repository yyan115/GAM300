-- Resources/Scripts/Gameplay/EnemyAI.lua
require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local StateMachine       = require("Gameplay.StateMachine")
local GroundIdleState    = require("Gameplay.GroundIdleState")
local GroundAttackState  = require("Gameplay.GroundAttackState")
local GroundHurtState    = require("Gameplay.GroundHurtState")
local GroundDeathState   = require("Gameplay.GroundDeathState")
local GroundHookedState  = require("Gameplay.GroundHookedState")
local GroundPatrolState  = require("Gameplay.GroundPatrolState")
local GroundChaseState = require("Gameplay.GroundChaseState")

local KnifePool = require("Gameplay.KnifePool")
local Input = _G.Input
local Time  = _G.Time
local Physics = _G.Physics

local function atan2(y, x)
    local ok, v = pcall(math.atan, y, x)
    if ok and type(v) == "number" then return v end
    if x > 0 then return math.atan(y / x) end
    if x < 0 and y >= 0 then return math.atan(y / x) + math.pi end
    if x < 0 and y < 0 then return math.atan(y / x) - math.pi end
    if x == 0 and y > 0 then return math.pi / 2 end
    if x == 0 and y < 0 then return -math.pi / 2 end
    return 0
end

local function yawQuatFromDir(dx, dz)
    local yaw = atan2(dx, dz)       -- radians
    local half = yaw * 0.5
    return math.cos(half), 0, math.sin(half), 0  -- (w,x,y,z) yaw-only about Y
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

local function isDynamic(self)
    return self._rb and self._rb.motionID == 2
end

local function isKinematic(self)
    return self._rb and self._rb.motionID == 1
end

return Component {
    mixins = { TransformMixin },

    fields = {
        MaxHealth = 5,

        DetectionRange = 4.0,
        AttackRange          = 3.0,   -- actually allowed to shoot
        AttackEngageRange    = 3.5,   -- enter attack state
        AttackDisengageRange = 4.0,   -- exit attack state (slightly bigger)
        AttackCooldown = 1.0,
        HurtDuration   = 2.0,
        HitIFrame      = 0.2,
        HookedDuration = 4.0,

        EnablePatrol   = true,
        PatrolSpeed    = 0.3,
        PatrolDistance = 3.0,
        PatrolWait     = 1.5,
        ChaseSpeed     = 0.6,

        ClipIdle   = 0,
        ClipAttack = 1,
        ClipHurt   = 2,
        ClipDeath  = 3,

        PlayerName = "Player",

        -- === Kinematic grounding tuning ===
        UseKinematicGrounding = true,

        GroundRayUp    = 0.35,   -- cast origin is y + this
        GroundRayLen   = 3.0,    -- max ray distance downward
        GroundEps      = 0.08,   -- considered grounded if hit <= GroundRayUp + eps
        GroundSnapMax  = 0.35,   -- max snap correction per step

        Gravity        = -9.81,
        MaxFallSpeed   = -25.0,

        WallRayUp      = 0.8,    -- cast from chest height
        WallSkin   = 0.18,  -- your current probe padding
        WallOffset = 0.08,  -- NEW: how far to stop before the wall
        MinStep    = 0.01,  -- NEW: avoids tiny jitter moves
    },

    Awake = function(self)
        self.dead = false
        self.health = self.MaxHealth
        self._hitLockTimer = 0

        self.fsm = StateMachine.new(self)

        self.states = {
            Idle   = GroundIdleState,
            Attack = GroundAttackState,
            Hurt   = GroundHurtState,
            Death  = GroundDeathState,
            Hooked = GroundHookedState,
            Patrol = GroundPatrolState,
            Chase  = GroundChaseState,
        }

        self.clips = {
            Idle   = self.ClipIdle,
            Attack = self.ClipAttack,
            Hurt   = self.ClipHurt,
            Death  = self.ClipDeath,
        }

        self.config = {
            DetectionRange = self.DetectionRange,
            AttackRange          = self.AttackRange,
            AttackEngageRange    = self.AttackEngageRange,
            AttackDisengageRange = self.AttackDisengageRange,
            ChaseSpeed           = self.ChaseSpeed,
            AttackCooldown = self.AttackCooldown,
            HurtDuration   = self.HurtDuration,
            HitIFrame      = self.HitIFrame,
            HookedDuration = self.HookedDuration,
            PatrolSpeed    = self.PatrolSpeed,
            PatrolDistance = self.PatrolDistance,
            PatrolWait     = self.PatrolWait,
            EnablePatrol   = self.EnablePatrol,
        }

        -- fixed-step accumulator for kinematic grounding
        self._acc = 0
        self._vy = 0

        self._prevX = nil
        self._prevZ = nil
    end,

    Start = function(self)
        self._animator  = self:GetComponent("AnimationComponent")
        self._audio     = self:GetComponent("AudioComponent")
        self._collider  = self:GetComponent("ColliderComponent")
        self._transform = self:GetComponent("Transform")
        self._rb        = self:GetComponent("RigidBodyComponent")

        -- Create CC only if we have the right inputs
        if self._collider and self._transform then
            self._controller = CharacterController.Create(self.entityId, self._collider, self._transform)
        end

        if self._rb then
            pcall(function() self._rb.motionID = 1 end)
            -- DO NOT force gravityFactor = 0 for CC unless you are 100% sure CC has its own gravity
            pcall(function() self._rb.linearVel = { x=0, y=0, z=0 } end)
            pcall(function() self._rb.impulseApplied = { x=0, y=0, z=0 } end)
        end

        -- If you're using CC, do NOT run your old kinematic grounding system
        self.UseKinematicGrounding = false

        self._playerTr = Engine.FindTransformByName(self.PlayerName)

        local x, y, z = self:GetPosition()
        self._spawnX, self._spawnY, self._spawnZ = x, y, z

        local dist = self.config.PatrolDistance or self.PatrolDistance or 3.0
        self._patrolA = { x = x - dist, y = y, z = z }
        self._patrolB = { x = x + dist, y = y, z = z }
        self._patrolTarget = self._patrolB

        self.fsm:Change("Idle", self.states.Idle)
    end,

    Update = function(self, dt)
        if self.health <= 0 and not self.dead then
            self.fsm:Change("Death", self.states.Death)
        end

        -- Cache motion once rb is ready
        self._motionID = self._rb and self._rb.motionID or nil

        -- DEBUG print less spammy
        -- self._dbgT = (self._dbgT or 0) + dt
        -- if self._dbgT > 1.0 then
        --     self._dbgT = 0
        --     print("[EnemyRB] motionID=", tostring(self._motionID),
        --         "gravityFactor=", tostring(self._rb and self._rb.gravityFactor))
        -- end

        -- DEBUG (disabled for unified input - no debug keys mapped)
        -- if Input.IsActionJustPressed("DebugHit") then self:ApplyHit(1) end
        -- if Input.IsActionJustPressed("DebugHook") then self:ApplyHook(4.0) end

        self._hitLockTimer = math.max(0, (self._hitLockTimer or 0) - dt)

        if not self.fsm.current or not self.fsm.currentName then
            self.fsm:ForceChange("Idle", self.states.Idle)
        end

        -- Let FSM drive behaviour
        self.fsm:Update(dt)

        -- === CharacterController position sync (like PlayerMovement) ===
        if self._controller then
            local pos = CharacterController.GetPosition(self._controller)
            if pos then
                self:SetPosition(pos.x, pos.y, pos.z)
            end
        end
    end,

    GetRanges = function(self)
        local attackR = (self.config and self.config.AttackRange) or self.AttackRange or 3.0
        local diseng  = (self.config and self.config.AttackDisengageRange) or self.AttackDisengageRange or 4.0

        -- safety: enforce diseng >= attack
        if diseng < attackR then diseng = attackR + 0.25 end
        return attackR, diseng
    end,

    GetPlayerDistanceSq = function(self)
        local tr = self._playerTr
        if not tr then
            tr = Engine.FindTransformByName(self.PlayerName)
            self._playerTr = tr
        end
        if not tr then return math.huge end

        local pp = Engine.GetTransformPosition(tr)
        local px, pz = pp[1], pp[3]

        -- enemy position MUST be current (controller-based if available)
        local ex, ez
        if self.GetEnemyPosXZ then
            ex, ez = self:GetEnemyPosXZ()
        else
            local x, _, z = self:GetPosition()
            ex, ez = x, z
        end

        local dx, dz = px - ex, pz - ez
        return dx*dx + dz*dz
    end,

    MoveCC = function(self, vx, vz)
        if not self._controller then
            print("[EnemyAI] MoveCC called but controller is NIL")
            return
        end

        local before = CharacterController.GetPosition(self._controller)
        CharacterController.Move(self._controller, vx or 0, 0, vz or 0)
        local after = CharacterController.GetPosition(self._controller)

        -- sync transform from CC
        if after then
            self:SetPosition(after.x, after.y, after.z)
        end
    end,

    StopCC = function(self)
        if not self._controller then return end
        -- Sending 0s is a safe "do nothing" step.
        CharacterController.Move(self._controller, 0, 0, 0)

        -- Also kill RB velocity if anything is leaking into motion.
        if self._rb then
            pcall(function() self._rb.linearVel = { x=0, y=0, z=0 } end)
            pcall(function() self._rb.impulseApplied = { x=0, y=0, z=0 } end)
        end
    end,

    ApplyRotation = function(self, w, x, y, z)
        self._lastFacingRot = { w = w, x = x, y = y, z = z }
        self:SetRotation(w, x, y, z)
    end,

    FaceDirection = function(self, dx, dz)
        local w, x, y, z = yawQuatFromDir(dx, dz)
        self:ApplyRotation(w, x, y, z)
    end,

    FacePlayer = function(self)
        local tr = self._playerTr
        if not tr then
            tr = Engine.FindTransformByName(self.PlayerName)
            self._playerTr = tr
        end
        if not tr then return end

        local pp = Engine.GetTransformPosition(tr)
        local px, pz = pp[1], pp[3]

        local ex, ez = self:GetEnemyPosXZ()
        local dx, dz = px - ex, pz - ez

        local w, x, y, z = yawQuatFromDir(dx, dz)
        self:ApplyRotation(w, x, y, z)
    end,

    PlayClip = function(self, clipIndex, loop)
        if self._animator then
            self._animator:PlayClip(clipIndex, loop and true or false)
        end
    end,

    IsPlayerInRange = function(self, range)
        local tr = self._playerTr
        if not tr then
            tr = Engine.FindTransformByName(self.PlayerName)
            self._playerTr = tr
        end
        if not tr then return false end

        local pp = Engine.GetTransformPosition(tr)
        local px, pz = pp[1], pp[3]

        local ex, ez
        if self._controller then
            local pos = CharacterController.GetPosition(self._controller)
            if pos then
                ex, ez = pos.x, pos.z
            end
        end
        if not ex then
            local x, _, z = self:GetPosition()
            ex, ez = x, z
        end

        local dx, dz = (px - ex), (pz - ez)
        return (dx*dx + dz*dz) <= (range * range)
    end,

    GetEnemyPosXZ = function(self)
        if self._controller then
            local pos = CharacterController.GetPosition(self._controller)
            if pos then return pos.x, pos.z end
        end
        local x, _, z = self:GetPosition()
        return x, z
    end,

    SpawnKnife = function(self)
        local knife = KnifePool.Request()
        if not knife then return end

        local ex, ey, ez = self:GetPosition()
        local tr = self._playerTr
        if not tr then return end

        local pp = Engine.GetTransformPosition(tr)
        local px, py, pz = pp[1], pp[2] + 0.5, pp[3]

        local spawnX = ex - 0.5
        local spawnY = ey + 1.0
        local spawnZ = ez

        knife:Launch(spawnX, spawnY, spawnZ, px, py, pz)
    end,

    ApplyHit = function(self, dmg, hitType)
        if self.dead then return end
        if (self._hitLockTimer or 0) > 0 then return end
        self._hitLockTimer = self.config.HitIFrame or 0.1

        self.health = self.health - (dmg or 1)

        if self.health <= 0 then
            self.health = 0
            self.fsm:Change("Death", self.states.Death)
            return
        end

        if self.fsm.currentName == "Hooked" then
            return
        end
        self.fsm:ForceChange("Hurt", self.states.Hurt)
    end,

    ApplyHook = function(self, duration)
        if self.dead then return end
        if duration and duration > 0 then
            self.config.HookedDuration = duration
        end
        if self.fsm.currentName ~= "Hooked" then
            self.fsm:Change("Hooked", self.states.Hooked)
        end
    end,
}
