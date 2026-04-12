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

local KnifePool     = require("Gameplay.KnifePool")
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

        -- === Identity ===
        EnemyType  = "Ground",   -- "Ground" or "Flying". Determines state profile and CC usage.
        IsMelee    = false,      -- true = melee attacker; false = ranged (knife) attacker.
        IsPassive  = false,      -- If true enemy never enters Chase/Attack on its own.
        PlayerName = "Player",   -- Engine entity name of the player; used for transform lookup.

        -- === Health ===
        MaxHealth = 5,

        -- === Detection & combat ranges ===
        DetectionRange       = 4.0,   -- Distance at which enemy first notices the player.
        AttackRange          = 3.0,   -- Distance at which ranged attack is allowed to fire.
        AttackDisengageRange = 4.0,   -- Distance at which enemy exits the attack state. Must be >= AttackRange.
        AttackCooldown       = 3.0,   -- Seconds between attack volleys.
        RangedAnimDelay      = 1.0,   -- Seconds into the attack animation before the projectile fires.
        MeleeAnimDelay       = 1.2,   -- Seconds into the melee animation before the hit registers.

        -- === Melee combat ===
        MeleeSpeed          = 0.9,   -- Movement speed while charging a melee attack (world units/sec).
        MeleeRange          = 1.2,   -- Distance at which melee hit registers.
        MeleeDamage         = 3,     -- Damage dealt per melee swing.
        MeleeAttackCooldown = 5.0,   -- Seconds between melee swings.
        LiftRange           = 2.5,   -- Max distance from player at which a lift attack connects.
                                     -- Slightly wider than MeleeRange to compensate for the
                                     -- upward-sweep animation that bypasses trigger overlap.
        SlamRange           = 3.5,   -- Max distance at which a slam_attack_active broadcast connects.
                                     -- Should match or slightly exceed SlamProximityRange in AttackHitbox.

        -- Shockwave applied to grounded enemies when the player lands a slam.
        SlamLandingRadius      = 4.5,   -- World-unit radius of the shockwave.
        SlamLandingKBStrength  = 10.0,  -- Knockback impulse magnitude for shockwave victims.
        SlamLandingKBDuration  = 0.35,  -- Seconds the shockwave KB velocity is integrated.

        -- === Hit response ===
        HurtDuration      = 2.0,    -- Seconds spent in the Hurt state after taking a hit.
        HitIFrame         = 0.05,    -- Invincibility window after a hit (seconds). Prevents hit-stacking.
        KnockbackStrength = 5.0,    -- FALLBACK only. Per-attack knockback comes from ComboManager's
                                    -- attack_performed payload (light_1=20, lift=80, air_slam=180, etc).
                                    -- This value is used only when no knockback is in the hit payload
                                    -- (e.g. chain hits, debug hits via Keyboard.IsDigitPressed).
        KnockbackDuration = 0.35,   -- Seconds the knockback velocity is applied.

        -- === Chain hook ===
        HookedDuration     = 4.0,     -- Seconds the enemy stays hooked before breaking free.
        HookStopDistance   = 1.2,     -- Pull stops when enemy is within this radius of the player.
        HookStaggerTime    = 1.0,     -- Duration of the stagger phase during a hook pull.
        HookStaggerSpeed   = 50.0,    -- Stagger pull speed (world units/sec).
        HookStaggerMaxStep = 25.0,    -- Max step size per tick during stagger pull.
        HookHardSpeed      = 1200.0,  -- Hard pull speed after stagger phase ends.
        HookHardMaxStep    = 600.0,   -- Max step size per tick during hard pull.
        HookedLandingDelay = 5.0,     -- Seconds before a slammed flying enemy can stand up.

        -- === Patrol & movement ===
        EnablePatrol   = true,   -- If false, enemy stands idle instead of patrolling.
        PatrolSpeed    = 0.3,    -- Movement speed while patrolling (world units/sec).
        PatrolDistance = 3.0,    -- Max distance from spawn that patrol points are placed.
        PatrolWait     = 1.5,    -- Seconds the enemy pauses at each patrol point.
        ChaseSpeed     = 0.6,    -- Movement speed while chasing the player (world units/sec).

        -- Pre-placed patrol endpoints. Set in the editor for each enemy instance.
        PatrolPointA_X =  2.0,
        PatrolPointA_Z = -1.1,
        PatrolPointB_X = -2.0,
        PatrolPointB_Z = -1.1,

        -- === Pathfinding ===
        PathRepathInterval    = 10,    -- Seconds between automatic path recalculations.
        PathGoalMoveThreshold = 0.9,   -- World units the goal must move before forcing a repath.
        PathWaypointRadius    = 0.6,   -- Distance to a waypoint considered "arrived" (world units).
        PathStuckTime         = 0.75,  -- Seconds of zero movement before stuck recovery triggers.

        -- === Flying ===
        HoverHeight      = 2.0,    -- Target height above ground while hovering (world units).
        HoverSnapSpeed   = 8.0,    -- Lerp speed snapping Y toward hover target. Higher = snappier.
        FlyingChaseSpeed = 0.8,    -- XZ movement speed while flying toward player (world units/sec).
        HoverBobAmp      = 0.02,   -- Amplitude of the idle hover bob (world units). 0 = no bob.
        HoverBobFreq     = 0.9,    -- Frequency of the idle hover bob (Hz).
        SlamDownSpeed    = 16.0,   -- Descent speed during the chain-hook slam-down (world units/sec).

        -- === Squash & stretch ===
        -- SquashStrength : how dramatic the effect is. Start here.
        --                  0.0 = disabled   0.3 = subtle   1.0 = cartoony
        --                  Scales all events — hit, death, slam landing.
        -- SquashDuration : seconds to hit peak squash. Match to impact frame.
        -- StretchDuration: seconds to spring back to normal.
        SquashStrength  = 0.6,   -- 0.4 was too subtle to read at 0.6 hit intensity. Effective = SquashStrength * intensity.
        SquashDuration  = 0.07,
        StretchDuration = 0.20,

        -- === Aerial juggle ===
        -- When the player lands a LIFT hit the enemy becomes airborne and is
        -- held up by subsequent AIR hits. A SLAM hit drives them into the floor.
        JuggleLiftVelY   = 2.0,   -- Upward velocity applied on lift_attack hit.
        JuggleAirBoost   = 4.0,   -- Upward velocity boost per air-combo hit to maintain juggle height.
        JuggleSlamVelY   = 20.0,  -- Downward velocity applied on air_slam hit.
        JuggleSlamKBStrength = 12.0,  -- Lateral knockback speed applied on SLAM (world units/sec).
        JuggleSlamKBDuration = 0.25,  -- Seconds the lateral KB velocity is integrated.
        JuggleGravity    = -18.0, -- Gravity while juggled (separate from world gravity).
        JuggleGroundEps  = 0.05,  -- Distance to ground considered "landed".

        -- Upward refresh applied to a _isKnockedUp enemy on an AIR hit.
        -- Deliberately small — this keeps them airborne for juggling without
        -- launching them as high as a LIFT hit. Tune between 2.5–5.0.
        -- DO NOT use LIFT knockback here (ComboManager lift=80 → flies into sky).
        AirHitKnockupBoost    = 1,
        -- XZ knockback speed applied to a knocked-up enemy on each AIR hit,
        -- pushing them away from the player so they drift around mid-air.
        AirHitKnockupKBStrength  = 10.0,
        AirHitKnockupKBDuration  = 0.18,

        -- === Kinematic grounding ===
        UseKinematicGrounding = true,
        GroundRayUp   = 0.35,    -- Ray cast origin height above feet.
        GroundRayLen  = 3.0,     -- Maximum downward ray length.
        GroundEps     = 0.08,    -- Considered grounded when hit distance <= GroundRayUp + GroundEps.
        GroundSnapMax = 0.35,    -- Maximum snap correction applied per step.
        Gravity       = -9.81,
        MaxFallSpeed  = -25.0,
        WallRayUp     = 0.8,     -- Wall probe ray origin height above feet.
        WallSkin      = 0.18,    -- Wall detection margin (world units).
        WallOffset    = 0.08,    -- Separation buffer pushed away from wall surface.
        MinStep       = 0.01,    -- Minimum movement step before the CC is called.

        -- === Abilities / skills ===
        FeatherSkillBufferDuration   = 0.2,   -- Window (seconds) after a feather hit during which further
                                              -- feather hits don't re-trigger Hurt FSM state.
        FeatherPrefabPath            = "Resources/Prefabs/Feather.prefab",
        NumFeathersSpawnedPerHit     = 5,     -- Feather particles spawned per hit from the feather skill.

        -- === Animation clips ===
        -- Clip index integers assigned in the editor.
        ClipIdle   = 0,
        ClipAttack = 1,
        ClipHurt   = 2,
        ClipDeath  = 3,

        -- === Audio ===
        -- SFX clips are configured on EnemyAIAudio (sibling script component).
        -- Footstep intervals stay here because they control movement-state timing.
        PatrolFootstepInterval    = 0.5,   -- Seconds between footstep sounds during patrol (WalkPatrol anim).
        ChaseFootstepInterval     = 0.35,  -- Seconds between footstep sounds during chase (Chase anim).
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

    end,

    Start = function(self)
        self._animator  = self:GetComponent("AnimationComponent")
        self._collider  = self:GetComponent("ColliderComponent")
        self._transform = self:GetComponent("Transform")
        self._rb        = self:GetComponent("RigidBodyComponent")
        self.particles  = self:GetComponent("ParticleComponent")

        self._entityName = Engine.GetEntityName(self.entityId)
        self._featherEntities = {}

        if self._controller then
            self:RemoveCharacterController()
        end

        if self._animator then
            --print("[PlayerMovement] Animator found, playing IDLE clip")
            --self._animator:PlayClip(IDLE, true)
        else
            --print("[PlayerMovement] ERROR: Animator is nil!")
        end

        self._animator:SetBool("PatrolEnabled", self.EnablePatrol)
        self._animator:SetBool("Passive", self.IsPassive)
        self._animator:SetBool("Melee", self.IsMelee)
        self._animator:SetBool("Flying", self:IsFlying())

        if not self:IsFlying() then
            self:CreateCharacterController()
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

        -- ── Squash & stretch ──────────────────────────────────────────────────
        -- _squashPhase: "squash" → peak → "stretch" → springs back → nil
        -- Call self:_squashTrigger(mode, intensity) to start an effect.
        self._squashPhase     = nil
        self._squashTimer     = 0
        self._squashIntensity = 1.0
        self._squashMode      = "vertical"

        -- ── Aerial juggle state ──────────────────────────────────────────────
        -- [v2 FIX] _juggleY removed. CC owns Y entirely in juggle mode.
        -- _juggleVY is fed to CC each frame via SetJuggleMode; gravity is
        -- accumulated here in Lua and passed down every Update tick.
        self._isJuggled     = false -- true while enemy is airborne from a juggle
        self._juggleVY      = 0     -- current vertical velocity (fed to CC each frame)
        self._juggleGroundY = nil   -- cached ground Y to detect landing
        self._juggleAirTime = 0     -- seconds airborne; gates the landing check

        -- Knockup system (separate from juggle)
        self._isKnockedUp = false
        self._knockupVY = 0
        self._knockupGravity = -4.5
        self._knockupRecoverT = 0
        self._knockupJustLanded = false

        self.aggressive = self.aggressive or false
        self.AlertRadiusOnHit = self.AlertRadiusOnHit or 10.0
        self._lastAlertTime = 0
        self._alertCooldown = 0.20

        -- Capture authored scale once so the effect always springs back to the
        -- editor-set proportions rather than a hardcoded 1,1,1.
        local _initScale      = self._transform and self._transform.localScale
        self._normalScaleX    = (_initScale and _initScale.x) or 1.0
        self._normalScaleY    = (_initScale and _initScale.y) or 1.0
        self._normalScaleZ    = (_initScale and _initScale.z) or 1.0

        -- === Damage event subscription ===
        self._damageSub = nil
        if _G.event_bus and _G.event_bus.subscribe then
            self._damageSub = _G.event_bus.subscribe("enemy_damage", function(payload)
                if not payload then return end

                -- Correct key: DamageZone sends payload.entityId
                if payload.entityId ~= nil and payload.entityId ~= self.entityId then
                    return
                end

                local dmg      = payload.dmg or 1
                local hitType  = string.upper(payload.hitType or payload.src or "MELEE")
                local knockback = payload.knockback  -- may be nil; ApplyHit will fall back to KnockbackStrength
                self:ApplyHit(dmg, hitType, knockback)
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
                if playerDead then
                    self:OnPlayerDied({ dead = true })
                end
            end)

            self._comboDamageSub = _G.event_bus.subscribe("deal_damage_to_entity", function(payload)
                if not payload then return end
                if payload.entityId ~= self.entityId then
                    return
                end

                local damage    = payload.damage or 10
                local hitType   = string.upper(payload.hitType or "COMBO")
                local knockback = payload.knockback  -- may be nil; ApplyHit will fall back to KnockbackStrength
                self:ApplyHit(damage, hitType, knockback)
            end)

            -- LIFT proximity detection:
            -- AttackHitbox publishes this when a lift window opens. We self-check
            -- distance because the upward-sweep animation means OnTriggerEnter
            -- may never fire against a grounded enemy.
            self._liftAttackSub = _G.event_bus.subscribe("lift_attack_active", function(payload)
                if self.dead then return end
                if self:IsFlying() then return end
                if self._isKnockedUp then return end
                if self._isJuggled then return end

                local range = tonumber(self.LiftRange) or 2.5
                local distSq = self:GetPlayerDistanceSq()
                if distSq <= range * range then
                    local dmg = (payload and payload.damage)    or 15
                    local kb  = (payload and payload.knockback) or 0
                    --print(string.format("[EnemyAI] lift_attack_active proximity HIT: entity=%s distSq=%.2f", tostring(self.entityId), distSq))
                    self:ApplyHit(dmg, "LIFT", kb)
                end
            end)

            -- SLAM proximity detection:
            -- AttackHitbox publishes this when a slam window opens. Player dives
            -- through enemies so OnTriggerEnter is unreliable — same fix as LIFT.
            -- Only airborne enemies (_isJuggled or _isKnockedUp) are eligible;
            -- grounded enemies are handled by the slam_landed shockwave below.
            self._slamAttackSub = _G.event_bus.subscribe("slam_attack_active", function(payload)
                if self.dead then return end
                if self:IsFlying() then return end
                if not self._isJuggled and not self._isKnockedUp then return end

                local range = tonumber(payload and payload.range or self.SlamRange) or 3.5
                local distSq = self:GetPlayerDistanceSq()
                if distSq <= range * range then
                    local dmg = (payload and payload.damage)    or 15
                    local kb  = (payload and payload.knockback) or 0
                    self:ApplyHit(dmg, "SLAM", kb)
                end
            end)

            -- Slam landing shockwave:
            -- When the player hits the ground after a slam, knock back all nearby
            -- grounded enemies. Airborne enemies are unaffected (already being juggled).
            self._slamLandedSub = _G.event_bus.subscribe("slam_landed", function()
                if self.dead then return end
                if self:IsFlying() then return end
                if self._isJuggled or self._isKnockedUp then return end

                local radius = tonumber(self.SlamLandingRadius) or 4.5
                local distSq = self:GetPlayerDistanceSq()
                if distSq > radius * radius then return end

                -- Direction: push enemy away from the player's landing position.
                local kbStr = tonumber(self.SlamLandingKBStrength) or 10.0
                local kbDur = tonumber(self.SlamLandingKBDuration) or 0.35
                local tr = self._playerTr
                if not tr then tr = Engine.FindTransformByName(self.PlayerName) end
                if tr then
                    local pp = Engine.GetTransformPosition(tr)
                    if pp then
                        local ex, ez = self:GetEnemyPosXZ()
                        if ex and ez then
                            local dx = ex - pp[1]
                            local dz = ez - pp[3]
                            local len = math.sqrt(dx * dx + dz * dz)
                            if len >= 0.001 then
                                self._kbVX = (dx / len) * kbStr
                                self._kbVZ = (dz / len) * kbStr
                                self._kbT  = kbDur
                                self.fsm:ForceChange("Hurt", self.states.Hurt)
                            end
                        end
                    end
                end
            end)

            self._chainEndpointHitSub = _G.event_bus.subscribe("chain.endpoint_hit_entity", function(payload)
                if not payload then return end
                if payload.rootName ~= self._entityName then return end
                --print("[EnemyAI] chain.endpoint_hit_entity received")
                self._animator:SetTrigger("Hooked")
                -- Tell the chain UI which kind of target was just hooked so the
                -- chain icon can switch to Pull (grounded) or Slam (flying)
                -- immediately, not only after the player presses again.
                if _G.event_bus and _G.event_bus.publish then
                    _G.event_bus.publish("chain.hooked_target_type", {
                        entityId = self.entityId,
                        isFlying = self:IsFlying(),
                    })
                end
            end)

            self._chainHookSub = _G.event_bus.subscribe("chain.enemy_hooked", function(payload)
                if not payload then return end
                if payload.entityId ~= self.entityId then return end
                --print("[EnemyAI] chain.enemy_hooked received — calling ApplyHook duration=" .. tostring(payload.duration))
                pcall(function() self:ApplyHook(payload.duration) end)
            end)

            self._onEnemyAlertSub = _G.event_bus.subscribe("enemy_alert", function(msg)
                self:OnEnemyAlert(msg)
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
        --print("[EnemyAI] Patrol points set")

        self._patrolWhich = 2
        self._patrolTarget = self._patrolB

        self._patrolWaitT = self.config.PatrolWait
        self._isPatrolWait = false

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

        -- Dead flying enemies: fall to the ground during death animation.
        if self.dead and self:IsFlying() and not self._deathLanded then
            local dtF = dt or 0
            if dtF > 1.0 then dtF = dtF * 0.001 end
            if dtF > 0.05 then dtF = 0.05 end
            if dtF > 0 then
                self._deathFallVel = (self._deathFallVel or 0) + 12.0 * dtF
                local x, y, z = self:GetPosition()
                local groundY = self._spawnY or 0
                if Nav and Nav.GetGroundY then
                    local gy = Nav.GetGroundY(self.entityId)
                    if gy then groundY = gy end
                end
                local newY = y - self._deathFallVel * dtF
                if newY <= groundY then
                    newY = groundY
                    self._deathLanded = true
                end
                self:SetPosition(x, newY, z)
            end
        end

        -- When player respawns, teleport the enemy back to its initial position.
        if self._playerRespawned and self._initialPos then
            self:SetPosition(self._initialPos.x, self._initialPos.y + 1.0, self._initialPos.z)
            if self._controller and CharacterController.SetPosition then
                pcall(function()
                    CharacterController.SetPosition(self._controller, self._transform)
                end)
            end
            --print(string.format("[EnemyAI] Teleported enemy %d to %f %f %f", self.entityId, self._initialPos.x, self._initialPos.y, self._initialPos.z))
            self.fsm:ForceChange("Idle", self.states.Idle)

            self._playerRespawned = false
            self._playerDead = false
            return
        end

        self._motionID = self._rb and self._rb.motionID or nil

        if Input.IsActionPressed("Interact") then
            self:ApplyHook(self.HookedDuration)
        end

        if Keyboard.IsDigitPressed(1) then
            self:ApplyHook(self.HookedDuration)
        end
        if Keyboard.IsDigitPressed(3) then
            self:ApplyHit(10)
        end
        if Keyboard.IsDigitPressed(7) then
            self.IsPassive = not self.IsPassive
        end

        if Keyboard.IsDigitPressed(9) and not self:IsFlying() then
            self:ApplyHit(1, "KNOCKUP", 0)
        end

        local dtSec = toDtSec(dt)
        self._hitLockTimer = math.max(0, (self._hitLockTimer or 0) - dtSec)
        self._featherSkillBufferTimer = math.max(0, (self._featherSkillBufferTimer or 0) - dtSec)

        -- ── Squash & stretch ──────────────────────────────────────────────────
        -- Modes:
        --   "vertical"  : slam landing / death collapse — Y squashes down, XZ widens
        --   "horizontal": hit reaction — Y pops up, XZ squashes in
        if self._squashPhase and self._transform then
            local scaleTable = self._transform.localScale
            if scaleTable then
                self._squashTimer = self._squashTimer + dtSec
                local i    = (self.SquashStrength or 0.4) * (self._squashIntensity or 1.0)
                local mode = self._squashMode or "vertical"

                -- Rest scale: use the authored transform scale captured in Start,
                -- not a hardcoded 1,1,1, so the effect springs back correctly for
                -- enemies whose scale differs from the default.
                local nX = self._normalScaleX or 1.0
                local nY = self._normalScaleY or 1.0
                local nZ = self._normalScaleZ or 1.0
                local sx, sy, sz = nX, nY, nZ

                local yPeak, xzPeak, yOver
                if mode == "vertical" then
                    yPeak  = nY * (1.0 - 0.30 * i)
                    xzPeak = nX * (1.0 + 0.18 * i)
                    yOver  = nY * (1.0 + 0.08 * i)
                elseif mode == "horizontal" then
                    yPeak  = nY * (1.0 + 0.08 * i)
                    xzPeak = nX * (1.0 - 0.10 * i)
                    yOver  = nY
                end

                if self._squashPhase == "squash" then
                    local t = math.min(self._squashTimer / math.max(self.SquashDuration or 0.07, 1e-4), 1.0)
                    sy = nY + (yPeak  - nY) * t
                    sx = nX + (xzPeak - nX) * t
                    sz = nZ + (xzPeak - nX) * t   -- Z tracks X offset (uniform XZ)
                    if t >= 1.0 then
                        self._squashPhase = "stretch"
                        self._squashTimer = 0
                    end

                elseif self._squashPhase == "stretch" then
                    local t = math.min(self._squashTimer / math.max(self.StretchDuration or 0.20, 1e-4), 1.0)
                    if t < 0.5 then
                        local t2 = t * 2
                        sy = yPeak  + (yOver - yPeak)  * t2
                        sx = xzPeak + (nX    - xzPeak) * t2
                    else
                        local t2 = (t - 0.5) * 2
                        sy = yOver + (nY - yOver) * t2
                        sx = nX
                    end
                    sz = nZ + (sx - nX)   -- Z tracks X offset (uniform XZ)
                    if t >= 1.0 then
                        sx, sy, sz        = nX, nY, nZ
                        self._squashPhase = nil
                    end
                end

                pcall(function()
                    scaleTable.x = sx
                    scaleTable.y = sy
                    scaleTable.z = sz
                    self._transform.isDirty = true
                end)
            end
        end

        -- ── Aerial juggle physics [v5] ───────────────────────────────────────
        -- CC destroyed on lift start, recreated on landing.
        -- KEY FIXES:
        --   1. Position written BEFORE gravity is subtracted (move-then-integrate).
        --      Enemy always moves UP on frame 1 — gravity can't kill the arc early.
        --   2. Landing check gated behind: (a) minimum airtime 0.1s AND
        --      (b) past the arc peak (_juggleVY < 0, i.e. falling).
        --      Both conditions together make spurious first-frame landings impossible.
        if self._isJuggled then
            local juggleDt = (Time and Time.GetUnscaledDeltaTime and Time.GetUnscaledDeltaTime()) or dtSec
            juggleDt = toDtSec(juggleDt)

            -- Accumulate airtime for the minimum-airtime gate.
            self._juggleAirTime = (self._juggleAirTime or 0) + juggleDt

            -- Move FIRST (semi-implicit Euler: x += v*dt, then v += a*dt).
            -- On frame 1 _juggleVY is the full positive lift velocity, so enemy
            -- always moves upward before gravity gets a chance to reduce velocity.
            self._juggleY = (self._juggleY or 0) + (self._juggleVY or 0) * juggleDt

            -- Integrate gravity for next frame.
            local grav = tonumber(self.JuggleGravity) or -18.0
            self._juggleVY = (self._juggleVY or 0) + grav * juggleDt

            -- Frozen ground reference (set once in ApplyJuggleLift, never updated mid-arc).
            local groundY = self._juggleGroundY or 0

            local x, _, z = self:GetPosition()
            if x ~= nil then
                local minAirTime = 0.1
                local isFalling  = (self._juggleVY or 0) < 0
                if self._juggleAirTime >= minAirTime and isFalling
                    and self._juggleY <= groundY + (tonumber(self.JuggleGroundEps) or 0.05) then

                    -- Confirmed landing.
                    self._juggleY       = groundY
                    self._juggleVY      = 0
                    self._isJuggled     = false
                    self._juggleAirTime = 0
                    self:SetPosition(x, self._juggleY, z)

                    -- Re-enable RigidBody before recreating CC (disabled in ApplyJuggleLift).
                    if self._rb then
                        pcall(function() self._rb.enabled = true end)
                        pcall(function() self._rb.linearVel = { x=0, y=0, z=0 } end)
                        pcall(function() self._rb.impulseApplied = { x=0, y=0, z=0 } end)
                    end

                    -- Recreate CC.
                    if self._collider and self._transform and CharacterController and CharacterController.Create then
                        local ctrl = self:CreateCharacterController()
                        if not ctrl then
                            --print("[EnemyAI] Juggle land: CC recreate failed")
                        end
                    end
                    self:_squashTrigger("vertical", 0.8)
                    if self._animator then
                        self._animator:SetBool("Hurt1", false)
                        self._animator:SetBool("Hurt2", false)
                        self._animator:SetBool("Hurt3", false)
                    end
                    self.fsm:ForceChange("Hurt", self.states.Hurt)
                else
                    -- Airborne: raw SetPosition.
                    self:SetPosition(x, self._juggleY, z)
                end
            end
        end

        -- ── Knockup physics ─────────────────────────────
        if self._isKnockedUp then
            local dtSec = toDtSec(dt)

            -- Move first
            self._knockupY = self._knockupY + self._knockupVY * dtSec

            -- Apply gravity
            self._knockupVY = self._knockupVY + self._knockupGravity * dtSec

            local x, _, z = self:GetPosition()

            -- XZ drift from air hits: CC is destroyed during knockup so the main
            -- KB block's CharacterController.Move silently fails. Consume _kbT here
            -- and integrate XZ directly into the SetPosition call below.
            if (self._kbT or 0) > 0 then
                self._kbT = self._kbT - dtSec
                if self._kbT < 0 then self._kbT = 0 end
                local mx = (self._kbVX or 0) * dtSec
                local mz = (self._kbVZ or 0) * dtSec
                local maxStep = 0.5
                if mx >  maxStep then mx =  maxStep end
                if mx < -maxStep then mx = -maxStep end
                if mz >  maxStep then mz =  maxStep end
                if mz < -maxStep then mz = -maxStep end
                x = x + mx
                z = z + mz
            end

            -- Landing check
            if self._knockupY <= self._knockupGroundY then
                self._knockupY = self._knockupGroundY
                self._knockupVY = 0
                self._isKnockedUp = false

                self:SetPosition(x, self._knockupY, z)

                if self._rb then
                    pcall(function() self._rb.enabled = true end)
                    pcall(function() self._rb.linearVel = { x=0, y=0, z=0 } end)
                    pcall(function() self._rb.impulseApplied = { x=0, y=0, z=0 } end)
                end

                local ctrl = self:CreateCharacterController()
                if not ctrl then
                    --print("[EnemyAI] Knockup land: CC recreate failed")
                end

                self:_squashTrigger("vertical", 0.7)

                self:ClearPath()
                self:StopCC()

                -- IMPORTANT: clear stale attack intent again on landing
                --self:_ResetCombatAnimatorParams()

                -- landing hurt
                self:_PlayRandomHurtAnim()

                -- hold briefly on landing
                self._knockupRecoverT = 0.35
                self._knockupJustLanded = true

                -- keep logic in Hurt during landing recovery
                self.fsm:ForceChange("Hurt", self.states.Hurt)
            else
                self:SetPosition(x, self._knockupY, z)
            end
        end

        if self._knockupRecoverT and self._knockupRecoverT > 0 then
            self._knockupRecoverT = math.max(0, self._knockupRecoverT - dtSec)
            self:ClearPath()
            self:StopCC()

            if self._knockupRecoverT <= 0 then
                --self:_ResetCombatAnimatorParams()
                self:ClearPath()
                self:StopCC()
                self.fsm:ForceChange("Idle", self.states.Idle)
                self._knockupJustLanded = false
            end
        elseif self._isKnockedUp then
            self:ClearPath()
            self:StopCC()
        else
            if not self.fsm.current or not self.fsm.currentName then
                self.fsm:ForceChange("Idle", self.states.Idle)
            end
            self.fsm:Update(dtSec)
        end

        -- flying hook slam-down sequence
        if self:IsFlying() and self._slamActive then
            local landed = self:UpdateSlamDown(dtSec)
            if landed then
                -- once we hit the ground, convert + start ground hooked pull
                self:ConvertToGroundEnemy({ nextState = "Hooked" })
            end
        end

        -- Apply knockback movement regardless of current FSM state.
        -- SKIP when _isKnockedUp: the knockup block above already consumed _kbT
        -- and integrated XZ into SetPosition. CharacterController is nil during
        -- knockup anyway, so this branch would silently no-op on ground enemies.
        if self._kbT and self._kbT > 0 and not self._isKnockedUp then
            self._kbT = self._kbT - dtSec
            if self._kbT < 0 then self._kbT = 0 end

            if self:IsFlying() then
                -- Flying: we drive the transform directly, so dt-scale here is correct.
                local mx = (self._kbVX or 0) * dtSec
                local mz = (self._kbVZ or 0) * dtSec
                -- clamp per-frame displacement so it never teleports
                local maxStep = 0.20
                if mx >  maxStep then mx =  maxStep end
                if mx < -maxStep then mx = -maxStep end
                if mz >  maxStep then mz =  maxStep end
                if mz < -maxStep then mz = -maxStep end
                local x, y, z = self:GetPosition()
                local nx, nz = x + mx, z + mz
                self:SetPosition(nx, y, nz)
                if self._controller and CharacterController.SetPosition then
                    pcall(function()
                        CharacterController.SetPosition(self._controller, nx, y, nz)
                    end)
                end
            else
                -- FIX: CC.Move stores mVelocity; CharacterController::Update then calls
                -- ExtendedUpdate(deltaTime, ...) which multiplies velocity by dt internally.
                -- Passing a dt-pre-scaled displacement here caused double-dt scaling,
                -- making knockback, lift, and slam effectively invisible (force ≈ 1/60th
                -- of intended). Pass raw velocity (units/sec) — no dt scaling in Lua.
                CharacterController.Move(self._controller, self._kbVX or 0, 0, self._kbVZ or 0)
            end
        end

        if not _G.__CC_UPDATED_THIS_FRAME then
            _G.__CC_UPDATED_THIS_FRAME = true
            --CharacterController.UpdateAll(dtSec)
        end

        -- ── Sync Transform from CC [v3] ──────────────────────────────────────
        -- During juggle CC is nil (destroyed in ApplyJuggleLift, recreated on
        -- landing). Position is already written above by the juggle block.
        if self._controller then
            local pos = CharacterController.GetPosition(self._controller)
            if pos then
                if self._isJuggled then
                    -- Should not reach here (CC is nil while juggling) but safe fallback.
                    self:SetPosition(pos.x, self._juggleY or pos.y, pos.z)
                elseif not self:IsFlying() then
                    local groundY = (Nav and Nav.GetGroundY) and Nav.GetGroundY(self.entityId) or pos.y
                    self:SetPosition(pos.x, groundY, pos.z)
                else
                    local x, y, z = self:GetPosition()
                    if x ~= nil then self:SetPosition(pos.x, y, pos.z) end
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

    SetCharacterControllerEnabled = function(self, enabled)
        if enabled then
            self:CreateCharacterController()
        else
            self:RemoveCharacterController()
        end
    end,

    CreateCharacterController = function(self)
        if self._controller then return self._controller end
        if not self._collider or not self._transform then return nil end

        local ok, ctrl = pcall(function()
            return CharacterController.Create(self.entityId, self._collider, self._transform)
        end)

        if ok and ctrl then
            self._controller = ctrl

            pcall(function()
                CharacterController.SetImmovable(self.entityId, true)
            end)

            -- IMPORTANT: sync using explicit coordinates, not transform object
            local x, y, z = self:GetPosition()
            if x ~= nil and CharacterController.SetPosition then
                pcall(function()
                    CharacterController.SetPosition(self._controller, x, y, z)
                end)
            end

            return ctrl
        end

        --print("[EnemyAI] CreateCharacterController FAILED")
        self._controller = nil
        return nil
    end,

    RemoveCharacterController = function(self)
        if not self._controller then return end

        pcall(function()
            CharacterController.DestroyByEntity(self.entityId)
        end)

        self._controller = nil

        -- Optional safety: kill any movement residue
        if self._rb then
            pcall(function() self._rb.linearVel = { x=0, y=0, z=0 } end)
            pcall(function() self._rb.impulseApplied = { x=0, y=0, z=0 } end)
        end
    end,

    _PlayRandomHurtAnim = function(self)
        if not self._animator then return end

        -- clear first so we never stack stale bools
        self._animator:SetBool("Hurt1", false)
        self._animator:SetBool("Hurt2", false)
        self._animator:SetBool("Hurt3", false)

        local r = math.random(1, 3)
        if r == 1 then
            self._animator:SetBool("Hurt1", true)
        elseif r == 2 then
            self._animator:SetBool("Hurt2", true)
        else
            self._animator:SetBool("Hurt3", true)
        end
    end,

    _ClearHurtAnims = function(self)
        if not self._animator then return end
        --print("[EnemyAI] _ClearHurtAnims Animator:SetBool(Hurt, false)")
        self._animator:SetBool("Hurt1", false)
        self._animator:SetBool("Hurt2", false)
        self._animator:SetBool("Hurt3", false)
    end,

    _ResetCombatAnimatorParams = function(self)
        if not self._animator then return end

        -- generic combat bools
        self._animator:SetBool("PlayerInAttackRange", false)
        self._animator:SetBool("PlayerInDetectionRange", false)
        self._animator:SetBool("ReadyToAttack", false)

        -- clear hurt bools too; we will re-apply the one we want explicitly
        --print("[EnemyAI] _ResetCombatAnimatorParams Animator:SetBool(Hurt, false)")
        self._animator:SetBool("Hurt1", false)
        self._animator:SetBool("Hurt2", false)
        self._animator:SetBool("Hurt3", false)

        -- if your animator supports triggers, clear the common ones used by ground enemies
        pcall(function() self._animator:ResetTrigger("Melee") end)
        pcall(function() self._animator:ResetTrigger("Ranged") end)
        pcall(function() self._animator:ResetTrigger("Hooked") end)
    end,

    CancelPendingAttack = function(self, reason)
        self._attackCancelled = true
        self._attackCancelReason = reason or "INTERRUPTED"
        self._attackToken = (self._attackToken or 0) + 1

        -- also clear obvious combat intent so animation/state doesn't keep arming
        self.attackTimer = 0
        --self:_ResetCombatAnimatorParams()
        self:StopCC()
    end,

    BeginAttackWindow = function(self)
        self._attackCancelled = false
        self._attackCancelReason = nil
        self._attackToken = (self._attackToken or 0) + 1
        return self._attackToken
    end,

    IsAttackWindowValid = function(self, token)
        if self.dead then return false end
        if self._isKnockedUp then return false end
        if self._knockupRecoverT and self._knockupRecoverT > 0 then return false end
        if self._attackCancelled then return false end
        if token ~= nil and token ~= self._attackToken then return false end
        if self.fsm.currentName == "Hurt" then return false end
        if self.fsm.currentName == "Hooked" then return false end
        return true
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
        if self._isKnockedUp then return end
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

        -- During juggle: CC is nil (v3). Drive XZ via SetPosition directly.
        if self._isJuggled then
            local x, _, z = self:GetPosition()
            if x ~= nil then
                local mx = (vx or 0) * (dt or 0)
                local mz = (vz or 0) * (dt or 0)
                -- Y comes from _juggleY, not current transform Y (which lags one frame).
                self:SetPosition(x + mx, self._juggleY or _, z + mz)
            end
            return
        end

        if not self._controller then
            self._dbgNoCCT = (self._dbgNoCCT or 0) + (dt or 0)
            if self._dbgNoCCT > 1.0 then
                self._dbgNoCCT = 0
                --print("[EnemyAI] MoveCC called but _controller is NIL")
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
        -- During juggle: skipping CC.Move prevents the CC's internal gravity from
        -- snapping the enemy back to the floor on the same frame juggle sets Y.
        if self._isJuggled then return end
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
            --print("[Nav] ERROR: Nav is NIL! Nav system not bound to Lua!")
            self:ClearPath()
            return false
        end
        
        if not Nav.RequestPathXZ then
            --print("[Nav] ERROR: Nav.RequestPathXZ is NIL! Function not bound!")
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
        t = string.lower(string.gsub(t, '[\"%s]+', ""))
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

        -- Smooth ground Y to prevent jitter at nav-cell boundaries
        if self._smoothGroundY == nil then
            self._smoothGroundY = gy
        else
            local alpha = math.min(1.0, 4.0 * dtSec)  -- ~4 Hz smoothing
            self._smoothGroundY = self._smoothGroundY + (gy - self._smoothGroundY) * alpha
        end
        gy = self._smoothGroundY

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

        -- Wall collision: raycast in movement direction before moving
        if Physics and Physics.Raycast then
            local _, ey, _ = self:GetPosition()
            local castY = ey  -- cast at hover height
            local wallMargin = 0.4
            local hitDist = Physics.Raycast(
                ex, castY, ez,
                dirX, 0, dirZ,
                step + wallMargin)
            if hitDist >= 0 and hitDist < (step + wallMargin) then
                step = math.max(0, hitDist - wallMargin)
            end
        end

        if step < 1e-4 then
            self:FaceDirection(dirX, dirZ)
            return
        end

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
            self:RemoveCharacterController()
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
            self:SetPosition(x, y, z)
            local ctrl = self:CreateCharacterController()
            if not ctrl then
                --print("[EnemyAI] ConvertToGroundEnemy: CharacterController.Create failed")
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
        --print("[EnemyAI] PULLDOWN")
        self._animator:SetTrigger("Pulldown")

        self._slamActive = true
        self._slamVy = 0

        -- Publish a chain.slam_chain event so the player knows to play the SlamChain animation.
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("chain.slam_chain", true)
        end
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
            -- Slam landing: vertical squash (crash impact)
            self:_squashTrigger("vertical", 0.8)
            --print("[EnemyAI] SLAMMED")
            self:_publishSFX("groundSlam")
            self._animator:SetTrigger("Slammed")
            
        --Publish VFX EVENT (GroundSlamVFX)
            if _G.event_bus and _G.event_bus.publish then
                _G.event_bus.publish("SlammedDown",{
                    targetId = self.entityId,
                    posX = x,
                    posY = newY,
                    posZ = z
                })
            end

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
        -- If direction is too small, DO NOT change rotation (prevents "flip/lie down")
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

    -- Publish an audio event to EnemyAIAudio (sibling script on same entity).
    _publishSFX = function(self, sfxType)
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("enemy_sfx", { entityId = self.entityId, sfxType = sfxType })
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

    -- Raycast-based line-of-sight check. Returns false if a wall/obstacle
    -- blocks the straight line between this enemy and the player.
    HasLineOfSight = function(self)
        if not (Physics and Physics.Raycast) then return true end

        local ex, ey, ez = self:GetPosition()
        if ex == nil then return false end

        local tr = self._playerTr
        if not tr then
            tr = Engine.FindTransformByName(self.PlayerName)
            self._playerTr = tr
        end
        if not tr then return false end

        local pp = Engine.GetTransformPosition(tr)
        if not pp then return false end
        local px, py, pz = pp[1], pp[2], pp[3]

        -- Cast from enemy chest height toward player chest height
        local srcY = ey + 1.0
        local tgtY = py + 0.8
        local dx = px - ex
        local dy = tgtY - srcY
        local dz = pz - ez
        local dist = math.sqrt(dx*dx + dy*dy + dz*dz)
        if dist < 0.8 then return true end

        local dirX, dirY, dirZ = dx / dist, dy / dist, dz / dist

        -- Offset origin forward to avoid hitting own collider
        local off = 0.5
        local hitDist = Physics.Raycast(
            ex + dirX * off, srcY + dirY * off, ez + dirZ * off,
            dirX, dirY, dirZ, dist - off)

        if hitDist < 0 then return true end                    -- nothing hit = clear
        if hitDist >= (dist - off - 1.0) then return true end  -- hit near player = clear
        return false                                            -- wall before player
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
            -- If anything failed, free all three so we never "lose" a side knife.
            for i=1,3 do
                if knives[i] then knives[i]:Reset() end
            end
            return false
        end

        return true
    end,

    -- ==========================================================================
    -- SQUASH TRIGGER HELPER
    -- Starts a squash/stretch effect. Call from any trigger site.
    --   mode      : "vertical" | "horizontal"
    --   intensity : 0.0–1.0, scales effect magnitude against SquashStrength
    -- TO ADD a new mode: add an elseif branch in the squash update block in Update.
    -- ==========================================================================
    _squashTrigger = function(self, mode, intensity)
        self._squashPhase     = "squash"
        self._squashTimer     = 0
        self._squashMode      = mode or "vertical"
        self._squashIntensity = intensity or 1.0
    end,

    -- ==========================================================================
    -- AERIAL JUGGLE HELPERS
    -- Called from ApplyHit based on hitType: LIFT / AIR / SLAM
    -- [v2 FIX] _juggleY accumulator removed. CC owns Y entirely.
    --          SetJuggleMode disables stickToFloor; mJuggleVY drives ExtendedUpdate.
    -- ==========================================================================
    -- TODO (Sven): ApplyJuggleLift is disabled — air lift hit detection is inconsistent.
    -- The enemy does not reliably become airborne when the LIFT attack connects.
    -- Root cause is suspected to be in how the AttackHitbox collider overlaps at
    -- the moment of RigidBody enable. Needs engine-level investigation.
    ApplyJuggleLift = function(self)
        -- Intentionally left empty until the lift system is revisited.
        -- _isJuggled remains false, so all downstream juggle logic (AIR, SLAM,
        -- the juggle physics block in Update) stays inert.
        if self._isKnockedUp then return end

        -- Cancel existing knockback.
        self._kbT = 0
        self._kbVX, self._kbVZ = 0, 0

        -- Destroy CC so Jolt CharacterVirtual stops grounding the capsule.
        if self._controller then
            self:RemoveCharacterController()
        end

        -- Disable RigidBody so Jolt stops writing its own position back to the
        -- transform every physics step. This is the main reason SetPosition was
        -- being overwritten — CC gone but RB still ticking.
        if self._rb then
            pcall(function() self._rb.enabled = false end)
        end

        -- Only snapshot groundY when launching from the ground.
        -- Re-lifting an already-airborne enemy must NOT overwrite _juggleGroundY
        -- with the current mid-air Y, or the landing check triggers immediately.
        if not self._isJuggled then
            local _, transformY, _ = self:GetPosition()
            self._juggleGroundY = transformY or 0
            self._juggleY       = transformY or 0
        end
        -- If already juggled, _juggleGroundY stays at the true floor — only reset velocity.
        self._juggleVY      =  2.0 --should be changed to determine by combomanager knockback
        self._isJuggled     = true
        self._juggleAirTime = 0
    end,

    ApplyJuggleAirHit = function(self, knockback)
        -- ── Path A: juggle system (_isJuggled) ──────────────────────────────
        if self._isJuggled then
            self._kbT = 0
            self._kbVX, self._kbVZ = 0, 0
            -- Boost upward so the enemy doesn't fall between air hits.
            local boost = tonumber(self.JuggleAirBoost) or 4.0
            self._juggleVY = math.max(self._juggleVY or 0, boost)
            -- CC is nil during juggle (v3), no Move call needed.
            return
        end

        -- ── Path B: knockup system (_isKnockedUp) ────────────────────────────
        -- LIFT uses ApplyKnockup which sets _isKnockedUp, NOT _isJuggled.
        -- AIR hits must keep the enemy airborne here using a *separate* small
        -- boost value. NEVER feed the LIFT knockback value into this path —
        -- ComboManager's lift knockback (e.g. 80) at _knockupGravity -4.5
        -- sends the enemy hundreds of units into the sky.
        if self._isKnockedUp then
            local airBoost = tonumber(self.AirHitKnockupBoost) or 3.5

            -- Refresh upward velocity: clamp to airBoost so repeated rapid hits
            -- don't stack velocity. If already moving up faster somehow, leave it.
            if (self._knockupVY or 0) < airBoost then
                self._knockupVY = airBoost
            end

            -- Reset air-time gate so the landing check doesn't trigger immediately
            -- after the velocity refresh.
            self._knockupJustLanded = false

            -- Lateral knockback: ApplyKnockback guards against _isKnockedUp AND
            -- routes through CharacterController.Move which is nil during knockup —
            -- both paths silently fail. Set KB state directly; the _isKnockedUp
            -- block in Update integrates _kbVX/Z into SetPosition each frame.
            local kbStr = tonumber(self.AirHitKnockupKBStrength) or 6.0
            local kbDur = tonumber(self.AirHitKnockupKBDuration) or 0.18
            local tr = self._playerTr
            if not tr then tr = Engine.FindTransformByName(self.PlayerName) end
            if tr then
                local pp = Engine.GetTransformPosition(tr)
                if pp then
                    local ex, ez = self:GetEnemyPosXZ()
                    if ex and ez then
                        local dx = ex - pp[1]
                        local dz = ez - pp[3]
                        local len = math.sqrt(dx*dx + dz*dz)
                        if len >= 0.001 then
                            self._kbVX = (dx / len) * kbStr
                            self._kbVZ = (dz / len) * kbStr
                            self._kbT  = kbDur
                        end
                    end
                end
            end
            return
        end

        -- Grounded enemy: AIR hit doesn't connect.
        --print(string.format("[EnemyAI] AIR hit BLOCKED: enemy is not airborne"))
    end,

    ApplyJuggleSlam = function(self)
        -- SLAM on a non-juggled (grounded) enemy: don't activate the juggle system.
        -- Driving a grounded enemy underground then snapping back looked like a lift.
        -- Just apply strong knockback visually — no arc needed.
        if not self._isJuggled then
            --print(string.format("[EnemyAI] SLAM BLOCKED: enemy is not airborne — applying ground knockback instead"))
            self:ApplyKnockback(self.KnockbackStrength, self.KnockbackDuration)
            return
        end

        -- Enemy is airborne: drive them down fast.
        self._juggleVY = -(tonumber(self.JuggleSlamVelY) or 20.0)

        -- Apply lateral knockback in the player's slam direction.
        -- _G.player_slam_dirX/Z is written by PlayerMovement at hover-end for normal
        -- slams and updated per-frame for targeted (chain) slams, so it always
        -- reflects the actual dive direction at the moment of impact.
        local kbStr = tonumber(self.JuggleSlamKBStrength) or 12.0
        local kbDur = tonumber(self.JuggleSlamKBDuration) or 0.25
        local sdx = _G.player_slam_dirX or 0
        local sdz = _G.player_slam_dirZ or 0
        local sLen = math.sqrt(sdx * sdx + sdz * sdz)
        if sLen > 0.001 then
            self._kbVX = (sdx / sLen) * kbStr
            self._kbVZ = (sdz / sLen) * kbStr
            self._kbT  = kbDur
        else
            -- Fallback: push enemy away from player if no slam direction is set.
            local tr = self._playerTr
            if not tr then tr = Engine.FindTransformByName(self.PlayerName) end
            if tr then
                local pp = Engine.GetTransformPosition(tr)
                if pp then
                    local ex, ez = self:GetEnemyPosXZ()
                    if ex and ez then
                        local dx = ex - pp[1]
                        local dz = ez - pp[3]
                        local len = math.sqrt(dx * dx + dz * dz)
                        if len >= 0.001 then
                            self._kbVX = (dx / len) * kbStr
                            self._kbVZ = (dz / len) * kbStr
                            self._kbT  = kbDur
                        end
                    end
                end
            end
        end
        -- Keep _juggleGroundY from the original lift — don't touch it.
        -- Keep _juggleAirTime running — don't reset it.
    end,

    ApplyKnockup = function(self, forceY)
        if self.dead then return end
        if self:IsFlying() then return end
        if self._isJuggled then return end
        if self._isKnockedUp then return end

        self:CancelPendingAttack("HOOKED")

        -- cancel existing knockback
        self._kbT = 0
        self._kbVX, self._kbVZ = 0, 0

        -- clear path / stop ground movement
        self:ClearPath()
        self:StopCC()

        -- IMPORTANT: hard reset combat intent before going airborne
        --self:_ResetCombatAnimatorParams()

        -- reset any simple attack timers/flags that may resume later
        self.attackTimer = 0
        self._skipFirstCooldown = false
        self._hasAttackedBefore = false

        -- freeze FSM behaviour while airborne / landing
        self._knockupRecoverT = 0
        self._knockupJustLanded = false

        -- put FSM in a neutral logic state immediately
        self.fsm:ForceChange("Idle", self.states.Idle)

        self:RemoveCharacterController()

        if self._rb then
            pcall(function() self._rb.enabled = false end)
            pcall(function() self._rb.linearVel = { x=0, y=0, z=0 } end)
            pcall(function() self._rb.impulseApplied = { x=0, y=0, z=0 } end)
        end

        self._isKnockedUp = true
        self._knockupVY = forceY or 3.0

        local _, y, _ = self:GetPosition()
        self._knockupY = y or 0
        self._knockupGroundY = y or 0

        self:_PlayRandomHurtAnim()
    end,

    ApplyHit = function(self, dmg, hitType, knockback)
        if self.dead then
            --print(string.format("[EnemyAI] ApplyHit BLOCKED [%s]: enemy is already dead", tostring(hitType)))
            return
        end

        -- Juggle hits (LIFT/AIR/SLAM) must never be gated by the I-frame.
        -- The combo chain depends on landing these rapidly in sequence.
        -- iFrame is only meant to prevent stacking on ground hits.
        local isJuggleHit = (hitType == "LIFT" or hitType == "AIR" or hitType == "SLAM")

        if not isJuggleHit and (self._hitLockTimer or 0) > 0 then
            --print(string.format("[EnemyAI] ApplyHit BLOCKED [%s]: iFrame active (%.3fs remaining)", tostring(hitType), self._hitLockTimer or 0))
            return
        end
        if self._featherSkillBufferTimer <= 0 then
            self._hurtTriggeredByFeather = false
        end

        -- Juggle hits also must not set the iFrame, or the next hit in the chain gets blocked.
        if hitType ~= "FEATHER" and not isJuggleHit then
            self._hitLockTimer = self.config.HitIFrame or 0.05
        end

        self.health = self.health - (dmg or 1)
        --print(string.format("[EnemyAI] Remaining health: %d", self.health))

        -- Juggle hit types: LIFT/AIR/SLAM are fully owned by the juggle system.
        -- They MUST return after their juggle call — the hurt FSM block below
        -- must NOT run for these types. Running it caused:
        --   SLAM → ForceChange("Hurt") fired on the same frame as the slam velocity
        --   was set, which under certain timing re-triggered ApplyJuggleLift on the
        --   Hurt state's next hit event, making the enemy appear to "lift" on slam.
        -- LIFT: launch the enemy upward using the knockup system (reliable).
        -- ApplyJuggleLift is intentionally disabled (inconsistent overlap timing).
        -- ApplyKnockup reuses the same arc/gravity loop and CC teardown/recreate
        -- that the debug key (Keyboard.IsDigitPressed(9) / "KNOCKUP") validates.
        if hitType == "LIFT" then
            if self:IsFlying() then return end

            if self.health <= 0 then
                self.health = 0
                self:_squashTrigger("vertical", 0.5)
                self:_publishSFX("death")
                if _G.event_bus and _G.event_bus.publish then
                    _G.event_bus.publish("enemy_died", { entityId = self.entityId })
                end
                self.fsm:Change("Death", self.states.Death)
                return
            end

            self:ApplyKnockup(knockback)
            return
        elseif hitType == "AIR" then
            if self:IsFlying() then
                -- Flying enemy is airborne via hover, not the juggle system.
                -- Skip juggle boost and fall through to the normal hurt FSM below.
                --print(string.format("[EnemyAI] AIR hit on flying enemy — treating as normal hit"))
                self:ApplyKnockback(knockback or self.KnockbackStrength, self.KnockbackDuration)
                -- intentional fall-through: no return here
            else
                self:ApplyJuggleAirHit(knockback)   -- pass per-attack knockback for lateral force
                self:_squashTrigger("horizontal", 0.4)
                if self.health <= 0 then
                    self.health = 0
                    self:_squashTrigger("vertical", 0.5)
                    self:_publishSFX("death")
                    if _G.event_bus and _G.event_bus.publish then
                        _G.event_bus.publish("enemy_died", { entityId = self.entityId })
                    end
                    self.fsm:Change("Death", self.states.Death)
                end
                return
            end
        elseif hitType == "SLAM" then
            self:ApplyJuggleSlam()
            -- No squash here — landing squash fires in the juggle block on touchdown.
            if self.health <= 0 then
                self.health = 0
                self:_squashTrigger("vertical", 0.5)
                self:_publishSFX("death")
                if _G.event_bus and _G.event_bus.publish then
                    _G.event_bus.publish("enemy_died", { entityId = self.entityId })
                end
                self.fsm:Change("Death", self.states.Death)
            end
            return
        elseif hitType == "KNOCKUP" then
            if self:IsFlying() then
                return
            end

            if self.health <= 0 then
                self.health = 0
                self:_squashTrigger("vertical", 0.5)
                self:_publishSFX("death")
                if _G.event_bus and _G.event_bus.publish then
                    _G.event_bus.publish("enemy_died", { entityId = self.entityId })
                end
                self.fsm:Change("Death", self.states.Death)
                return
            end

            self:ApplyKnockup(3.0)
            return
        else
            -- Ground/combo hits: XZ knockback only.
            self:ApplyKnockback(knockback or self.KnockbackStrength, self.KnockbackDuration)
        end

        if self.health <= 0 then
            self.health = 0
            self:_squashTrigger("vertical", 0.5)
            self:_publishSFX("death")
            if _G.event_bus and _G.event_bus.publish then
                _G.event_bus.publish("enemy_died", { entityId = self.entityId })
            end
            self.fsm:Change("Death", self.states.Death)
            return
        end

        -- This hit alerts nearby enemies from THIS enemy's position
        self:SetAggressive(true)
        self:BroadcastAggroAlert(self.AlertRadiusOnHit or 8.0)

        -- interrupt any delayed melee/ranged attack that has not fired yet
        self:CancelPendingAttack("HURT")

        -- Hit reaction: horizontal squash (pushed sideways)
        self:_squashTrigger("horizontal", 0.6)

        if not self._hurtTriggeredByFeather then
            local myRandomValue = math.random(1, 3)
            if myRandomValue == 1 then
                --print("[EnemyAI] Animator:SetBool(Hurt1, true)")
                self._animator:SetBool("Hurt1", true)
            elseif myRandomValue == 2 then
                --print("[EnemyAI] Animator:SetBool(Hurt2, true)")
                self._animator:SetBool("Hurt2", true)
            elseif myRandomValue == 3 then
                --print("[EnemyAI] Animator:SetBool(Hurt3, true)")
                self._animator:SetBool("Hurt3", true)
            end

            -- Play hurt SFX (only if not dead)
            self:_publishSFX("hurt")

            if self.fsm.currentName == "Hooked" then
                return
            end
            self.fsm:ForceChange("Hurt", self.states.Hurt)
        end

        if hitType == "FEATHER" then
            self._featherSkillBufferTimer = self.FeatherSkillBufferDuration
            self._hurtTriggeredByFeather = true
        end
    end,

    ApplyKnockback = function(self, strength, duration)
        if self.dead then return end
        if self._isKnockedUp then return end

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
        if self._isKnockedUp then return end
        if duration and duration > 0 then
            self.config.HookedDuration = duration
        end

        self:CancelPendingAttack("HOOKED")

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

        -- Publish a chain.pull_chain event so the player knows to play the PullChain animation.
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("chain.pull_chain", true)
        end
    end,

    BroadcastAggroAlert = function(self, radius)
        if not (_G.event_bus and _G.event_bus.publish) then return end

        local x, _, z = self:GetPosition()
        _G.event_bus.publish("enemy_alert", {
            x = x or 0,
            z = z or 0,
            radius = radius or self.AlertRadiusOnHit or 8.0,
            sourceEntityId = self.entityId,
            source = "combat",
        })
    end,

    SetAggressive = function(self, value)
        self.aggressive = value and true or false
    end,

    OnEnemyAlert = function(self, msg)
        if not msg then return end
        if self.dead then return end
        if self.fsm.currentName == "Death" then return end
        if self._frozenBycinematic then return end
        if self.IsPassive then return end

        -- use controller-aware position
        local ex, ez = self:GetEnemyPosXZ()
        if ex == nil or ez == nil then return end

        local ax = msg.x or 0
        local az = msg.z or 0
        local r  = msg.radius or 0

        local dx = ax - ex
        local dz = az - ez
        if (dx * dx + dz * dz) > (r * r) then
            return
        end

        self:SetAggressive(true)

        -- hard reset stale patrol / attack / path state before chase
        self:CancelPendingAttack("ALERTED")
        self:ClearPath()
        self:StopCC()

        self._patrolWaitT = 0
        self._isPatrolWait = false
        self._readyLatched = false
        self._readySettleT = 0

        if self.fsm and self.states and self.states.Chase then
            local s = self.fsm.currentName
            if s ~= "Death" and s ~= "Hooked" then
                self.fsm:ForceChange("Chase", self.states.Chase)
            end
        end
    end,

    OnPlayerDied = function(self, msg)
        self:SetAggressive(false)

        -- optional: clear attack intent too
        if self.CancelPendingAttack then
            self:CancelPendingAttack("PLAYER_DIED")
        end

        -- if enemy is alive and not in special states, return to patrol
        if self.dead then return end
        if not (self.fsm and self.states) then return end

        local s = self.fsm.currentName
        if s ~= "Death" and s ~= "Hooked" and s ~= "Hurt" then
            self.fsm:Change("Patrol", self.states.Patrol)
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

        self:RemoveCharacterController()

        -- Kill RB velocity (prevents kinematic leftovers)
        if self._rb then
            pcall(function() self._rb.linearVel = { x=0, y=0, z=0 } end)
            pcall(function() self._rb.impulseApplied = { x=0, y=0, z=0 } end)
        end

        -- Reset facing cache (avoid "lying down" from stale quat)
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

            if self._comboDamageSub then
                pcall(function()
                    _G.event_bus.unsubscribe(self._comboDamageSub)
                end)
                self._comboDamageSub = nil
            end

            if self._liftAttackSub then
                pcall(function() _G.event_bus.unsubscribe(self._liftAttackSub) end)
                self._liftAttackSub = nil
            end

            if self._slamAttackSub then
                pcall(function() _G.event_bus.unsubscribe(self._slamAttackSub) end)
                self._slamAttackSub = nil
            end

            if self._slamLandedSub then
                pcall(function() _G.event_bus.unsubscribe(self._slamLandedSub) end)
                self._slamLandedSub = nil
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

            if self._onEnemyAlertSub then
                pcall(function() _G.event_bus.unsubscribe(self._onEnemyAlertSub) end)
                self._onEnemyAlertSub = nil
            end
        end
        self._frozenBycinematic = false
    end,

    Despawn = function(self)
        --print("[EnemyAI] Despawn called for entity =", tostring(self.entityId))
        if self._despawned then return end
        self._despawned = true
        --self._softDespawned = true
        self._freezeAI = true
        self.dead = true

        -- stop movement
        pcall(function() self:StopCC() end)
        pcall(function() self:ClearPath() end)

        -- full cleanup
        self:RemoveCharacterController()

        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._damageSub then pcall(function() _G.event_bus.unsubscribe(self._damageSub) end) self._damageSub = nil end
            if self._comboDamageSub then pcall(function() _G.event_bus.unsubscribe(self._comboDamageSub) end) self._comboDamageSub = nil end
            if self._liftAttackSub then pcall(function() _G.event_bus.unsubscribe(self._liftAttackSub) end) self._liftAttackSub = nil end
            if self._slamAttackSub then pcall(function() _G.event_bus.unsubscribe(self._slamAttackSub) end) self._slamAttackSub = nil end
            if self._slamLandedSub then pcall(function() _G.event_bus.unsubscribe(self._slamLandedSub) end) self._slamLandedSub = nil end
            if self._freezeEnemySub then pcall(function() _G.event_bus.unsubscribe(self._freezeEnemySub) end) self._freezeEnemySub = nil end
            if self._respawnPlayerSub then pcall(function() _G.event_bus.unsubscribe(self._respawnPlayerSub) end) self._respawnPlayerSub = nil end
            if self._playerDeadSub then pcall(function() _G.event_bus.unsubscribe(self._playerDeadSub) end) self._playerDeadSub = nil end
            if self._chainEndpointHitSub then pcall(function() _G.event_bus.unsubscribe(self._chainEndpointHitSub) end) self._chainEndpointHitSub = nil end
            if self._chainHookSub then pcall(function() _G.event_bus.unsubscribe(self._chainHookSub) end) self._chainHookSub = nil end
            if self._onEnemyAlertSub then pcall(function() _G.event_bus.unsubscribe(self._onEnemyAlertSub) end) self._onEnemyAlertSub = nil end
        end

        if self._rb then
            pcall(function() self._rb.linearVel = { x=0, y=0, z=0 } end)
            pcall(function() self._rb.impulseApplied = { x=0, y=0, z=0 } end)
            pcall(function() self._rb.enabled = false end)
        end

        local col = self:GetComponent("ColliderComponent")
        if col then pcall(function() col.enabled = false end) end

        local model = self:GetComponent("ModelRenderComponent")
        if model then pcall(function() ModelRenderComponent.SetVisible(model, false) end) end

        local active = self:GetComponent("ActiveComponent")
        if active then pcall(function() active.isActive = false end) end

        -- actual entity destruction
        if _G.Engine and _G.Engine.DestroyEntityDup then
            local ok, err = pcall(function()
                _G.Engine.DestroyEntityDup(self.entityId)
            end)
            --print("[EnemyAI] DestroyEntityDup ok =", tostring(ok), " err =", tostring(err), " entity =", tostring(self.entityId))
        else
            --print("[EnemyAI] WARNING: DestroyEntityDup not available, falling back to soft despawn")
        end
    end,

    SoftDespawn = function(self)
        if self._softDespawned then return end
        self._softDespawned = true

        --print("[EnemyAI] SoftDespawn entity=", tostring(self.entityId))

        -- Stop FSM/AI updates from doing anything
        self._freezeAI = true
        self.dead = true

        self:RemoveCharacterController()

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
        self:RemoveCharacterController()
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

            if self._liftAttackSub then
                pcall(function() _G.event_bus.unsubscribe(self._liftAttackSub) end)
                self._liftAttackSub = nil
            end

            if self._slamAttackSub then
                pcall(function() _G.event_bus.unsubscribe(self._slamAttackSub) end)
                self._slamAttackSub = nil
            end

            if self._slamLandedSub then
                pcall(function() _G.event_bus.unsubscribe(self._slamLandedSub) end)
                self._slamLandedSub = nil
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

            if self._onEnemyAlertSub then
                pcall(function() _G.event_bus.unsubscribe(self._onEnemyAlertSub) end)
                self._onEnemyAlertSub = nil
            end
        end
    end,
}