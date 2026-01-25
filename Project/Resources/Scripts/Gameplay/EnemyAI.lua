-- Resources/Scripts/Gameplay/EnemyAI.lua
require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local StateMachine       = require("GamePlay.StateMachine")
local GroundIdleState    = require("GamePlay.GroundIdleState")
local GroundAttackState  = require("GamePlay.GroundAttackState")
local GroundHurtState    = require("GamePlay.GroundHurtState")
local GroundDeathState   = require("GamePlay.GroundDeathState")
local GroundHookedState  = require("GamePlay.GroundHookedState")
local GroundPatrolState  = require("GamePlay.GroundPatrolState")
local GroundChaseState = require("GamePlay.GroundChaseState")

local KnifePool = require("GamePlay.KnifePool")
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
    -- Guard: if direction is ~zero, do not produce a new rotation
    local lenSq = (dx or 0)*(dx or 0) + (dz or 0)*(dz or 0)
    if lenSq < 1e-10 then
        return nil
    end

    -- Normalize helps keep yaw stable when very small values appear
    local invLen = 1.0 / math.sqrt(lenSq)
    dx, dz = dx * invLen, dz * invLen

    local yaw = atan2(dx, dz)       -- radians (your convention)
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

local function toDtSec(dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return 0 end
    if dtSec > 0.05 then dtSec = 0.05 end
    return dtSec
end

local function _dumpPath(self, tag)
    if not self._path then
        print("[Nav] " .. tag .. " path=nil")
        return
    end
    print(string.format("[Nav] %s pathLen=%d", tag, #self._path))
    for i = 1, math.min(#self._path, 12) do
        local p = self._path[i]
        print(string.format("  [%d] (%.2f, %.2f, %.2f)", i, p.x or 0, p.y or 0, p.z or 0))
    end
end

return Component {
    mixins = { TransformMixin },

    fields = {
        MaxHealth = 5,

        DetectionRange = 4.0,
        AttackRange          = 3.0,   -- actually allowed to shoot
        --AttackEngageRange    = 3.5,   -- enter attack state
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

        -- PathRepathInterval = 0.45,
        -- PathGoalMoveThreshold = 0.9,
        -- PathWaypointRadius = 0.6,
        -- PathStuckTime = 0.75,

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

        if self._controller then
            pcall(function() CharacterController.DestroyByEntity(self.entityId) end)
            self._controller = nil
        end

        -- Create CC only if we have the right inputs, and only if we don't already have one
        if not self._controller and self._collider and self._transform then
            local ok, ctrl = pcall(function()
                return CharacterController.Create(self.entityId, self._collider, self._transform)
            end)
            if ok then
                self._controller = ctrl
            else
                print("[EnemyAI] CharacterController.Create failed")
                self._controller = nil
            end
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
        local skin = self.WallOffset or 0.1  -- same value CC uses

        -- Clamp patrol points slightly inward
        self._patrolA = { x = x - dist + skin, y = y, z = z }
        self._patrolB = { x = x + dist - skin, y = y, z = z }

        self._patrolWhich = 2
        self._patrolTarget = self._patrolB

        -- print(string.format("[EnemyAI] spawn=(%.2f,%.2f,%.2f) A=(%.2f,%.2f) B=(%.2f,%.2f)",
        -- self._spawnX, self._spawnY, self._spawnZ,
        -- self._patrolA.x, self._patrolA.z,
        -- self._patrolB.x, self._patrolB.z))
        print(string.format("[EnemyAI][Start] A=(%.2f,%.2f) B=(%.2f,%.2f)",
        self._patrolA.x, self._patrolA.z, self._patrolB.x, self._patrolB.z))

        self.fsm:Change("Idle", self.states.Idle)
    end,

    Update = function(self, dt)
        _G.__CC_UPDATED_THIS_FRAME = nil

        if self.health <= 0 and not self.dead then
            self.fsm:Change("Death", self.states.Death)
        end

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

        -- TEMP DEBUG: press K to force a small move step
        if Input.GetKeyDown(Input.Key.K) then
            print("[EnemyAI] DEBUG forced move step")
            self:MoveCC(1.0, 0.0, dt) -- 1 unit/sec to +X
        end

        local dtSec = toDtSec(dt)
        self._hitLockTimer = math.max(0, (self._hitLockTimer or 0) - dtSec)

        if not self.fsm.current or not self.fsm.currentName then
            self.fsm:ForceChange("Idle", self.states.Idle)
        end

        -- FSM drives behaviour (may call MoveCC)
        self.fsm:Update(dtSec)

        if not _G.__CC_UPDATED_THIS_FRAME then
            _G.__CC_UPDATED_THIS_FRAME = true
            --CharacterController.UpdateAll(dtSec)
        end

        -- then sync your own transform from your controller
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
        if not pp then return math.huge end
        local px, pz = pp[1], pp[3]

        -- enemy position MUST be current (controller-based if available)
        local ex, ez
        if self.GetEnemyPosXZ then
            ex, ez = self:GetEnemyPosXZ()
        else
            local x, _, z = self:GetPosition()
            ex, ez = x, z
        end

        -- Safety check: if position is nil, return huge distance
        if ex == nil or ez == nil then return math.huge end

        local dx, dz = px - ex, pz - ez
        return dx*dx + dz*dz
    end,

    MoveCC = function(self, vx, vz, dt)
        if not self._controller then
            self._dbgNoCCT = (self._dbgNoCCT or 0) + (dt or 0)
            if self._dbgNoCCT > 1.0 then
                self._dbgNoCCT = 0
                print("[EnemyAI] MoveCC called but _controller is NIL")
            end
            return
        end

        self._dbgMoveT = (self._dbgMoveT or 0) + (dt or 0)
        if self._dbgMoveT > 1.0 then
            self._dbgMoveT = 0
            local p = CharacterController.GetPosition(self._controller)
            print(string.format("[EnemyAI] MoveCC vx=%.3f vz=%.3f pos=(%.3f,%.3f,%.3f)",
                vx or 0, vz or 0, p.x, p.y, p.z))
        end

        CharacterController.Move(self._controller, vx or 0, 0, vz or 0)
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

    ClearPath = function(self)
        self._path = nil
        self._pathIndex = 1
        self._pathGoalX, self._pathGoalZ = nil, nil
        self._pathRepathT = 0
        self._pathStuckT = 0
        self._pathLastX, self._pathLastZ = nil, nil
    end,

    SetPath = function(self, waypoints, goalX, goalZ)
        self._path = waypoints
        self._pathIndex = 1
        self._pathGoalX, self._pathGoalZ = goalX, goalZ
        self._pathRepathT = 0
        self._pathStuckT = 0
        local ex, ez = self:GetEnemyPosXZ()
        self._pathLastX, self._pathLastZ = ex, ez
    end,

    RequestPathToXZ = function(self, goalX, goalZ)
        -- Always record fallback goal (so we can still move even without a path)
        self._fallbackGoalX, self._fallbackGoalZ = goalX, goalZ

        if not _G.NavService or not _G.NavService.RequestPathXZ then
            -- No nav service -> no path, but fallback goal remains
            self:ClearPath()
            return false
        end

        local sx, sz = self:GetEnemyPosXZ()
        local path = _G.NavService.RequestPathXZ(sx, sz, goalX, goalZ)
        _dumpPath(self, "RECEIVED")

        if path and #path >= 1 then
            print(string.format("[Nav] PATH OK len=%d", #path))
            self:SetPath(path, goalX, goalZ)
            return true
        end

        print("[Nav] PATH FAIL -> fallback direct movement ENABLED")
        self:ClearPath()
        return false
    end,

    ShouldRepathToXZ = function(self, goalX, goalZ, dtSec)
        local repathInterval = self.PathRepathInterval or 0.45
        local goalMoveThres  = self.PathGoalMoveThreshold or 0.9

        self._pathRepathT = (self._pathRepathT or 0) + (dtSec or 0)

        -- no path yet
        if not self._path or not self._pathIndex then
            return true
        end

        -- timed repath
        if self._pathRepathT >= repathInterval then
            return true
        end

        -- goal moved enough since last planned goal
        if self._pathGoalX and self._pathGoalZ then
            local dx = goalX - self._pathGoalX
            local dz = goalZ - self._pathGoalZ
            if (dx*dx + dz*dz) >= (goalMoveThres * goalMoveThres) then
                return true
            end
        end

        return false
    end,

    -- Returns: true if reached end-of-path (arrived), false otherwise
    FollowPath = function(self, dtSec, speed)
        if not self._path or #self._path == 0 then
        if self._fallbackGoalX and self._fallbackGoalZ then
            -- Fallback movement should NOT signal arrival here
            self:MoveDirectToXZ(self._fallbackGoalX, self._fallbackGoalZ, dtSec, speed)
            return false
        end
        self:StopCC()
        return false
    end

        local idx = self._pathIndex or 1
        if idx > #self._path then
            self:StopCC()
            return true
        end

        local wp = self._path[idx]
        if not wp then
            self:StopCC()
            return true
        end

        local ex, ez = self:GetEnemyPosXZ()
        local dx = (wp.x or 0) - ex
        local dz = (wp.z or 0) - ez
        local d2 = dx*dx + dz*dz

        local arriveR = self.PathWaypointRadius or 0.6
        local arriveR2 = arriveR * arriveR

        print(string.format("[Nav] following idx=%d / %d target=(%.2f, %.2f, %.2f)",
            self._pathIndex or 1, #self._path,
            node.x, node.y or 0, node.z
        ))

        -- Advance waypoint if close enough
        if d2 <= arriveR2 then
            self._pathIndex = idx + 1
            -- If that was the last waypoint, we arrived.
            if self._pathIndex > #self._path then
                self:StopCC()
                return true
            end
            wp = self._path[self._pathIndex]
            if not wp then
                self:StopCC()
                return true
            end
            -- recompute to new waypoint
            ex, ez = self:GetEnemyPosXZ()
            dx = (wp.x or 0) - ex
            dz = (wp.z or 0) - ez
            d2 = dx*dx + dz*dz
            if d2 <= 1e-8 then
                self:StopCC()
                return false
            end
        end

        local d = math.sqrt(d2)
        if d <= 1e-6 then
            self:StopCC()
            return false
        end

        local dirX, dirZ = dx / d, dz / d
        self:MoveCC(dirX * (speed or 0), dirZ * (speed or 0))
        self:FaceDirection(dirX, dirZ)

        -- Simple stuck detection while following path
        local lastX, lastZ = self._pathLastX, self._pathLastZ
        if lastX ~= nil and lastZ ~= nil then
            local mx = ex - lastX
            local mz = ez - lastZ
            local movedSq = mx*mx + mz*mz
            local movedEpsSq = 1e-6

            if movedSq < movedEpsSq then
                self._pathStuckT = (self._pathStuckT or 0) + dtSec
            else
                self._pathStuckT = 0
            end
        end
        self._pathLastX, self._pathLastZ = ex, ez

        return false
    end,

    MoveDirectToXZ = function(self, goalX, goalZ, dtSec, speed)
        local ex, ez = self:GetEnemyPosXZ()
        local dx = (goalX or ex) - ex
        local dz = (goalZ or ez) - ez
        local d2 = dx*dx + dz*dz

        if d2 < 1e-6 then
            self:StopCC()
            return true
        end

        local d = math.sqrt(d2)
        local dirX, dirZ = dx / d, dz / d

        self:MoveCC(dirX * (speed or 0), dirZ * (speed or 0))
        self:FaceDirection(dirX, dirZ)
        return false
    end,

    ApplyRotation = function(self, w, x, y, z)
        self._lastFacingRot = { w = w, x = x, y = y, z = z }
        self:SetRotation(w, x, y, z)
    end,

    FaceDirection = function(self, dx, dz)
        local q = { yawQuatFromDir(dx, dz) }
        -- If direction is too small, DO NOT change rotation (prevents “flip/lie down”)
        if #q == 0 then
            return
        end
        local w, x, y, z = q[1], q[2], q[3], q[4]
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
        if not pp then return end
        local px, pz = pp[1], pp[3]

        local ex, ez = self:GetEnemyPosXZ()
        -- Safety check: if position is nil, skip facing
        if ex == nil or ez == nil then return end
        local dx, dz = px - ex, pz - ez

        local q = { yawQuatFromDir(dx, dz) }
        if #q == 0 then
            return -- don't rotate if player is basically on top of enemy
        end

        local w, x, y, z = q[1], q[2], q[3], q[4]
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
        if not pp then return false end
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

        -- Safety check: if position is nil, return false (not in range)
        if ex == nil or ez == nil then return false end

        local dx, dz = (px - ex), (pz - ez)
        return (dx*dx + dz*dz) <= (range * range)
    end,

    GetEnemyPosXZ = function(self)
        if self._controller then
            local pos = CharacterController.GetPosition(self._controller)
            if pos then return pos.x, pos.z end
        end
        local x, _, z = self:GetPosition()
        -- Return nil if position is not available (caller should handle)
        return x, z
    end,

    SpawnKnife = function(self)
        local knife = KnifePool.Request()
        if not knife then return end

        local ex, ey, ez = self:GetPosition()
        -- Safety check: if position is nil, skip spawning
        if ex == nil or ey == nil or ez == nil then return end

        local tr = self._playerTr
        if not tr then return end

        local pp = Engine.GetTransformPosition(tr)
        if not pp then return end
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

    OnDisable = function(self)
        -- Stop any movement immediately
        pcall(function() self:StopCC() end)

        -- Clear pathing state so we don't resume stale movement
        pcall(function() self:ClearPath() end)

        -- Best-effort: destroy controller if the API exists.
        -- (Different engines name this differently; we probe safely.)
        if self._controller then
            pcall(function()
                if CharacterController.DestroyByEntity then
                    CharacterController.DestroyByEntity(self.entityId)
                elseif CharacterController.Remove then
                    CharacterController.Remove(self._controller)
                elseif CharacterController.Release then
                    CharacterController.Release(self._controller)
                end
            end)
        end
        self._controller = nil

        -- Kill RB velocity (prevents kinematic leftovers)
        if self._rb then
            pcall(function() self._rb.linearVel = { x=0, y=0, z=0 } end)
            pcall(function() self._rb.impulseApplied = { x=0, y=0, z=0 } end)
        end

        -- Reset facing cache (avoid “lying down” from stale quat)
        self._lastFacingRot = nil

        -- If you use any subscriptions elsewhere, ensure they're removed
        pcall(function()
            if self.Unsubscribe then self:Unsubscribe() end
        end)
    end,

    OnDestroy = function(self)
        -- Prevent “0xDDDDDDDD” shape pointer crashes on subsequent plays:
        -- Lua must stop calling Update/GetPosition on an old controller pointer.
        if self._controller then
            pcall(function()
                CharacterController.DestroyByEntity(self.entityId)
            end)
            self._controller = nil
        end
    end,
}
