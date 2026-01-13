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

-- Estimate "half height" of the collider in world units (best-effort)
local function getColliderHalfHeight(col)
    if not col then return 1.0 end

    local shape = col.shapeTypeID or -1

    -- Assumption (typical enum order): Box=0, Sphere=1, Capsule=2, Cylinder=3, Mesh=4
    if shape == 0 and col.boxHalfExtents then
        return (col.boxHalfExtents.y or 1.0)
    elseif shape == 1 then
        return (col.sphereRadius or 0.5)
    elseif shape == 2 then
        local hh = col.capsuleHalfHeight or 0.5
        local r  = col.capsuleRadius or 0.25
        return hh + r
    elseif shape == 3 then
        local hh = col.cylinderHalfHeight or 0.5
        local r  = col.cylinderRadius or 0.25
        return hh + r
    end

    -- Mesh/unknown fallback
    return 1.0
end

local function getScaleY(self)
    -- If TransformMixin exposes scale, use it. Otherwise fallback to 1.
    local tr = self:GetComponent("Transform")
    if tr and tr.localScale and tr.localScale.y then
        return tr.localScale.y
    end
    return 1.0
end

local function getColliderFootprintRadius(col)
    if not col then return 0.35 end
    local shape = col.shapeTypeID or -1

    -- Box footprint radius
    if shape == 0 and col.boxHalfExtents then
        local hx = col.boxHalfExtents.x or 0.3
        local hz = col.boxHalfExtents.z or 0.3
        return math.max(hx, hz)
    end

    -- Sphere/Capsule/Cylinder radius
    if shape == 1 then return (col.sphereRadius or 0.25) end
    if shape == 2 then return (col.capsuleRadius or 0.25) end
    if shape == 3 then return (col.cylinderRadius or 0.25) end

    return 0.35
end

-- Returns clearance from feet to ground. Avoids self-hit by casting from points outside footprint.
local function groundClearanceMultiRay(self, rayUp, rayLen)
    local x, y, z = self:GetPosition()
    local col = self._collider
    if not col then return -1 end

    local scaleY = getScaleY(self)
    local halfH  = getColliderHalfHeight(col) * scaleY

    local r = getColliderFootprintRadius(col) * 0.95
    local skin = 0.03
    local off = r + skin

    local ox = x
    local oz = z

    local samples = {
        { ox + off, oz },
        { ox - off, oz },
        { ox, oz + off },
        { ox, oz - off },
        { ox + off * 0.707, oz + off * 0.707 },
        { ox - off * 0.707, oz + off * 0.707 },
        { ox + off * 0.707, oz - off * 0.707 },
        { ox - off * 0.707, oz - off * 0.707 },
    }

    local bestClearance = nil

    local originY = y + (rayUp or 0.8)
    local maxD = rayLen or 3.0

    for i = 1, #samples do
        local sx, sz = samples[i][1], samples[i][2]
        local d = Physics.Raycast(sx, originY, sz, 0, -1, 0, maxD)
        if d and d >= 0 then
            local groundY = originY - d
            local feetY = y - halfH
            local clearance = feetY - groundY
            if bestClearance == nil or clearance < bestClearance then
                bestClearance = clearance
            end
        end
    end

    return bestClearance or -1
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

        DetectionRange = 3.0,
        AttackCooldown = 1.0,
        HurtDuration   = 2.0,
        HitIFrame      = 0.2,
        HookedDuration = 4.0,

        EnablePatrol   = true,
        PatrolSpeed    = 0.6,
        PatrolDistance = 4.0,
        PatrolWait     = 0.5,

        ChaseSpeed     = 1.8,

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
        WallSkin       = 0.10,   -- extra distance to stop before wall
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
        self._animator = self:GetComponent("AnimationComponent")
        self._audio    = self:GetComponent("AudioComponent")
        self._collider = self:GetComponent("ColliderComponent")
        self._rb       = self:GetComponent("RigidBodyComponent")
        self._playerTr = Engine.FindTransformByName(self.PlayerName)

        local x, y, z = self:GetPosition()
        self._spawnX, self._spawnY, self._spawnZ = x, y, z

        local dist = self.config.PatrolDistance or self.PatrolDistance or 4.0
        self._patrolA = { x = x - dist, y = y, z = z }
        self._patrolB = { x = x + dist, y = y, z = z }
        self._patrolTarget = self._patrolB

        self._prevX, self._prevZ = x, z

        self.fsm:Change("Idle", self.states.Idle)
    end,

    Update = function(self, dt)
        if self.health <= 0 and not self.dead then
            self.fsm:Change("Death", self.states.Death)
        end

        -- Cache motion once rb is ready
        self._motionID = self._rb and self._rb.motionID or nil

        -- DEBUG print less spammy
        self._dbgT = (self._dbgT or 0) + dt
        if self._dbgT > 1.0 then
            self._dbgT = 0
            print("[EnemyRB] motionID=", tostring(self._motionID),
                "gravityFactor=", tostring(self._rb and self._rb.gravityFactor))
        end

        -- DEBUG
        if Input.GetKeyDown(Input.Key.H) then self:ApplyHit(1) end
        if Input.GetKeyDown(Input.Key.J) then self:ApplyHook(4.0) end

        self._hitLockTimer = math.max(0, (self._hitLockTimer or 0) - dt)

        if not self.fsm.current or not self.fsm.currentName then
            self.fsm:ForceChange("Idle", self.states.Idle)
        end

        -- ✅ Let FSM drive behaviour
        self.fsm:Update(dt)

        -- ✅ Only do custom grounding for KINEMATIC bodies
        -- Dynamic should be fully handled by Jolt.
        if isKinematic(self) then
            self:KinematicPostStep(dt)
        end
    end,

    -- === NEW: fixed-step kinematic grounding/collision ===
    KinematicPostStep = function(self, dt)
        if not self.UseKinematicGrounding then return end
        if not Physics or not Time then return end

        -- ✅ Kinematic only
        if not isKinematic(self) then return end

        local fixedDt = Time.GetFixedDeltaTime and Time.GetFixedDeltaTime() or (1.0 / 60.0)
        if fixedDt <= 0 then fixedDt = 1.0 / 60.0 end

        self._acc = (self._acc or 0) + dt
        if self._acc > 0.25 then self._acc = 0.25 end

        while self._acc >= fixedDt do
            self._acc = self._acc - fixedDt
            self:StepGroundAndWalls(fixedDt)
        end
    end,

    StepGroundAndWalls = function(self, fixedDt)
        local x, y, z = self:GetPosition()

        -- --- WALL BLOCKING (simple) ---
        if self._prevX ~= nil and self._prevZ ~= nil then
            local dx = x - self._prevX
            local dz = z - self._prevZ
            local dist2 = dx*dx + dz*dz
            if dist2 > 1e-8 then
                local dist = math.sqrt(dist2)
                local dirX = dx / dist
                local dirZ = dz / dist
                local rayY = y + (self.WallRayUp or 0.8)

                local hit = Physics.Raycast(x, rayY, z, dirX, 0, dirZ, dist + (self.WallSkin or 0.1))
                if hit >= 0 and hit <= dist + (self.WallSkin or 0.1) then
                    -- hit a wall: revert horizontal move
                    x = self._prevX
                    z = self._prevZ
                end
            end
        end

        -- --- GROUNDING (multi-ray) ---
        local eps     = self.GroundEps or 0.08
        local rayLen  = self.GroundRayLen or 3.0
        local maxSnap = self.GroundSnapMax or 0.35
        local rayUp   = self.GroundRayUp or 0.8

        local clearance = groundClearanceMultiRay(self, rayUp, rayLen)

        print(string.format("[GROUNDDBG] state=%s y=%.3f clearance=%.3f vy=%.3f",
        tostring(self.fsm.currentName), y, clearance, self._vy or 0))

        local grounded = (clearance >= 0) and (clearance <= eps)

        if grounded then
            -- If hovering above ground, snap DOWN
            if clearance > 0.001 then
                local snap = clearance
                if snap > maxSnap then snap = maxSnap end
                y = y - snap
            end
            self._vy = 0
        else
            local g = self.Gravity or -9.81
            self._vy = (self._vy or 0) + g * fixedDt

            local maxFall = self.MaxFallSpeed or -25.0
            if self._vy < maxFall then self._vy = maxFall end

            y = y + self._vy * fixedDt
        end

        self:SetPosition(x, y, z)
        self._prevX, self._prevZ = x, z
    end,

    SetMoveVelocityXZ = function(self, vx, vz)
        local rb = self._rb
        if not rb then return end

        -- Preserve current Y velocity so gravity/jumps still work
        local vy = 0
        if rb.linearVel and rb.linearVel.y then vy = rb.linearVel.y end

        rb.linearVel = rb.linearVel or { x=0, y=0, z=0 }
        rb.linearVel.x = vx or 0
        rb.linearVel.y = vy
        rb.linearVel.z = vz or 0
    end,

    -- === Movement command buffers (Lua-writable) ===
    StopMoveXZ = function(self)
        local rb = self._rb
        if not rb then return end
        -- stop horizontal push without touching Y/gravity
        pcall(function()
            rb.impulseApplied = { x = 0, y = 0, z = 0 }
        end)
    end,

    AddImpulseXZ = function(self, ix, iz)
        local rb = self._rb
        if not rb then return false end
        local ok = pcall(function()
            rb.impulseApplied = { x = ix or 0, y = 0, z = iz or 0 }
        end)
        return ok
    end,

    FaceDirection = function(self, dx, dz)
        local yaw = math.deg(atan2(dx, dz))
        local q = eulerToQuat(0, yaw, 0)
        self:SetRotation(q.w, q.x, q.y, q.z)
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

        local ex, _, ez = self:GetPosition()
        local dx, dz = (px - ex), (pz - ez)
        return (dx*dx + dz*dz) <= (range * range)
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

        local ex, _, ez = self:GetPosition()
        local dx, dz = px - ex, pz - ez

        local yaw = math.deg(atan2(dx, dz))
        local q = eulerToQuat(0, yaw, 0)
        self:SetRotation(q.w, q.x, q.y, q.z)
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
