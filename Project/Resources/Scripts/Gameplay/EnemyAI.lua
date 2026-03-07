-- Resources/Scripts/GamePlay/EnemyAI.lua
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
local GroundChaseState   = require("Gameplay.GroundChaseState")

local FlyingIdleState   = require("Gameplay.FlyingIdleState")
local FlyingPatrolState = require("Gameplay.FlyingPatrolState")
local FlyingChaseState  = require("Gameplay.FlyingChaseState")
local FlyingAttackState = require("Gameplay.FlyingAttackState")
local FlyingHookedState = require("Gameplay.FlyingHookedState")

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

-- Play random SFX from array
local function playRandomSFX(audio, clips)
    local count = clips and #clips or 0
    if count > 0 and audio then
        audio:PlayOneShot(clips[math.random(1, count)])
    end
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
    -- if not self._path then
    --     print("[Nav] " .. tag .. " path=nil")
    --     return
    -- end
    -- --print(string.format("[Nav] %s pathLen=%d", tag, #self._path))
    -- for i = 1, math.min(#self._path, 12) do
    --     local p = self._path[i]
    --     print(string.format("  [%d] (%.2f, %.2f, %.2f)", i, p.x or 0, p.y or 0, p.z or 0))
    -- end
end

return Component {
    mixins = { TransformMixin },

    fields = {
        EnemyType = "Ground",

        MaxHealth = 5,

        DetectionRange       = 4.0,
        AttackRange          = 3.0,   -- actually allowed to shoot
        AttackDisengageRange = 4.0,   -- exit attack state (slightly bigger)
        AttackCooldown       = 3.0,
        RangedAnimDelay      = 1.0,
        MeleeAnimDelay       = 1.2,
        IsMelee              = false,
        IsPassive            = false,
        MeleeSpeed           = 0.9,
        MeleeRange           = 1.2,
        MeleeDamage          = 3,
        MeleeAttackCooldown  = 5.0,

        HurtDuration      = 2.0,
        HitIFrame         = 0.2,
        HookedDuration    = 4.0,
        KnockbackStrength = 12.0,
        KnockbackDuration = 0.5,

        FeatherSkillBufferDuration = 0.2,

        HookStopDistance    = 1.2,
        HookStaggerTime     = 1.0,
        HookStaggerSpeed    = 50.0,
        HookStaggerMaxStep  = 25.0,
        HookHardSpeed       = 1200.0,
        HookHardMaxStep     = 600.0,

        EnablePatrol     = true,
        PatrolSpeed      = 0.3,
        PatrolDistance   = 3.0,
        PatrolWait       = 1.5,
        ChaseSpeed       = 0.6,
        HoverHeight      = 2.0,
        HoverSnapSpeed   = 8.0,
        FlyingChaseSpeed = 0.8,

        HoverBobAmp        = 0.02,
        HoverBobFreq       = 0.9,
        SlamDownSpeed      = 16.0,
        HookedLandingDelay = 5.0,

        PathRepathInterval = 10,
        PathGoalMoveThreshold = 0.9,
        PathWaypointRadius = 0.6,
        PathStuckTime = 0.75,

        PatrolPointA_X = 2.0,
        PatrolPointA_Z = -1.1,
        PatrolPointB_X = -2.0,
        PatrolPointB_Z = -1.1,

        ClipIdle   = 0,
        ClipAttack = 1,
        ClipHurt   = 2,
        ClipDeath  = 3,

        PlayerName = "Player",

        FeatherPrefabPath = "Resources/Prefabs/Feather.prefab",
        NumFeathersSpawnedPerHit = 5,

        -- === Kinematic grounding tuning ===
        UseKinematicGrounding = true,

        GroundRayUp    = 0.35,   -- cast origin is y + this
        GroundRayLen   = 3.0,    -- max ray distance downward
        GroundEps      = 0.08,   -- considered grounded if hit <= GroundRayUp + eps
        GroundSnapMax  = 0.35,   -- max snap correction per step

        Gravity        = -9.81,
        MaxFallSpeed   = -25.0,

        WallRayUp      = 0.8,
        WallSkin       = 0.18,
        WallOffset     = 0.08,
        MinStep        = 0.01,

        -- SFX clip arrays (populate in editor with audio GUIDs)
        enemyHurtSFX = {},
        enemyDeathSFX = {},
        enemyAlertSFX = {},         -- Growl when first detecting player
        enemyMeleeAttackSFX = {},   -- Scratch woosh for melee enemies
        enemyMeleeHitSFX = {},      -- Scratch hit when melee lands on player
        enemyRangedAttackSFX = {},  -- Throw woosh for ranged enemies
        enemyRangedHitSFX = {},     -- Throw hit when ranged lands on player
    },

    Awake = function(self)

        self.dead = false
        self.health = self.MaxHealth
        self._hitLockTimer = 0
        self._featherSkillBufferTimer = 0

        self.fsm = StateMachine.new(self)

        self:BuildStateProfile()

        self.clips = {
            Idle   = self.ClipIdle,
            Attack = self.ClipAttack,
            Hurt   = self.ClipHurt,
            Death  = self.ClipDeath,
        }

        self.config = {
            DetectionRange       = self.DetectionRange,
            AttackRange          = self.AttackRange,
            AttackDisengageRange = self.AttackDisengageRange,
            ChaseSpeed           = self.ChaseSpeed,
            AttackCooldown       = self.AttackCooldown,
            MeleeRange           = self.MeleeRange,
            MeleeDamage          = self.MeleeDamage,
            MeleeAttackCooldown  = self.MeleeAttackCooldown,
            HurtDuration         = self.HurtDuration,
            HitIFrame            = self.HitIFrame,
            HookedDuration       = self.HookedDuration,
            HookPullSpeed        = self.HookPullSpeed,
            HookStopDistance     = self.HookStopDistance,
            HookMaxStep          = self.HookMaxStep,
            PatrolSpeed          = self.PatrolSpeed,
            PatrolDistance       = self.PatrolDistance,
            PatrolWait           = self.PatrolWait,
            EnablePatrol         = self.EnablePatrol,
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
        self.particles  = self:GetComponent("ParticleComponent")

        self._entityName = Engine.GetEntityName(self.entityId)
        self._featherEntities = {}

        if self._controller then
            pcall(function() CharacterController.DestroyByEntity(self.entityId) end)
            self._controller = nil
        end

        if self._animator then
            --print("[PlayerMovement] Animator found, playing IDLE clip")
            --self._animator:PlayClip(IDLE, true)
        else
            print("[PlayerMovement] ERROR: Animator is nil!")
        end

        self._animator:SetBool("PatrolEnabled", EnablePatrol)
        self._animator:SetBool("Passive", IsPassive)
        self._animator:SetBool("Melee", IsMelee)
        self._animator:SetBool("Flying", self:IsFlying())

        if (not self:IsFlying()) and (not self._controller) and self._collider and self._transform then
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
            pcall(function() self._rb.motionID = 0 end)
            pcall(function() self._rb.linearVel = { x=0, y=0, z=0 } end)
            pcall(function() self._rb.impulseApplied = { x=0, y=0, z=0 } end)
        end
        if self:IsFlying() and self._rb then
            -- stop physics from pulling the flyer down
            pcall(function() self._rb.gravityFactor = 0 end)
            pcall(function() self._rb.linearVel = { x=0, y=0, z=0 } end)
            pcall(function() self._rb.impulseApplied = { x=0, y=0, z=0 } end)
        end

        if self.particles then
            self.particles.isEmitting   = false
            self.particles.emissionRate = 0
        end

        -- === Damage event subscription ===
        self._damageSub = nil
        if _G.event_bus and _G.event_bus.subscribe then
            self._damageSub = _G.event_bus.subscribe("enemy_damage", function(payload)
                if not payload then return end

                -- Correct key: DamageZone sends payload.entityId
                if payload.entityId ~= nil and payload.entityId ~= self.entityId then
                    return
                end

                local dmg = payload.dmg or 1
                local hitType = payload.hitType or payload.src or "MELEE"
                self:ApplyHit(dmg, hitType)
            end)
            
            self._frozenBycinematic = false
            self._freezeEnemySub = _G.event_bus.subscribe("freeze_enemy", function(frozen)
                self._frozenBycinematic = frozen
                if frozen then
                    self:StopCC()
                end
            end)

            self._playerRespawned = false
            self._respawnPlayerSub = _G.event_bus.subscribe("respawnPlayer", function(respawn)
                self._playerRespawned = respawn
            end)

            self._playerDead = false
            self._playerDeadSub = _G.event_bus.subscribe("playerDead", function(playerDead)
                self._playerDead = playerDead
            end)

            self._comboDamageSub = _G.event_bus.subscribe("deal_damage_to_entity", function(payload)
                if not payload then return end

                if payload.entityId ~= self.entityId then
                    return
                end

                local damage = payload.damage or 10
                local hitType = payload.hitType or "COMBO"
                self:ApplyHit(damage, hitType)
            end)

            self._chainEndpointHitSub = _G.event_bus.subscribe("chain.endpoint_hit_entity", function(payload)
                if not payload then return end
                if payload.rootName ~= self._entityName then return end
                print("[EnemyAI] chain.endpoint_hit_entity received")
                self._animator:SetTrigger("Hooked")
            end)

            self._chainHookSub = _G.event_bus.subscribe("chain.enemy_hooked", function(payload)
                if not payload then return end
                if payload.entityId ~= self.entityId then return end
                print("[EnemyAI] chain.enemy_hooked received — calling ApplyHook duration=" .. tostring(payload.duration))
                pcall(function() self:ApplyHook(payload.duration) end)
            end)
        end

        -- If you're using CC, do NOT run your old kinematic grounding system
        self.UseKinematicGrounding = false

        self._playerTr = Engine.FindTransformByName(self.PlayerName)

        local x, y, z = self:GetPosition()
        self._spawnX, self._spawnY, self._spawnZ = x, y, z

        local dist = self.config.PatrolDistance or self.PatrolDistance or 3.0
        local skin = self.WallOffset or 0.1  -- same value CC uses

        -- Clamp patrol points slightly inward
        self._patrolA = { x = self.PatrolPointA_X, y = y, z = self.PatrolPointA_Z }
        self._patrolB = { x = self.PatrolPointB_X, y = y, z = self.PatrolPointB_Z }
        print("[EnemyAI] Patrol points set")

        self._patrolWhich = 2
        self._patrolTarget = self._patrolB

        self._patrolWaitT = self.config.PatrolWait
        self._isPatrolWait = false

        -- print(string.format("[EnemyAI] spawn=(%.2f,%.2f,%.2f) A=(%.2f,%.2f) B=(%.2f,%.2f)",
        -- self._spawnX, self._spawnY, self._spawnZ,
        -- self._patrolA.x, self._patrolA.z,
        -- self._patrolB.x, self._patrolB.z))
        -- print(string.format("[EnemyAI][Start] A=(%.2f,%.2f) B=(%.2f,%.2f)",
        -- self._patrolA.x, self._patrolA.z, self._patrolB.x, self._patrolB.z))

        self:BuildStateProfile()
        self.fsm:Change("Idle", self.states.Idle)

        if self._controller and CharacterController.SetPosition then
            local x, y, z = self:GetPosition()
            pcall(function()
                CharacterController.SetPosition(self._controller, x, y, z)
            end)
        end

        -- Save the initial spawn position so the enemy can teleport back here when player respawns.
        self._initialPos = { x = self._spawnX, y = self._spawnY, z = self._spawnZ }
    end,

    Update = function(self, dt)
        _G.__CC_UPDATED_THIS_FRAME = nil

        if self.health <= 0 and not self.dead then
            self.dead = true
        end

        if self.dead then
            self.fsm:Change("Death", self.states.Death)
        end

        if self._freezeAI or self._despawned or self._softDespawned then return end

        -- Freeze movement during cinematic
        if self._frozenBycinematic then return end

        -- When player respawns, teleport the enemy back to its initial position.
        if self._playerRespawned and self._initialPos then
            self:SetPosition(self._initialPos.x, self._initialPos.y + 1.0, self._initialPos.z)
            CharacterController.SetPosition(self._controller, self._transform)
            print(string.format("[EnemyAI] Teleported enemy %d to %f %f %f", self.entityId, self._initialPos.x, self._initialPos.y, self._initialPos.z))
            self.fsm:ForceChange("Idle", self.states.Idle)

            self._playerRespawned = false
            self._playerDead = false
            return
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

        -- -- TEMP DEBUG: press K to force a small move step
        -- if Input.GetKeyDown(Input.Key.K) then
        --     print("[EnemyAI] DEBUG forced move step")
        --     self:MoveCC(1.0, 0.0, dt) -- 1 unit/sec to +X
        -- end

        if Input.IsActionJustPressed("Interact") then
            self:ApplyHook(self.HookedDuration)
        end

        local dtSec = toDtSec(dt)
        self._hitLockTimer = math.max(0, (self._hitLockTimer or 0) - dtSec)
        self._featherSkillBufferTimer = math.max(0, (self._featherSkillBufferTimer or 0) - dtSec)

        if not self.fsm.current or not self.fsm.currentName then
            self.fsm:ForceChange("Idle", self.states.Idle)
        end

        -- FSM drives behaviour (may call MoveCC)
        self.fsm:Update(dtSec)

        -- flying hook slam-down sequence
        if self:IsFlying() and self._slamActive then
            local landed = self:UpdateSlamDown(dtSec)
            if landed then
                -- once we hit the ground, convert + start ground hooked pull
                self:ConvertToGroundEnemy({ nextState = "Hooked" })
            end
        end

        -- Apply knockback movement regardless of current FSM state
        if self._kbT and self._kbT > 0 then
            self._kbT = self._kbT - dtSec
            if self._kbT < 0 then self._kbT = 0 end

            -- convert kb velocity (units/sec) -> displacement this frame
            local mx = (self._kbVX or 0) * dtSec
            local mz = (self._kbVZ or 0) * dtSec

            -- clamp displacement so it never "teleports"
            local maxStep = 0.20  -- tune (0.15 - 0.30)
            if mx >  maxStep then mx =  maxStep end
            if mx < -maxStep then mx = -maxStep end
            if mz >  maxStep then mz =  maxStep end
            if mz < -maxStep then mz = -maxStep end

            if self:IsFlying() then
                -- Move transform XZ, then resync CC to that exact position
                local x,y,z = self:GetPosition()
                local nx, nz = x + mx, z + mz
                self:SetPosition(nx, y, nz)
                if self._controller and CharacterController.SetPosition then
                    pcall(function()
                        CharacterController.SetPosition(self._controller, nx, y, nz)
                    end)
                end
            else
                CharacterController.Move(self._controller, mx, 0, mz)
            end
        end

        if not _G.__CC_UPDATED_THIS_FRAME then
            _G.__CC_UPDATED_THIS_FRAME = true
            --CharacterController.UpdateAll(dtSec)
        end

        -- Sync Transform from CharacterController every frame (ground enemies),
        -- and clamp Y to ground so they don't float.
        if self._controller then
            local pos = CharacterController.GetPosition(self._controller)
            if pos then
                if not self:IsFlying() then
                    local groundY = (Nav and Nav.GetGroundY) and Nav.GetGroundY(self.entityId) or pos.y
                    self:SetPosition(pos.x, groundY, pos.z)
                else
                    -- flying: ALWAYS sync XZ only, never touch Y here
                    local x, y, z = self:GetPosition()
                    if x ~= nil then
                        self:SetPosition(pos.x, y, pos.z)
                    end
                end
            end
        end

        -- broadcast enemy position (for DamageZone / melee test)
        if _G.event_bus and _G.event_bus.publish then
            local x, y, z = self:GetPosition()
            _G.event_bus.publish("enemy_position", {
                entityId = self.entityId,
                x = x, y = y, z = z
            })
        end
    end,

    GetRanges = function(self)
        local attackR = (self.config and self.config.AttackRange) or self.AttackRange or 3.0
        local meleeR = (self.config and self.config.MeleeRange) or self.MeleeRange or 1.2
        local diseng  = (self.config and self.config.AttackDisengageRange) or self.AttackDisengageRange or 4.0

        -- safety: enforce diseng >= attack
        if diseng < attackR then diseng = attackR + 0.25 end
        return attackR, meleeR, diseng
    end,

    GetPlayerDistanceSq = function(self)
        -- If the player is dead, this function always returns a huge value so the enemy exits the chase/attack state.
        if self._playerDead then
            return math.huge
        end

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
        if self:IsFlying() then
            local x,y,z = self:GetPosition()
            if x == nil then return end

            -- Treat vx,vz as velocity (units/sec) like your ground logic intends
            local mx = (vx or 0) * dt
            local mz = (vz or 0) * dt

            local nx, nz = x + mx, z + mz
            self:SetPosition(nx, y, nz)

            -- keep CC synced, but never Move() it
            if self._controller and CharacterController.SetPosition then
                pcall(function()
                    CharacterController.SetPosition(self._controller, nx, y, nz)
                end)
            end
            return
        end

        if not self._controller then
            self._dbgNoCCT = (self._dbgNoCCT or 0) + (dt or 0)
            if self._dbgNoCCT > 1.0 then
                self._dbgNoCCT = 0
                print("[EnemyAI] MoveCC called but _controller is NIL")
            end
            return
        end

        if (self._kbT or 0) > 0 then
            -- ignore normal movement while knockback is active
            -- (but allow if this MoveCC call is the knockback call)
            -- easiest: add a flag when calling knockback MoveCC
            if not self._isApplyingKnockback then return end
        end

        self._dbgMoveT = (self._dbgMoveT or 0) + (dt or 0)
        if self._dbgMoveT > 1.0 then
            self._dbgMoveT = 0
            local p = CharacterController.GetPosition(self._controller)
            -- print(string.format("[EnemyAI] MoveCC vx=%.3f vz=%.3f pos=(%.3f,%.3f,%.3f)",
            --     vx or 0, vz or 0, p.x, p.y, p.z))
        end

        local pos = CharacterController.GetPosition(self._controller)
        --print(string.format("[EnemyAI] Before MoveCC Position: %f %f %f", pos.x, pos.y, pos.z))
        CharacterController.Move(self._controller, vx or 0, 0, vz or 0)
        pos = CharacterController.GetPosition(self._controller)
        --print(string.format("[EnemyAI] After MoveCC Position: %f %f %f", pos.x, pos.y, pos.z))
    end,

    StopCC = function(self)
        if not self._controller then return end
        local pos = CharacterController.GetPosition(self._controller)
        --print(string.format("[EnemyAI] Before StopCC Position: %f %f %f", pos.x, pos.y, pos.z))
        -- Sending 0s is a safe "do nothing" step.
        CharacterController.Move(self._controller, 0, 0, 0)
        pos = CharacterController.GetPosition(self._controller)
        --print(string.format("[EnemyAI] After StopCC Position: %f %f %f", pos.x, pos.y, pos.z))

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
        --print("[EnemyAI] ClearPath called. Path is nil")
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
        if not Nav then
            print("[Nav] ERROR: Nav is NIL! Nav system not bound to Lua!")
            self:ClearPath()
            return false
        end
        
        if not Nav.RequestPathXZ then
            print("[Nav] ERROR: Nav.RequestPathXZ is NIL! Function not bound!")
            self:ClearPath()
            return false
        end
        
        --print("[Nav] Nav binding OK, calling RequestPathXZ...")
        
        local sx, sz = self:GetEnemyPosXZ()
        local path = Nav.RequestPathXZ(sx, sz, goalX, goalZ, self.entityId)
        
        _dumpPath(self, "RECEIVED")

        --print(string.format("[Nav] path = %d", #path))

        if path and #path >= 1 then
            --print(string.format("[Nav] PATH OK len=%d", #path))
            self:SetPath(path, goalX, goalZ)
            return true
        end

        --print("[Nav] PATH FAIL -> fallback direct movement ENABLED")
        self:ClearPath()
        return false
    end,

    ShouldRepathToXZ = function(self, goalX, goalZ, dtSec)
        local repathInterval = self.PathRepathInterval or 0.45
        local goalMoveThres  = self.PathGoalMoveThreshold or 0.9

        self._pathRepathT = (self._pathRepathT or 0) + (dtSec or 0)

        -- no path yet
        if not self._path or not self._pathIndex then
            --print("[ShouldRepathToXZ] TRUE. PATH IS EMPTY")
            return true
        end

        -- timed repath
        if self._pathRepathT >= repathInterval then
            --print("[ShouldRepathToXZ] TRUE. self._pathRepathT: ", self._pathRepathT)
            return true
        end

        -- goal moved enough since last planned goal
        if self._pathGoalX and self._pathGoalZ then
            local dx = goalX - self._pathGoalX
            local dz = goalZ - self._pathGoalZ
            if (dx*dx + dz*dz) >= (goalMoveThres * goalMoveThres) then
                --print("[ShouldRepathToXZ] TRUE. (dx*dx + dz*dz) >= (goalMoveThres * goalMoveThres)")
                return true
            end
        end
        
        return false
    end,

    -- Returns: true if reached end-of-path (arrived), false otherwise
    FollowPath = function(self, dtSec, speed)
        if not self._path or #self._path == 0 then
            -- NO FALLBACK - if there's no path, STOP
            --print("[FollowPath] NO PATH, STOPPING CC")
            self:StopCC()
            return false
        end
        
        local idx = self._pathIndex or 1
        if idx > #self._path then
            --print("[FollowPath] REACHED GOAL, STOPPING CC")
            self:StopCC()
            return true
        end

        local wp = self._path[idx]
        if not wp then
            --print("[FollowPath] INVALID WAYPOINT, idx=", idx);
            self:StopCC()
            return true
        end

        local ex, ez = self:GetEnemyPosXZ()
        local dx = (wp.x or 0) - ex
        local dz = (wp.z or 0) - ez
        local d2 = dx*dx + dz*dz

        local arriveR = self.PathWaypointRadius or 0.6
        local arriveR2 = arriveR * arriveR

        -- print(string.format("[Nav] following idx=%d / %d target=(%.2f, %.2f, %.2f)",
        --     self._pathIndex or 1, #self._path,
        --     node.x, node.y or 0, node.z
        -- ))

        -- Advance waypoint if close enough
        if d2 <= arriveR2 then
            --print("[FollowPath] REACHED WAYPOINT ", idx)
            self._pathIndex = idx + 1
            -- If that was the last waypoint, we arrived.
            if self._pathIndex > #self._path then
                --print("[FollowPath] REACHED GOAL, STOPPING CC")
                self:StopCC()
                return true
            end
            wp = self._path[self._pathIndex]
            --print("[FollowPath] NEW PATHINDEX: ", self._pathIndex)
            --print(string.format("[FollowPath] NEW WAYPOINT: %f %f %f", wp.x, wp.y, wp.z))
            if not wp then
                --print("[FollowPath] INVALID WAYPOINT, idx=", idx);
                self:StopCC()
                return true
            end
            -- recompute to new waypoint
            ex, ez = self:GetEnemyPosXZ()
            dx = (wp.x or 0) - ex
            dz = (wp.z or 0) - ez
            d2 = dx*dx + dz*dz
            if d2 <= 1e-8 then
                --print("[FollowPath] REACHED NEW ENDPOINT")
                self:StopCC()
                return false
            end
        end

        local d = math.sqrt(d2)
        -- if d <= 1e-6 then
        --     self:StopCC()
        --     return false
        -- end

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

    PullTowardPlayer = function(self, dtSec)
        if not self._controller then return end
        if (self._kbT or 0) > 0 then
            return
        end

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
        if ex == nil or ez == nil then return end

        local dx, dz = px - ex, pz - ez
        local d2 = dx*dx + dz*dz

        -- stop once close enough
        local stopR = self.HookStopDistance or 1.2
        if d2 <= (stopR * stopR) then
            self:StopCC()
            return
        end

        local d = math.sqrt(d2)
        if d < 1e-6 then
            self:StopCC()
            return
        end

        local dirX, dirZ = dx / d, dz / d

        -- ONE quick hard pull only
        local pullSpeed = tonumber(self.HookHardSpeed) or 1200.0
        local maxStep   = tonumber(self.HookHardMaxStep) or 600.0

        local step = pullSpeed * (dtSec or 0)
        if step > maxStep then step = maxStep end

        local mx = dirX * step
        local mz = dirZ * step

        CharacterController.Move(self._controller, mx, 0, mz)
        self:FaceDirection(dirX, dirZ)
    end,

    IsFlying = function(self)
        local t = tostring(self.EnemyType or "")
        t = string.lower(string.gsub(t, "%s+", ""))
        return t == "flying"
    end,

    BuildStateProfile = function(self)
        if self:IsFlying() then
            self.states = {
                Idle   = FlyingIdleState,
                Patrol = FlyingPatrolState,
                Chase  = FlyingChaseState,
                Attack = FlyingAttackState,
                Hooked = FlyingHookedState,

                -- reuse existing
                Hurt   = GroundHurtState,
                Death  = GroundDeathState,
            }
        else
            self.states = {
                Idle   = GroundIdleState,
                Attack = GroundAttackState,
                Hurt   = GroundHurtState,
                Death  = GroundDeathState,
                Hooked = GroundHookedState,
                Patrol = GroundPatrolState,
                Chase  = GroundChaseState,
            }
        end
    end,

    MaintainHover = function(self, dtSec)
        dtSec = toDtSec(dtSec)
        if dtSec <= 0 then return end
        if self._slamActive then return end  -- don't hover while slamming

        local x, y, z = self:GetPosition()
        if x == nil then return end

        local gy = y
        if Nav and Nav.GetGroundY then
            local g = Nav.GetGroundY(self.entityId)
            if g ~= nil then gy = g end
        end

        -- bobbing target
        self._hoverT = (self._hoverT or 0) + dtSec
        local amp  = tonumber(self.HoverBobAmp)  or 0
        local freq = tonumber(self.HoverBobFreq) or 0
        local bob = 0
        if amp ~= 0 and freq ~= 0 then
            bob = math.sin(self._hoverT * (math.pi * 2) * freq) * amp
        end

        local baseTargetY = (gy or y) + (self.HoverHeight or 2.0)
        local targetY = baseTargetY + bob

        local snap = self.HoverSnapSpeed or 8.0
        local dy = targetY - y
        local maxStep = snap * dtSec
        if dy >  maxStep then dy =  maxStep end
        if dy < -maxStep then dy = -maxStep end

        self:SetPosition(x, y + dy, z)
    end,

    MoveTowardPlayerXZ_Flying = function(self, dtSec, speed)
        dtSec = toDtSec(dtSec)
        if dtSec <= 0 then return end

        local tr = self._playerTr
        if not tr then
            tr = Engine.FindTransformByName(self.PlayerName)
            self._playerTr = tr
        end
        if not tr then return end

        local pp = Engine.GetTransformPosition(tr)
        if not pp then return end
        local px, pz = pp[1], pp[3]

        local ex, _, ez = self:GetPosition()
        if ex == nil then return end

        local dx, dz = px - ex, pz - ez
        local d2 = dx*dx + dz*dz
        if d2 < 1e-8 then return end

        local d = math.sqrt(d2)
        local dirX, dirZ = dx / d, dz / d

        local spd = speed or (self.FlyingChaseSpeed or 1.2)
        local step = spd * dtSec

        -- clamp to avoid teleporting
        local maxStep = 0.25
        if step > maxStep then step = maxStep end

        -- Keep current Y (hover handled separately)
        local _, y, _ = self:GetPosition()
        self:SetPosition(ex + dirX * step, y, ez + dirZ * step)
        self:FaceDirection(dirX, dirZ)
    end,

    ConvertToGroundEnemy = function(self, opts)
        opts = opts or {}
        if not self:IsFlying() then return end

        -- flip profile first
        self.EnemyType = "Ground"

        -- animator (wings close)
        if self._animator then
            self._animator:SetBool("Flying", false)
        end

        -- re-enable gravity on RB (if any)
        if self._rb then
            pcall(function() self._rb.gravityFactor = 1 end)
        end

        -- destroy any leftover controller ptr (should be nil for flying, but safe)
        if self._controller then
            pcall(function() CharacterController.DestroyByEntity(self.entityId) end)
            self._controller = nil
        end

        -- snap to ground (authoritative)
        local x, y, z = self:GetPosition()
        if Nav and Nav.GetGroundY then
            local gy = Nav.GetGroundY(self.entityId)
            if gy ~= nil then y = gy end
        end
        self:SetPosition(x, y, z)

        -- CREATE CC NOW (ground enemies need it)
        if self._collider and self._transform and CharacterController and CharacterController.Create then
            local ok, ctrl = pcall(function()
                return CharacterController.Create(self.entityId, self._collider, self._transform)
            end)
            if ok then
                self._controller = ctrl
                if CharacterController.SetPosition then
                    pcall(function() CharacterController.SetPosition(self._controller, x, y, z) end)
                end
            else
                self._controller = nil
                print("[EnemyAI] ConvertToGroundEnemy: CharacterController.Create failed")
            end
        end

        -- stop movement + clear path
        self._kbT = 0
        self._kbVX, self._kbVZ = 0, 0
        if self.ClearPath then self:ClearPath() end

        -- rebuild states for ground
        self:BuildStateProfile()

        self._justConvertedFromFlying = true

        -- enter requested state (default: Hooked on ground)
        local nextName = opts.nextState or "Hooked"
        local nextState = self.states[nextName] or self.states.Idle
        self.fsm:ForceChange(nextName, nextState)
    end,

    BeginSlamDown = function(self)
        if not self:IsFlying() then return end
        print("[EnemyAI] PULLDOWN")
        self._animator:SetTrigger("Pulldown")

        self._slamActive = true
        self._slamVy = 0
    end,

    UpdateSlamDown = function(self, dtSec)
        if not self._slamActive then return false end
        dtSec = toDtSec(dtSec)
        if dtSec <= 0 then return false end

        local x, y, z = self:GetPosition()
        if x == nil then return false end

        local gy = 0
        -- self._animator:SetBool("Hooked", false)
        if Nav and Nav.GetGroundY then
            local g = Nav.GetGroundY(self.entityId)
            if g ~= nil then gy = g end
        end

        local targetY = gy

        local speed = tonumber(self.SlamDownSpeed) or 3.0
        local newY = y - speed * dtSec

        -- landed
        if newY <= targetY then
            newY = targetY
            self:SetPosition(x, newY, z)

            self._slamActive = false
            self._slamVy = 0
            print("[EnemyAI] SLAMMED")
            self._animator:SetTrigger("Slammed")
            return true
        end

        self:SetPosition(x, newY, z)
        return false
    end,

    ApplyRotation = function(self, w, x, y, z)
        --print(string.format("[ApplyRotation] w=%f, x=%f, y=%f, z=%f", w, x, y, z))
        self._lastFacingRot = { w = w, x = x, y = y, z = z }
        self:SetRotation(w, x, y, z)
    end,

    FaceDirection = function(self, dx, dz)
        --print(string.format("[FaceDirection] dx=%f, dz=%f", dx, dz))
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

    -- Play attack SFX (melee or ranged based on IsMelee flag)
    PlayAttackSFX = function(self)
        if self.IsMelee then
            playRandomSFX(self._audio, self.enemyMeleeAttackSFX)
        else
            playRandomSFX(self._audio, self.enemyRangedAttackSFX)
        end
    end,

    -- Play alert SFX when first detecting player
    PlayAlertSFX = function(self)
        playRandomSFX(self._audio, self.enemyAlertSFX)
    end,

    -- Play hit SFX when attack lands on player (melee or ranged)
    PlayHitSFX = function(self)
        if self.IsMelee then
            playRandomSFX(self._audio, self.enemyMeleeHitSFX)
        else
            playRandomSFX(self._audio, self.enemyRangedHitSFX)
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
        local knives = KnifePool.RequestMany(3)
        if not knives then
            return false
        end

        -- Play ranged attack SFX when throwing knives
        playRandomSFX(self._audio, self.enemyRangedAttackSFX)

        local ex, ey, ez = self:GetPosition()
        -- Safety check: if position is nil, skip spawning
        if ex == nil or ey == nil or ez == nil then return end

        local tr = self._playerTr
        if not tr then
            for i=1,#knives do
                knives[i].reserved = false
                knives[i]._reservedToken = nil
            end
            return false
        end

        -- Unique token per volley
        self._knifeVolleyId = (self._knifeVolleyId or 0) + 1
        local token = tostring(self.entityId) .. ":" .. tostring(self._knifeVolleyId)

        -- stamp token onto reserved knives
        for i=1,3 do
            knives[i]._reservedToken = token
            knives[i].reserved = true
        end

        local ex, ey, ez
        if self._controller then
            local pos = CharacterController.GetPosition(self._controller)
            if pos then ex, ey, ez = pos.x, pos.y, pos.z end
        end
        if not ex then
            ex, ey, ez = self:GetPosition()
        end

        local pp = Engine.GetTransformPosition(tr)
        if not pp then return end
        local px, py, pz = pp[1], pp[2] + 0.5, pp[3]

        local spawnX, spawnY, spawnZ = ex, ey + 1.0, ez

        local dx, dz = (px - ex), (pz - ez)
        local len = math.sqrt(dx*dx + dz*dz)
        if len < 1e-6 then len = 1 end
        dx, dz = dx / len, dz / len

        local rx, rz = -dz, dx
        local spread = 0.6

        local t0x, t0y, t0z = px, py, pz
        local t1x, t1y, t1z = px - rx * spread, py, pz - rz * spread
        local t2x, t2y, t2z = px + rx * spread, py, pz + rz * spread

        local ok1 = knives[1]:Launch(spawnX, spawnY, spawnZ, t0x, t0y, t0z, token, "C")
        local ok2 = knives[2]:Launch(spawnX, spawnY, spawnZ, t1x, t1y, t1z, token, "L")
        local ok3 = knives[3]:Launch(spawnX, spawnY, spawnZ, t2x, t2y, t2z, token, "R")

        if not (ok1 and ok2 and ok3) then
            -- If anything failed, free all three so we never “lose” a side knife.
            for i=1,3 do
                if knives[i] then knives[i]:Reset() end
            end
            -- print(string.format("[EnemyAI] SpawnKnife FAIL tok=%s ok=(%s,%s,%s)",
            --     token, tostring(ok1), tostring(ok2), tostring(ok3)))
            return false
        end

        return true
    end,

    ApplyHit = function(self, dmg, hitType)
        if self.dead then return end
        if (self._hitLockTimer or 0) > 0 then return end
        if self._featherSkillBufferTimer <= 0 then
            self._hurtTriggeredByFeather = false
        end

        -- Feather hits shouldn't apply I-Frame.
        if hitType ~= "FEATHER" then
            self._hitLockTimer = self.config.HitIFrame or 0.1
        end

        self.health = self.health - (dmg or 1)
        print(string.format("[EnemyAI] Remaining health: %d", self.health))
        self:ApplyKnockback(self.KnockbackStrength, self.KnockbackDuration)

        if self.health <= 0 then
            self.health = 0
            -- Play death SFX
            playRandomSFX(self._audio, self.enemyDeathSFX)
            self.fsm:Change("Death", self.states.Death)
            return
        end

        if not self._hurtTriggeredByFeather then
            local myRandomValue = math.random(1, 3)
            if myRandomValue == 1 then
                self._animator:SetBool("Hurt1", true)
            elseif myRandomValue == 2 then
                self._animator:SetBool("Hurt2", true)
            elseif myRandomValue == 3 then
                self._animator:SetBool("Hurt3", true)
            end

            -- Play hurt SFX (only if not dead)
            playRandomSFX(self._audio, self.enemyHurtSFX)

            if self.fsm.currentName == "Hooked" then
                return
            end
            --self._animator:SetBool("Hurt", false)
            self.fsm:ForceChange("Hurt", self.states.Hurt)
        end

        if hitType == "FEATHER" then
            self._featherSkillBufferTimer = self.FeatherSkillBufferDuration
            self._hurtTriggeredByFeather = true
        end
    end,

    ApplyKnockback = function(self, strength, duration)
        if self.dead then return end

        -- Get player position from existing cached transform
        local tr = self._playerTr
        if not tr then
            tr = Engine.FindTransformByName(self.PlayerName)
            self._playerTr = tr
        end
        if not tr then return end

        local pp = Engine.GetTransformPosition(tr)
        if not pp then return end
        local px, pz = pp[1], pp[3]

        -- Enemy position (prefer controller)
        local ex, ez = self:GetEnemyPosXZ()
        if ex == nil or ez == nil then return end

        local dx = ex - px
        local dz = ez - pz
        local len = math.sqrt(dx*dx + dz*dz)
        if len < 0.001 then return end

        dx = dx / len
        dz = dz / len

        local s = tonumber(strength) or self.KnockbackStrength or 0.8
        local d = tonumber(duration) or self.KnockbackDuration or 0.12

        self._kbVX = dx * s
        self._kbVZ = dz * s
        self._kbT  = d
    end,

    ApplyHook = function(self, duration)
        if self.dead then return end
        if duration and duration > 0 then
            self.config.HookedDuration = duration
        end

        if self:IsFlying() then
            -- start slam instead of instant convert
            self:BeginSlamDown()
            -- keep whatever your FlyingHookedState does (animations etc.)
            if self.fsm.currentName ~= "Hooked" then
                self.fsm:Change("Hooked", self.states.Hooked)
            end
            return
        end

        if self.fsm.currentName ~= "Hooked" then
            self.fsm:Change("Hooked", self.states.Hooked)
        end
    end,

    GetHitDirection = function(self)
        -- 1. Get Player Position (Source of the hit)
        local tr = self._playerTr
        if not tr then
            tr = Engine.FindTransformByName(self.PlayerName)
            self._playerTr = tr
        end
        if not tr then return 0, 0, 1 end -- Default forward if player missing

        local pp = Engine.GetTransformPosition(tr)
        if not pp then return 0, 0, 1 end
        local px, py, pz = pp[1], pp[2], pp[3]

        -- 2. Get Enemy Position (Target of the hit)
        local ex, ey, ez = self:GetPosition()
        if not ex then return 0, 0, 1 end

        -- 3. Subtract (Target - Source)
        local dx = ex - px
        local dy = ey - py
        local dz = ez - pz

        -- 4. Normalize (Make length 1.0)
        local lenSq = dx*dx + dy*dy + dz*dz
        if lenSq < 1e-8 or lenSq > 1e12 then 
            return 0, 0, 1 
        end

        local len = math.sqrt(lenSq)
        return dx / len, dy / len, dz / len
    end,

    SpawnFeather = function(self, featherIndex)
        if not self.FeatherPrefabPath then return end
        
        -- Capture data NOW (before next frame)
        local spawnPos = { x=0, y=0, z=0 }
        local x,y,z = self:GetPosition()
        if x then spawnPos = {x=x, y=y, z=z} end
        
        local hx, hy, hz = self:GetHitDirection()
        --print("[EnemyAI] SpawnFeather - GOT HIT DIRECTION")

        -- Stagger slightly
        local delay = featherIndex * 0.02

        if _G.scheduler then
            _G.scheduler.after(delay, function()
                -- 1. Instantiate
                local ent = Prefab.InstantiatePrefab(self.FeatherPrefabPath)

                if ent then
                    -- [FIX] Store data in a global registry keyed by ID
                    -- This survives even if the engine resets the script instance 'self'
                    _G.PendingFeatherData = _G.PendingFeatherData or {}
                    _G.PendingFeatherData[ent] = { x = hx, y = hy, z = hz }
        
                    local featherTr = GetComponent(ent, "Transform")
                    
                    -- 2. Position [FIXED: Don't use a table!]
                    -- Get the existing Vector3D from the new feather
                    local pos = featherTr.localPosition 
                    
                    -- Modify its values
                    pos.x = spawnPos.x
                    pos.y = spawnPos.y + 0.5
                    pos.z = spawnPos.z
                    
                    -- Assign the Vector3D back to the transform
                    featherTr.localPosition = pos
                    featherTr.isDirty = true
                end
            end)
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

        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._damageSub then
                pcall(function()
                    _G.event_bus.unsubscribe(self._damageSub)
                end)
                self._damageSub = nil
            end

            -- NEW: Unsubscribe from combo damage
            if self._comboDamageSub then
                pcall(function()
                    _G.event_bus.unsubscribe(self._comboDamageSub)
                end)
                self._comboDamageSub = nil
            end

            if self._freezeEnemySub then
                pcall(function()
                    _G.event_bus.unsubscribe(self._freezeEnemySub)
                end)
                self._freezeEnemySub = nil
            end

            if self._chainHookSub then
                pcall(function() _G.event_bus.unsubscribe(self._chainHookSub) end)
                self._chainHookSub = nil
            end
        end
        self._frozenBycinematic = false
    end,

    Despawn = function(self)
        if self._despawned then return end
        self._despawned = true

        -- stop AI update first
        self._freezeAI = true

        -- cleanup controller + event subs
        if self.OnDestroy then pcall(function() self:OnDestroy() end) end

        -- hide visuals + disable physics (pool-like safety)
        local model = self:GetComponent("ModelRenderComponent")
        if model then pcall(function() ModelRenderComponent.SetVisible(model, false) end) end

        local col = self:GetComponent("ColliderComponent")
        if col then col.enabled = false end

        local rb = self:GetComponent("RigidBodyComponent")
        if rb then rb.enabled = false end

        -- ACTUAL removal (if entity was created as a duplicate/prefab instance)
        if _G.Engine and _G.Engine.DestroyEntityDup then
            pcall(function()
                _G.Engine.DestroyEntityDup(self.entityId)
            end)
        else
            print("[EnemyAI] Despawn: Engine.DestroyEntityDup not available")
        end
    end,

    SoftDespawn = function(self)
        if self._softDespawned then return end
        self._softDespawned = true

        print("[EnemyAI] SoftDespawn entity=", tostring(self.entityId))

        -- Stop FSM/AI updates from doing anything
        self._freezeAI = true
        self.dead = true

        -- Kill controller safely (prevents pointer crash later)
        if self._controller then
            pcall(function()
                CharacterController.DestroyByEntity(self.entityId)
            end)
            self._controller = nil
        end

        -- Unsubscribe events (reuse your existing cleanup if you want)
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._damageSub then pcall(function() _G.event_bus.unsubscribe(self._damageSub) end) end
            if self._comboDamageSub then pcall(function() _G.event_bus.unsubscribe(self._comboDamageSub) end) end
            self._damageSub, self._comboDamageSub = nil, nil
        end

        -- Hide render
        local model = self:GetComponent("ModelRenderComponent")
        if model then
            pcall(function() ModelRenderComponent.SetVisible(model, false) end)
        end

        -- Disable collider + rigidbody
        local col = self:GetComponent("ColliderComponent")
        if col then col.enabled = false end

        local rb = self:GetComponent("RigidBodyComponent")
        if rb then rb.enabled = false end

        -- If you have an ActiveComponent, disable it too
        local active = self:GetComponent("ActiveComponent")
        if active then
            active.isActive = false
        end
    end,

    OnDestroy = function(self)
        -- Prevent "0xDDDDDDDD" shape pointer crashes on subsequent plays:
        -- Lua must stop calling Update/GetPosition on an old controller pointer.
        if self._controller then
            pcall(function()
                CharacterController.DestroyByEntity(self.entityId)
            end)
            self._controller = nil
        end
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._damageSub then
                pcall(function()
                    _G.event_bus.unsubscribe(self._damageSub)
                end)
                self._damageSub = nil
            end

            if self._comboDamageSub then
                pcall(function()
                    _G.event_bus.unsubscribe(self._comboDamageSub)
                end)
                self._comboDamageSub = nil
            end

            if self._freezeEnemySub then
                pcall(function()
                    _G.event_bus.unsubscribe(self._freezeEnemySub)
                end)
                self._freezeEnemySub = nil
            end

            if self._chainHookSub then
                pcall(function() _G.event_bus.unsubscribe(self._chainHookSub) end)
                self._chainHookSub = nil
            end
        end
    end,
}
