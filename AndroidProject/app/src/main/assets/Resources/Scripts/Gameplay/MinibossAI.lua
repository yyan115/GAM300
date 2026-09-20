-- Resources/Scripts/Gameplay/MinibossAI.lua
require("extension.engine_bootstrap")
local debugControls = os and os.getenv and os.getenv("GAM300_DEBUG") == "1"
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local StateMachine = require("Gameplay.StateMachine")
local ChooseState  = require("Gameplay.MinibossChooseState")
local ExecuteState = require("Gameplay.MinibossExecuteState")
local RecoverState = require("Gameplay.MinibossRecoverState")
local BattlecryState = require("Gameplay.MinibossBattlecryState")

local KnifePool = require("Gameplay.KnifePool")

-- Hit types AttackHitbox sends for the player's weapon swings. AttackHitbox
-- already lets each swing hit a target only once, so these do not need the
-- long HitIFrame to stop one swing counting twice.
local SWING_HIT_TYPES = { COMBO = true, LIFT = true, AIR = true, SLAM = true }

-- Moves that deal damage. Nothing the player does stops one once it has
-- started: the boss cannot be stunned.
local ATTACK_MOVE_KINDS = {
    BossMelee = true, P1RangedCharged = true, ShoutAOE = true, Basic = true,
    BurstFire = true, AntiDodge = true, FateSealed = true, DeathLotus = true,
    FeatherFlurry = true,
}

-- The animator trigger an attack move sets to start its animation. A trigger
-- stays set until a transition uses it, so a cancelled move clears its own,
-- or the animation would play later with no attack behind it.
local ATTACK_MOVE_TRIGGER = {
    BossMelee = "Melee", FateSealed = "Melee",
    P1RangedCharged = "Ranged", Basic = "Ranged", BurstFire = "Ranged", AntiDodge = "Ranged",
    DeathLotus = "Ranged", ShoutAOE = "Taunt", FeatherFlurry = "Ranged",
}

local HURT_TRIGGERS = { "Hurt1", "Hurt2", "Hurt3" }

-- What phase 3 does on the ground after each dive, in order
local P3_GROUND_PLAN = { "DeathLotus", "Lunge", "FeatherFlurry", "Lunge" }

-- The MinibossAC states the boss flinches from: its combat idle, between
-- attacks, the only one of them with a transition into Hurt. A hurt animation
-- never interrupts an attack, and a hurt trigger set in any other state would
-- wait and pull a later attack into Hurt.
local TAKES_HURT = {
    ["Recovery"] = true,
}

-- The MinibossAC states that play the claw swing and the throw
local SWING_STATE = "Melee Attack"
local THROW_STATE = "Ranged Attack"

-- The MinibossAC states of being pulled out of the air: the fall, the impact
-- and getting up. The boss is on the floor through all three.
local FALL_STATES = {
    ["Falling"] = true, ["Fall Impact"] = true, ["Stand Up"] = true,
}

-------------------------------------------------
-- Helpers
-------------------------------------------------
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
    local lenSq = (dx or 0)*(dx or 0) + (dz or 0)*(dz or 0)
    if lenSq < 1e-10 then
        return nil
    end
    local invLen = 1.0 / math.sqrt(lenSq)
    dx, dz = dx * invLen, dz * invLen

    local yaw = atan2(dx, dz) -- radians
    local half = yaw * 0.5
    return math.cos(half), 0, math.sin(half), 0 -- (w,x,y,z) yaw-only about Y
end

local function toDtSec(dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return 0 end
    if dtSec > 0.05 then dtSec = 0.05 end
    return dtSec
end

local function unpackPos(pp)
    if not pp then return nil end

    if type(pp) == "table" then
        if (pp.x ~= nil or pp.y ~= nil or pp.z ~= nil) then
            return pp.x, pp.y, pp.z
        end
        return pp[1], pp[2], pp[3]
    end

    -- NEW: userdata that supports numeric indexing
    if type(pp) == "userdata" then
        local ok, x, y, z = pcall(function() return pp[1], pp[2], pp[3] end)
        if ok then return x, y, z end
    end

    return nil
end

local function requestUpTo(n)
    for k = n, 1, -1 do
        local knives = KnifePool.RequestMany(k)
        if knives and #knives > 0 then return knives end
    end
    return nil
end

local function _lockPriority(reason)
    if reason == "DEAD"            then return 100 end
    if reason == "PHASE_TRANSFORM" then return 90  end
    return 0
end

-------------------------------------------------
-- Phase definition (HP-based)
-------------------------------------------------
local PHASE_THRESHOLDS = {
    { id = 3, hpPct = 0.33 },
    { id = 2, hpPct = 0.66 },
}

local function _phaseThresholdPct(phaseId)
    if phaseId == 1 then return 1.00 end
    for i = 1, #PHASE_THRESHOLDS do
        if PHASE_THRESHOLDS[i].id == phaseId then
            return PHASE_THRESHOLDS[i].hpPct
        end
    end
    return nil
end

-------------------------------------------------
-- Move definitions (DATA-DRIVEN)
-------------------------------------------------
-- DEAD CODE, kept because it is the only written record of the intended
-- weighting. Nothing calls ChooseMove or GetMoveWeightForPhase, so this table
-- selects nothing: the moves the boss actually uses are driven by the scripted
-- sequences in _UpdatePhase1/2/3. Death Lotus, for instance, is step 3 of the
-- phase 3 loop, not a weighted roll, which is why it fires despite being
-- weighted 0 in every phase this table can reach.
local MOVES = {
    Move1 = { cooldown = 2.0, weights = { [1]=50, [2]=20, [3]=10, [4]=0 }, execute = function(ai) print("[Miniboss] Move1: Basic Attack") ai:BasicAttack() end },
    Move2 = { cooldown = 2.5, weights = { [1]=25, [2]=35, [3]=30, [4]=20 }, execute = function(ai) print("[Miniboss] Move2: Burst Fire") ai:BurstFire() end },
    Move3 = { cooldown = 3.0, weights = { [1]=25,  [2]=35, [3]=30, [4]=20 }, execute = function(ai) print("[Miniboss] Move3: Anti Dodge") ai:AntiDodge() end },
    Move4 = { cooldown = 4.0, weights = { [1]=0,  [2]=10,  [3]=30, [4]=30 }, execute = function(ai) print("[Miniboss] Move4: Fate Sealed") ai:FateSealed() end },
    Move5 = { cooldown = 5.0, weights = { [1]=0,  [2]=0,  [3]=0,  [4]=30 }, execute = function(ai) print("[Miniboss] Move5: Death Lotus") ai:DeathLotus() end },

    -- For testing individual moves 1 by 1
    -- Move1 = { cooldown = 2.0, weights = { [1]=0, [2]=20, [3]=10, [4]=0 }, execute = function(ai) print("[Miniboss] Move1: Basic Attack") ai:BasicAttack() end },
    -- Move2 = { cooldown = 2.5, weights = { [1]=0, [2]=35, [3]=30, [4]=20 }, execute = function(ai) print("[Miniboss] Move2: Burst Fire") ai:BurstFire() end },
    -- Move3 = { cooldown = 3.0, weights = { [1]=0,  [2]=35, [3]=30, [4]=20 }, execute = function(ai) print("[Miniboss] Move3: Anti Dodge") ai:AntiDodge() end },
    -- Move4 = { cooldown = 4.0, weights = { [1]=0,  [2]=10,  [3]=30, [4]=30 }, execute = function(ai) print("[Miniboss] Move4: Fate Sealed") ai:FateSealed() end },
    -- Move5 = { cooldown = 5.0, weights = { [1]=10,  [2]=0,  [3]=0,  [4]=30 }, execute = function(ai) print("[Miniboss] Move5: Death Lotus") ai:DeathLotus() end },
}

local MOVE_ORDER = { "Move1", "Move2", "Move3", "Move4", "Move5" }

-------------------------------------------------
-- Component
-------------------------------------------------
return Component {
    mixins = { TransformMixin },

    fields = {
        MaxHealth = 30,
        RecoverDuration = 1.0,
        KnockbackDuration = 0.12,

        -- Damage / Hook / Death
        HitIFrame      = 0.2,
        -- The player's three hit chain lands about half a second apart, inside
        -- HitIFrame (1.0 on the boss in 04_Level), so without a window of its
        -- own only the first hit of a chain would count. Weapon swings, on the
        -- ground or in the air, use this one. Everything else keeps HitIFrame,
        -- which stops a single explosion registering once per tick.
        ComboHitIFrame = 0.25,

        -- "Transformation" (phase transition) lock
        PhaseTransformDuration = 3.2,

        PlayerName = "Player",

        -- Gravity tuning
        Gravity      = -9.81,
        MaxFallSpeed = -25.0,

        -- small "stick to ground" behaviour
        GroundStickVel = -0.2,

        IntroDuration = 5.0,
        AggroRange    = 15.0,  -- distance to trigger intro

        -- Phase gates
        Phase2HpPct = 0.66,
        Phase3HpPct = 0.33,

        -- Phase 1 shout checkpoints
        P1_Shout1Pct = 0.90,
        P1_Shout2Pct = 0.75,

        -- Boss melee tuning (phase 1)
        BossMeleeRange = 2.2,
        BossMeleeWindup = 0.40,
        BossMeleeCooldown = 2.95,

        -- Ranged charge (phase 1 Move1)
        P1_RangedCharge = 0.75,
        -- Charge time for the phase 1 charged slash. Longer than phase 3's so
        -- the telegraph is readable the first time a player meets it.
        P1_ChargedSlashCharge = 1.10,
        -- Knife throws at a player who keeps out of melee range before the
        -- boss answers with the charged slash. Rolled again after each slash.
        P1_ThrowsBeforeLungeMin = 1,
        P1_ThrowsBeforeLungeMax = 2,
        -- Melee attacks at a player who stays in melee range before the boss
        -- answers with the charged slash, the same way. More than the throws,
        -- since a melee attack comes round faster than a throw.
        P1_MeleesBeforeLungeMin = 3,
        P1_MeleesBeforeLungeMax = 3,
        -- Speed of the charged slash's dash, in units per second, in every
        -- phase. The claw lands 0.36 s into the dash, and at this speed the
        -- boss crosses the arena (about 14 units) by then.
        LungeSpeed = 40.0,
        -- Seconds an attack waits for its animation to start: the claw swing
        -- for a melee attack, the throw for a ranged one. The animation can be
        -- held up by a hurt animation or by getting up off the floor, and the
        -- attack is dropped if it does not start in this time.
        AttackAnimStartTimeout = 1.0,
        -- Damage the player's feather skill does to this boss: three times a
        -- first hit, which ComboManager puts at 10.
        FeatherSkillDamage = 30,
        -- Damage the ground combo's three hits do to this boss, weak to strong.
        -- 3, 4 and 6 wore it down too slowly.
        ComboHit1Damage = 5,
        ComboHit2Damage = 7,
        ComboHit3Damage = 10,
        -- Seconds before the hurt sound can play again. Every landed hit counts
        -- now, and a sound for each was far too often.
        HurtSoundCooldown = 3.5,

        -- Shout AOE
        ShoutRadius = 4.0,
        ShoutDamage = 1,
        ShoutKnockback = 120.0,
        ShoutWindup = 0.95,
        ShoutPostDelay = 2.15,
        ShoutCooldown = 999, -- checkpoint only (or set if you want it to recur)

        ShoutShakeIntensity       = 0.85,
        ShoutShakeDuration        = 0.55,
        ShoutShakeFrequency       = 24.0,
        ShoutFxChromaticIntensity = 4.45,
        ShoutFxChromaticDuration  = 0.55,
        ShoutFxBlurIntensity      = 0.35,
        ShoutFxBlurRadius         = 2.5,
        ShoutFxBlurDuration       = 0.25,

        -- The charged slash crosses the room, so its wind up shakes the camera
        -- for as long as the wind up lasts. camera_follow fades a shake out over
        -- its duration, so it is strongest as the charge begins. Intensity is in
        -- degrees: 0.55 sits above the small hit shakes and below the phase
        -- transition shake.
        ChargeShakeIntensity      = 0.55,
        ChargeShakeFrequency      = 18.0,
        -- The slam is the opposite: one short hard jolt on impact.
        SlamShakeIntensity        = 0.70,
        SlamShakeDuration         = 0.35,
        SlamShakeFrequency        = 40.0,

        PhaseShakeIntensity       = 0.85,
        PhaseShakeDuration        = 2.55,
        PhaseShakeFrequency       = 48.0,
        PhaseFxChromaticIntensity = 2.75,
        PhaseFxChromaticDuration  = 0.80,
        PhaseFxBlurIntensity      = 2.55,
        PhaseFxBlurRadius         = 3.5,
        PhaseFxBlurDuration       = 1.25,

        -- Air movement
        AirHeight = 1.0,
        AirMoveSpeed = 9.0,
        AirSpeedMultiplier = 10.0,
        AirVerticalMultiplier = 10.0,
        AirArriveRadius = 0.25,
        AirWaitAfterAttack = 0.45,

        -- Hover tuning
        HoverSnapSpeed = 8.0,   -- how fast it corrects to target height
        HoverBobAmp    = 0.10,  -- bob amplitude (try 0.06 - 0.18)
        HoverBobFreq   = 0.90,  -- bob frequency (Hz-ish)

        -- Slam-down when hooked in air
        SlamDownSpeed  = 16.0,  -- fall speed while slamming down

        -- Arena 3x3 grid waypoint positions (world-space XZ)
        -- You can tune these in editor per arena
        GridStep = 4.0,
        GridCenterX = 0.0,
        GridCenterZ = 0.0,

        ArenaCenterX = 0.0,
        ArenaCenterZ = 0.0,
        ArenaRadius  = 12.0,
        ArenaLeashBuffer = 0.6,
        ReturnToArenaSpeedMultiplier = 1.15,
        PlayerArenaExtraRadius = 0.75,

        P2_BurstRounds = 3,
        P2_BurstGap = 0.8, -- small pause between bursts
        -- Seconds between the knives within one burst
        P2_BurstInterval = 0.12,
        -- Attacks the boss takes on the ground after being hooked down, before
        -- it lifts off again. This is the window the player earned with the hook.
        P2_GroundAttacksAfterSlam = 2,
        -- Least time it stays down after a hook, from landing. On the ground it
        -- gets up, takes its attacks, then the charged slash, and lifts off once
        -- all of that is done and this much time has passed.
        P2_MinGroundTime = 8.0,

        -- Phase 3 tuning
        -- Least time it stays on the ground after the dive, from landing. The
        -- player can only hit it down here, and the feather bombs above are
        -- still what makes it fly up again.
        P3_MinGroundTime = 9.0,
        -- Phase 3's charged slash: a shorter wind up and a faster dash than
        -- phase 1's, twice in every stay on the ground.
        P3_LungeCharge = 0.50,
        P3_LungeSpeed = 60.0,
        -- The feather flurry on the ground: feathers thrown at the player in
        -- quick succession, over as many throw animations as it takes.
        P3_FlurryShots = 12,
        P3_FlurryInterval = 0.10,
        P3_FeatherCellsPerRound = 5,
        P3_FeatherRounds = 2,
        P3_FeatherRoundGap = 0.90,        -- time between rounds (telegraph/explode window)
        P3_FeatherTargetYOffset = 0.25,   -- aim slightly above ground

        P3_DiveCommitRadius = 0.20,       -- how close in XZ before slamming down
        P3_DivePreDelay = 1.00,           -- how long to wait before the dive smash
        P3_DivePostDelay = 0.50,          -- how long to wait after the dive smash

        P3_FeatherCastTime = 0.25,         -- longer "windup" before firing all 5
        P3_FeatherCooldown = 2.00,         -- longer cooldown after firing (between rounds)
        FeatherBombProjectilePrefab = "Resources/Prefabs/Knife_FeatherBomb.prefab",
        P3_FeatherBombExplosionPrefabPath = "Resources/Prefabs/MinibossFeatherBombExplosion.prefab",

        -- Tile activation timing (keep activating tile, but slower)
        P3_FeatherActivateDelay = 0.90,    -- telegraph delay before tile becomes dangerous
        P3_FeatherActiveDuration = 1.25, -- how long the tile is dangerous after activation

        -- Feather travel speed (slower fall/flight)
        P3_FeatherSpeedScale = 0.2,       -- multiplier on knife speed for P3F tags (0.2~0.6 feels good)

    },

    Awake = function(self)
        self.health = self.MaxHealth
        self.dead   = false

        self.fsm = StateMachine.new(self)
        self.states = {
            Choose    = ChooseState,
            Execute   = ExecuteState,
            Recover   = RecoverState,
            Battlecry = BattlecryState
        }

        self._moveCooldowns = {}
        self.currentMove = nil
        self.currentMoveDef = nil
        self._recoverTimer = 0
        self._hitLockTimer = 0

        self._moveQueue = {}

        -- active move runtime
        self._move = nil
        self._moveFinished = true

        -- dash state helpers
        self._dash = nil

        -- lotus state helpers
        self._lotus = nil

        -- knife volley counter (token uniqueness)
        self._knifeVolleyId = 0

        -- action lock system (blocks Choose/Execute/etc)
        self._inIntro      = false
        self._introDone    = false
        self._lockAction   = false
        self._lockReason   = nil
        self._lockTimer    = 0
        self._combatActive = false
        self._bossHealthBarShown = false

        -- phase tracking
        self._phase = 1                 -- current phase id
        self._lastPhaseProcessed = 1
        self._pendingPhase = nil        -- when transforming
        self._transforming = false

        self._hoverT = 0
        self._slamActive = false
        self._slamMode = nil
        self._phase3DiveStarted = false

        -- CC / movement state
        self._controller = nil
        self._rb = nil
        self._collider = nil
        self._transform = nil

        self._vy = 0
        self._prevY = nil

        -- facing cache (prevents “lying down” style issues)
        self._lastFacingRot = nil

        -- player cache
        self._playerTr = nil

        self._p1DidShout90 = false
        self._p1DidShout75 = false
        self._meleeCdT = 0
        self._p1ThrowsAway = 0
        self._p1ThrowTarget = self:_RollP1ThrowTarget()
        self._p1MeleesClose = 0
        self._p1MeleeTarget = self:_RollP1MeleeTarget()

        self._p3_dive_postdelay = self.P3_DivePostDelay
        self._p3_dive_predelay = self.P3_DivePreDelay

        self._pendingRainExplosions = {}  -- { {t=seconds, payload=table}, ... }
    end,

    Start = function(self)
        -- Grab components (same pattern as EnemyAI)
        self._collider  = self:GetComponent("ColliderComponent")
        self._transform = self:GetComponent("Transform")
        self._rb        = self:GetComponent("RigidBodyComponent")
        self._animator  = self:GetComponent("AnimationComponent")
        self._entityName = Engine.GetEntityName(self.entityId)

        -- (Re)create controller safely
        if self._controller then
            pcall(function() CharacterController.DestroyByEntity(self.entityId) end)
            self._controller = nil
        end

        if not self._controller and self._collider and self._transform then
            local ok, ctrl = pcall(function()
                return CharacterController.Create(self.entityId, self._collider, self._transform)
            end)
            if ok and ctrl then
                self._controller = ctrl
                pcall(function() CharacterController.SetImmovable(self.entityId, true) end)
                pcall(function() CharacterController.SetStepUp(ctrl, 0.15, 0.3) end)
            else
                --print("[MinibossAI] CharacterController.Create failed")
                self._controller = nil
            end
        end

        -- Set RB kinematic-ish like EnemyAI
        if self._rb then
            pcall(function() self._rb.motionID = 0 end)
            pcall(function() self._rb.linearVel = { x=0, y=0, z=0 } end)
            pcall(function() self._rb.impulseApplied = { x=0, y=0, z=0 } end)
        end

        -- Cache player transform
        self._playerTr = Engine.FindTransformByName(self.PlayerName)

        -- Seed RNG ONCE (so each playthrough differs)
        if not _G.__MINIBOSS_RNG_SEEDED then
            local t = 0
            if _G.Time and _G.Time.GetTime then
                t = _G.Time.GetTime() -- seconds (engine)
            else
                t = os.time()
            end
            -- mix in entityId so multiple minibosses don't share the same sequence
            math.randomseed(math.floor((t * 1000) + (self.entityId or 0)))
            -- burn a few values (Lua RNG sometimes has poor early distribution)
            math.random(); math.random(); math.random()
            _G.__MINIBOSS_RNG_SEEDED = true
        end

        -- === Damage event subscription ===
        self._damageSub = nil
        if _G.event_bus and _G.event_bus.subscribe then
            self._damageSub = _G.event_bus.subscribe("enemy_damage", function(payload)
                if not payload then return end
                if payload.entityId ~= nil and payload.entityId ~= self.entityId then
                    return
                end
                local dmg = payload.dmg or 1
                local hitType = payload.hitType or payload.src or "MELEE"
                self:ApplyHit(dmg, hitType)
            end)

            self._comboDamageSub = _G.event_bus.subscribe("deal_damage_to_entity", function(payload)
                if not payload then return end

                if payload.entityId ~= self.entityId then
                    return
                end

                local damage = payload.damage or 10
                local hitType = payload.hitType or "COMBO"

                -- The feather skill names its own damage here rather than
                -- through the explosion prefab, which every enemy shares.
                if hitType == "FEATHER" then
                    damage = self.FeatherSkillDamage or damage
                end

                -- So do the ground combo's hits, which other enemies take at
                -- ComboManager's own figures.
                if hitType == "COMBO" then
                    local byStep = {
                        light_1 = self.ComboHit1Damage,
                        light_2 = self.ComboHit2Damage,
                        light_3 = self.ComboHit3Damage,
                    }
                    damage = byStep[payload.state] or damage
                end

                self:ApplyHit(damage, hitType)
            end)
        end

        -- === Freeze during cinematic ===
        self._frozenBycinematic = true
        self._freezeEnemySub = nil
        if _G.event_bus and _G.event_bus.subscribe then
            self._freezeEnemySub = _G.event_bus.subscribe("freeze_enemy", function(frozen)
                self._frozenBycinematic = frozen
            end)
        end

        -- === Hook event subscription ===
        self._hookSub = nil
        if _G.event_bus and _G.event_bus.subscribe then
            self._hookSub = _G.event_bus.subscribe("enemy_hook", function(payload)
                if not payload then return end
                if payload.entityId ~= nil and payload.entityId ~= self.entityId then
                    return
                end
                self:ApplyHook()
            end)
        end

        -- === Melee hit confirmation subscription ===
        self._meleeHitSub = nil
        if _G.event_bus and _G.event_bus.subscribe then
            self._meleeHitSub = _G.event_bus.subscribe("miniboss_melee_hit_confirmed", function(payload)
                if not payload then return end
                if payload.entityId ~= nil and payload.entityId ~= self.entityId then
                    return
                end
                -- Play melee hit SFX when slash hits player
                self:_publishSFX("meleeHit")
            end)
        end

        self._chainEndpointHitSub = _G.event_bus.subscribe("chain.endpoint_hit_entity", function(payload)
            if not payload then return end
            if payload.rootName ~= self._entityName then return end
            --print("[MinibossAI] chain.endpoint_hit_entity received")
            self._animator:SetTrigger("Hooked")
            -- Miniboss is grounded — tell the chain icon to switch to the Pull variant.
            if _G.event_bus and _G.event_bus.publish then
                _G.event_bus.publish("chain.hooked_target_type", {
                    entityId = self.entityId,
                    isFlying = false,
                })
            end
        end)

        self._chainEnemyHookedSub = _G.event_bus.subscribe("chain.enemy_hooked", function(payload)
            if not payload then return end
            if payload.entityId ~= self.entityId then return end
            --print("[MinibossAI] chain.enemy_hooked received — calling ApplyHook")
            pcall(function() self:ApplyHook() end)
        end)

        -- === Player death subscription ===
        self._playerDead = false
        self._playerDeadSub = nil
        self._respawnPlayerSub = nil

        if _G.event_bus and _G.event_bus.subscribe then
            self._playerDeadSub = _G.event_bus.subscribe("playerDead", function(dead)
                self._playerDead = dead == true
                if self._playerDead then
                    self:ResetBossToIdle()
                end
            end)

            self._respawnPlayerSub = _G.event_bus.subscribe("respawnPlayer", function(respawn)
                if respawn then
                    self._playerDead = false
                end
            end)
        end

        -- Seed prevY for grounded heuristic
        local _, y, _ = self:GetPosition()
        self._prevY = y

        -- Use NEW HP gates (Phase2HpPct / Phase3HpPct)
        self._phase = self:_ComputePhase()
        self._pendingPhase = nil
        self._transforming = false
        self._immuneDamage = false

        -- no old FSM combat loop
        self._postIntroRecoverT = 0
        self._phaseRecoverT = 0

        self:_publishBossHealth()
        self:_setBossHealthBarVisible(false)
        self._bossHealthBarShown = false
    end,

    Update = function(self, dt)
        local dtSec = toDtSec(dt)
        self._meleeCdT = math.max(0, (self._meleeCdT or 0) - dtSec)

        if not self._frozenBycinematic and not self.dead then
            self:_ForceBackInsideArena(dtSec)
        end

        if debugControls and Keyboard.IsDigitPressed(2) then
            self:ApplyHook()
        end

        if debugControls and Keyboard.IsDigitPressed(4) then
            self:ApplyHit(10)
        end

        if debugControls and Keyboard.IsDigitPressed(6) then
            self:ForceNextPhase()
        end

        -- The HP bar shows while the fight is on: after the intro, and only
        -- while the player is inside the arena, the same test that ends the
        -- fight when they leave. It goes when they walk out of the boss room
        -- and comes back when they return.
        local barWanted = (self._introDone and (not self._inIntro) and (not self.dead)
            and self:_IsPlayerInsideArena()) == true
        if barWanted ~= (self._bossHealthBarShown == true) then
            self._bossHealthBarShown = barWanted
            if barWanted then self:_publishBossHealth() end
            self:_setBossHealthBarVisible(barWanted)
        end

        -- -- Tick pending rain explosion "land" events
        -- do
        --     local q = self._pendingRainExplosions
        --     if q and #q > 0 then
        --         for i = #q, 1, -1 do
        --             local e = q[i]
        --             e.t = (e.t or 0) - dtSec
        --             if e.t <= 0 then
        --                 -- The payload contains an array of 5 targeted cells. 
        --                 -- We must spawn an explosion for each one.
        --                 if e.payload and e.payload.cells then
        --                     for _, cellNum in ipairs(e.payload.cells) do
                                
        --                         -- 1. Calculate the exact world X and Z for this specific cell
        --                         local gx, gz = self:_GetGridXZ(cellNum)
                                
        --                         -- 2. Get the ground Y level
        --                         local gy = (Nav and Nav.GetGroundY and Nav.GetGroundY(self.entityId)) or select(2, self:GetPosition()) or 0
                                
        --                         -- 3. Instantiate the prefab
        --                         local explosionPrefabId = Prefab.InstantiatePrefab(self.P3_FeatherBombExplosionPrefabPath)
        --                         local explosionPrefabTr = GetComponent(explosionPrefabId, "Transform")

        --                         -- 4. Set the prefab's position to the ground at the cell's center
        --                         if explosionPrefabTr then
        --                             explosionPrefabTr.localPosition.x = gx
        --                             explosionPrefabTr.localPosition.y = gy
        --                             explosionPrefabTr.localPosition.z = gz
        --                             explosionPrefabTr.isDirty = true
        --                         end
        --                     end
        --                 end

        --                 if _G.event_bus and _G.event_bus.publish then
        --                     _G.event_bus.publish("boss_rain_explosives", e.payload)
        --                 end
        --                 table.remove(q, i)
        --             end
        --         end
        --     end
        -- end

        -- 1) Ground physics only when NOT in air
        if not self._inAir then
            self:EnsureController()
            self:ApplyGravity(dtSec)
        end

        -- Diagnosis: the boss stayed dormant with the player four units away
        -- and the aggro check below never ran once. The counter goes here,
        -- above the first early return, so "Update is not running" can be told
        -- apart from "Update runs and returns here".
        _G.miniboss_ticks = (_G.miniboss_ticks or 0) + 1
        _G.miniboss_frozen = self._frozenBycinematic or false
        _G.miniboss_intro_done = self._introDone or false

        if self._frozenBycinematic then
            --print("[Miniboss] FROZEN by cinematic, skipping Update. lockReason=", tostring(self._lockReason), "lockT=", tostring(self._lockTimer))
            return
        end

        -- Freeze movement during cinematic
        if self._frozenBycinematic then return end

        -- 2) Tick timers always
        self._hitLockTimer = math.max(0, (self._hitLockTimer or 0) - dtSec)
        self._hurtSoundCd = math.max(0, (self._hurtSoundCd or 0) - dtSec)
        for k, v in pairs(self._moveCooldowns) do
            self._moveCooldowns[k] = math.max(0, v - dtSec)
        end

        -- Re-engage / disengage logic after intro has already happened once
        if self._introDone and not self.dead then
            local px, py, pz = self:GetPlayerPosForAI()
            local ex, ez = self:GetEnemyPosXZ()
            if not ex or not ez then
                self._combatActive = false
                self:_EndMove()
                self._moveQueue = {}
                self:_ReturnToArenaCenter(dtSec)
                return
            end

            -- if player missing, dead, or outside arena -> disengage and return center
            if self._playerDead or (not px) or (not self:_IsPlayerInsideArena()) then
                self._combatActive = false
            else
                local dx, dz = px - ex, pz - ez
                local r = self.AggroRange or 15.0
                if (dx*dx + dz*dz) <= (r*r) then
                    self._combatActive = true
                end
            end

            if not self._combatActive then
                self:_EndMove()
                self._moveQueue = {}
                self:_ReturnToArenaCenter(dtSec)
                return
            end
        end

        -- 3) Phase change detection (NEW system only)
        -- Only start a phase transition if we're not already transforming.
        -- Use computed phase
        local computed = self:_ComputePhase()
        if (computed ~= (self._phase or 1)) and (not self._transforming) and (not self.dead) then
            self:StartBossPhaseTransition(computed)
        end

        -- 4) Action lock system (handles finishing transitions + hook)
        if self._lockAction and (self._lockReason ~= "DEAD") then
            self._lockTimer = math.max(0, (self._lockTimer or 0) - dtSec)

            if self._lockTimer <= 0 then
                local reason = self._lockReason
                self:UnlockActions()

                if reason == "PHASE_TRANSFORM" then
                    self:FinishBossPhaseTransition()

                    -- NEW SYSTEM: small pause after transform before resuming phase logic
                    self._phaseRecoverT = math.max(self.RecoverDuration or 0.6, 0.35)
                end
            end
        end

        -- 5) Physics sync
        if self._controller then
            if not self._inAir then
                -- GROUND: CC is authoritative
                local pos = CharacterController.GetPosition(self._controller)
                if pos then
                    self:SetPosition(pos.x, pos.y, pos.z)
                end
            else
                -- AIR: Transform is authoritative (like FlyingEnemy)
                local x, y, z = self:GetPosition()
                if x ~= nil and CharacterController.SetPosition then
                    pcall(function()
                        CharacterController.SetPosition(self._controller, x, y, z)
                    end)
                end
            end

            -- preserve facing unless special move
            if (not self:IsInMove("DeathLotus")) and self._lastFacingRot then
                local r = self._lastFacingRot
                self:SetRotation(r.w, r.x, r.y, r.z)
            end
        end

        -- 6) Death handling
        if self.dead then
            self._deathFadeDelay = (self._deathFadeDelay or 4.0) - dtSec
            if self._deathFadeDelay <= 0 then
                if not self._deathFadeSprite then
                    local fadeEntity = Engine.GetEntityByName("GameOverFade")
                    if fadeEntity then
                        local active = GetComponent(fadeEntity, "ActiveComponent")
                        if active then active.isActive = true end
                        self._deathFadeSprite = GetComponent(fadeEntity, "SpriteRenderComponent")
                    end
                end
                if self._deathFadeSprite then
                    self._deathFadeTimer = (self._deathFadeTimer or 0) + dtSec
                    self._deathFadeSprite.alpha = math.min(self._deathFadeTimer / 1.0, 1.0)
                    if self._deathFadeSprite.alpha >= 1.0 then
                        Scene.Load("Resources/Scenes/05_EndCutscene.scene")
                    end
                end
            end
            return
        end

        -- 7) If locked, we still want queued reactions + current move to run.
        -- Only block phase decision-making below.
        local locked = self:IsActionLocked()
        local lockReason = self._lockReason

        -- 8) Aggro trigger / intro
        if not self._introDone then
            self:FacePlayer()

            local px, py, pz = self:GetPlayerPosForAI()
            -- Diagnosis: the boss stayed dormant with the player standing four
            -- units away. These say whether Update is running at all, and what
            -- position the boss believes the player is at.
            _G.miniboss_ticks = (_G.miniboss_ticks or 0) + 1
            _G.miniboss_sees_player = (px ~= nil)
            _G.miniboss_player_x = px or -999
            _G.miniboss_player_z = pz or -999
            if px then
                local ex, ez = self:GetEnemyPosXZ()
                _G.miniboss_self_x = ex
                _G.miniboss_self_z = ez
                local dx, dz = px - ex, pz - ez
                local r = self.AggroRange or 15.0

                if (dx*dx + dz*dz) <= (r*r) then
                    self._introDone = true
                    self._inIntro = true
                    --print(string.format("[Miniboss][Aggro] Player in range. Starting Battlecry."))
                    self.fsm:Change("Battlecry", self.states.Battlecry)
                    return
                end
            end
            return
        end

        -- During intro/Battlecry, let FSM run
        if self._inIntro then
            self.fsm:Update(dtSec)
        end

        -- ALWAYS tick move runtime + queued reactions (locked or not)
        self:TickMove(dtSec)
        self:TryStartQueuedMove()

        -- If locked (and not INTRO), block ONLY phase decision-making
        if locked and lockReason ~= "INTRO" then
            -- still broadcast position etc if you want
            if _G.event_bus and _G.event_bus.publish then
                local x, y, z = self:GetPosition()
                _G.event_bus.publish("enemy_position", {
                    entityId = self.entityId,
                    x = x, y = y, z = z
                })
            end
            return
        end

        -- 9) Phase dispatch (only when not locked)
        self._phase = self._phase or self:_ComputePhase()

        -- Post-intro recovery pause
        if (self._postIntroRecoverT or 0) > 0 then
            self._postIntroRecoverT = math.max(0, self._postIntroRecoverT - dtSec)
            return
        end

        -- Phase-transition recovery pause
        local phaseRecoverActive = (self._phaseRecoverT or 0) > 0
        if phaseRecoverActive then
            self._phaseRecoverT = math.max(0, self._phaseRecoverT - dtSec)
        end
        self._phaseRecoverActive = phaseRecoverActive

        if self._phase == 1 then
            self:_UpdatePhase1(dtSec)
        elseif self._phase == 2 then
            self:_UpdatePhase2(dtSec)
        elseif self._phase == 3 then
            self:_UpdatePhase3(dtSec)
        end

        -- Hover correction for air phases
        if self._inAir then
            self:MaintainHover(dtSec)
        end

        -- 10) Broadcast position
        if _G.event_bus and _G.event_bus.publish then
            local x, y, z = self:GetPosition()
            _G.event_bus.publish("enemy_position", {
                entityId = self.entityId,
                x = x, y = y, z = z
            })
        end
    end,

    -- =================================================
    -- CC lifecycle helpers
    -- =================================================
    DestroyCC = function(self)
        if self._controller then
            pcall(function()
                if CharacterController.DestroyByEntity then
                    CharacterController.DestroyByEntity(self.entityId)
                end
            end)
        end
        self._controller = nil
        self._animator:SetBool("Flying", true)
    end,

    CreateCC = function(self)
        -- Only create if we have collider+transform
        self._collider  = self._collider  or self:GetComponent("ColliderComponent")
        self._transform = self._transform or self:GetComponent("Transform")
        if not (self._collider and self._transform) then
            --print("[Miniboss] CreateCC failed: missing collider/transform")
            return false
        end

        local ok, ctrl = pcall(function()
            return CharacterController.Create(self.entityId, self._collider, self._transform)
        end)

        if ok and ctrl then
            self._controller = ctrl
            pcall(function() CharacterController.SetImmovable(self.entityId, true) end)
            pcall(function() CharacterController.SetStepUp(ctrl, 0.15, 0.3) end)
            -- Sync CC to current Transform position
            local x,y,z = self:GetPosition()
            if CharacterController.SetPosition then
                pcall(function() CharacterController.SetPosition(self._controller, x, y, z) end)
            end
            self._animator:SetBool("Flying", false)
            return true
        end

        self._controller = nil
        --print("[Miniboss] CreateCC failed: CharacterController.Create error")
        return false
    end,

    _IsInsideArenaXZ = function(self, x, z)
        local cx = self.ArenaCenterX or 0.0
        local cz = self.ArenaCenterZ or 0.0
        local r  = (self.ArenaRadius or 12.0) - (self.ArenaLeashBuffer or 0.6)

        local dx = x - cx
        local dz = z - cz
        return (dx*dx + dz*dz) <= (r*r)
    end,

    -- How far (x, z) can travel along the unit direction (dx, dz) before it
    -- leaves the arena circle. From outside, travel inward still counts. Zero
    -- if the line misses the circle or the circle is behind.
    _ArenaTravelLimit = function(self, x, z, dx, dz)
        local cx = self.ArenaCenterX or 0.0
        local cz = self.ArenaCenterZ or 0.0
        local r  = (self.ArenaRadius or 12.0) - (self.ArenaLeashBuffer or 0.6)
        local ox, oz = x - cx, z - cz
        -- The far root of |o + t*d| = r, with |d| = 1
        local b = ox * dx + oz * dz
        local disc = b * b - (ox * ox + oz * oz - r * r)
        if disc < 0 then return 0 end
        return math.max(0, -b + math.sqrt(disc))
    end,

    _ClampToArenaXZ = function(self, x, z)
        local cx = self.ArenaCenterX or 0.0
        local cz = self.ArenaCenterZ or 0.0
        local r  = (self.ArenaRadius or 12.0) - (self.ArenaLeashBuffer or 0.6)

        local dx = x - cx
        local dz = z - cz
        local d2 = dx*dx + dz*dz
        if d2 <= r*r then
            return x, z
        end

        local d = math.sqrt(d2)
        if d < 1e-6 then
            return cx, cz
        end

        local nx = dx / d
        local nz = dz / d
        return cx + nx * r, cz + nz * r
    end,

    _ForceBackInsideArena = function(self, dtSec)
        local x, y, z = self:GetPosition()
        if not x then return end
        if self:_IsInsideArenaXZ(x, z) then return end

        local tx, tz = self:_ClampToArenaXZ(x, z)

        if self._phase == 2 or self._phase == 3 or self._inAir then
            self:_MoveToXZ_Air(tx, tz, dtSec)
        else
            local oldSpeed = self.MoveSpeed
            self.MoveSpeed = (self.MoveSpeed or self.Speed or 6.0) * (self.ReturnToArenaSpeedMultiplier or 1.15)
            self:_MoveToXZ_Ground(tx, tz, dtSec)
            self.MoveSpeed = oldSpeed
        end
    end,

    _IsPlayerInsideArena = function(self)
        local px, py, pz = self:GetPlayerPosForAI()
        if not px then return false end

        local cx = self.ArenaCenterX or 0.0
        local cz = self.ArenaCenterZ or 0.0

        local baseR = (self.ArenaRadius or 12.0) - (self.ArenaLeashBuffer or 0.6)
        local extra = self.PlayerArenaExtraRadius or 0.75
        local r = baseR + extra

        local dx = px - cx
        local dz = pz - cz
        return (dx*dx + dz*dz) <= (r*r)
    end,

    _ReturnToArenaCenter = function(self, dtSec)
        local tx = self.ArenaCenterX or 0.0
        local tz = self.ArenaCenterZ or 0.0

        if self._phase == 2 or self._phase == 3 or self._inAir then
            return self:_MoveToXZ_Air(tx, tz, dtSec)
        else
            return self:_MoveToXZ_Ground(tx, tz, dtSec)
        end
    end,

    _EnterAirMode = function(self)
        self._inAir = true
        self._animator:SetBool("Flying", true)
        -- Stop gravity/vertical integration
        self._vy = 0

        -- Disable RB gravity if present (optional)
        if self._rb then
            pcall(function() self._rb.gravityFactor = 0 end)
            pcall(function() self._rb.linearVel = {x=0,y=0,z=0} end)
            pcall(function() self._rb.impulseApplied = {x=0,y=0,z=0} end)
        end

        -- IMPORTANT: remove CC so ground collision can't "drag feet"
        self:DestroyCC()
    end,

    _EnterGroundMode = function(self)
        self._inAir = false
        self._animator:SetBool("Flying", false)

        -- Re-enable RB gravity if present (optional)
        if self._rb then
            pcall(function() self._rb.gravityFactor = 1 end)
        end

        -- Create CC again (ground enemies require CC)
        self:CreateCC()

        -- Reset grounded heuristic
        local _, y, _ = self:GetPosition()
        self._prevY = y
        self._vy = 0
    end,

    -------------------------------------------------
    -- Gravity (controller-based)
    -------------------------------------------------
    ApplyGravity = function(self, dtSec)
        if not self._controller then return end
        if dtSec <= 0 then return end
        if self._inAir then return end

        -- integrate velocity
        local g = self.Gravity or -9.81
        self._vy = (self._vy or 0) + g * dtSec

        -- clamp
        local maxFall = self.MaxFallSpeed or -25.0
        if self._vy < maxFall then self._vy = maxFall end

        -- move vertically (CC handles collision)
        CharacterController.Move(self._controller, 0, self._vy * dtSec, 0)

        -- heuristic “grounded”: if Y didn’t change and we are falling, zero out vy
        local pos = CharacterController.GetPosition(self._controller)
        if pos then
            local y = pos.y
            if self._prevY ~= nil then
                local dy = y - self._prevY
                if dy >= -1e-6 and (self._vy or 0) < 0 then
                    -- on ground (or blocked)
                    self._vy = self.GroundStickVel or 0
                end
            end
            self._prevY = y
        end
    end,

    EnsureController = function(self)
        -- In air, we intentionally have NO CC (FlyingEnemy-style)
        if self._inAir then return false end
        if self._controller then return true end
        return self:CreateCC()
    end,

    -------------------------------------------------
    -- Facing (copied from EnemyAI)
    -------------------------------------------------
    ApplyRotation = function(self, w, x, y, z)
        self._lastFacingRot = { w = w, x = x, y = y, z = z }
        self:SetRotation(w, x, y, z)
    end,

    FacePlayer = function(self)
        local px, py, pz = self:GetPlayerPosForAI()
        
        -- If we can't find the player, don't try to rotate
        if not px or not pz then 
            --print("[Miniboss] Can't find player")
            return 
        end

        local ex, ez = self:GetEnemyPosXZ()
        local dx, dz = px - ex, pz - ez

        -- If the player is practically inside the boss, don't rotate (prevents snapping)
        if (dx*dx + dz*dz) < 0.1 then return end

        local q = { yawQuatFromDir(dx, dz) }
        if #q >= 4 then
            self:ApplyRotation(q[1], q[2], q[3], q[4])
        end
    end,

    _FaceXZ = function(self, tx, tz)
        local x, y, z = self:GetPosition()
        if x == nil then return end

        local dx = tx - x
        local dz = tz - z
        local d2 = dx*dx + dz*dz
        if d2 < 1e-6 then return end

        local q = { yawQuatFromDir(dx, dz) }
        if #q >= 4 then
            self:ApplyRotation(q[1], q[2], q[3], q[4])
        end
    end,

    GetEnemyPosXZ = function(self)
        if self._controller then
            local pos = CharacterController.GetPosition(self._controller)
            if pos then return pos.x, pos.z end
        end
        local x, _, z = self:GetPosition()
        return x, z
    end,

    _publishSFX = function(self, sfxType)
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("miniboss_sfx", { entityId = self.entityId, sfxType = sfxType })
        end
    end,

    _publishBossHealth = function(self)
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("bossMaxhealth", self.MaxHealth or 1)
            _G.event_bus.publish("bossCurrentHealth", self.health or 0)
        end
    end,

    _setBossHealthBarVisible = function(self, visible)
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("bossHealthBarVisible", visible == true)
        end
    end,

    GetPlayerPosForAI = function(self)
        local tr = self._playerTr
        if not tr then
            tr = Engine.FindTransformByName(self.PlayerName)
            self._playerTr = tr
        end
        if not tr then return nil end
        local pp = Engine.GetTransformPosition(tr)
        local px, py, pz = unpackPos(pp)
        if not px then
            -- try reacquire once (stale handle)
            tr = Engine.FindTransformByName(self.PlayerName)
            self._playerTr = tr
            pp = tr and Engine.GetTransformPosition(tr) or nil
            px, py, pz = unpackPos(pp)
        end
        if not px then return nil end
        return px, py, pz
    end,

    _GetGridXZ = function(self, numpad)
        local step = self.GridStep or 4.0
        local cx = self.GridCenterX or 0.0
        local cz = self.GridCenterZ or 0.0

        -- numpad layout (player POV):
        -- 7 8 9   (z+)
        -- 4 5 6
        -- 1 2 3   (z-)

        local map = {
            [1] = {-1, -1}, [2] = {0, -1}, [3] = {1, -1},
            [4] = {-1,  0}, [5] = {0,  0}, [6] = {1,  0},
            [7] = {-1,  1}, [8] = {0,  1}, [9] = {1,  1},
        }
        local v = map[numpad] or map[5]
        local ix, iz = v[1], v[2]
        return cx + ix * step, cz + iz * step
    end,

    _GetAirWaypoint = function(self, numpad)
        local x, z = self:_GetGridXZ(numpad)
        local y = (Nav and Nav.GetGroundY and Nav.GetGroundY(self.entityId)) or select(2, self:GetPosition()) or 0
        return x, y + (self.AirHeight or 4.0), z
    end,

    _PickRandomAirNumpad = function(self, avoid)
        local opts = {1,3,5,7,9}
        if avoid then
            local filtered = {}
            for i=1,#opts do if opts[i] ~= avoid then filtered[#filtered+1] = opts[i] end end
            opts = filtered
        end
        return opts[math.random(1, #opts)]
    end,

    -- True while the animator is still playing the boss being pulled down:
    -- the fall, the impact on the floor, or getting up.
    _IsGettingUp = function(self)
        return self._animator ~= nil and FALL_STATES[self._animator:GetCurrentState()] == true
    end,

    -- Whether the chain can take hold of the boss. Published when it changes,
    -- so the boss's glow can show that it cannot be hooked.
    _SetChainImmune = function(self, immune)
        immune = immune == true
        if self._immuneChain == immune then return end
        self._immuneChain = immune
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("miniboss_unhookable", { entityId = self.entityId, active = immune })
        end
    end,

    _SetInAir = function(self, inAir)
        if inAir then
            if not self._inAir then
                --print("[Miniboss] -> AIR MODE (destroy CC)")
                self:_EnterAirMode()
            end
        else
            if self._inAir then
                --print("[Miniboss] -> GROUND MODE (create CC)")
                self:_EnterGroundMode()
            end
        end
    end,

    _MoveToXZ_Ground = function(self, tx, tz, dtSec)
        if dtSec <= 0 then return false end
        if not self._controller then return true end

        local pos = CharacterController.GetPosition(self._controller)
        if not pos then return true end

        local x, y, z = pos.x, pos.y, pos.z
        local dx, dz = tx - x, tz - z

        local r = self.ArenaReturnStopDistance or 0.25
        local d2 = dx*dx + dz*dz
        if d2 <= r*r then
            pcall(function() self:StopCC() end)
            self:FacePlayer()
            return true
        end

        local d = math.sqrt(d2)
        if d < 1e-6 then
            pcall(function() self:StopCC() end)
            return true
        end

        local spd = self.MoveSpeed or 6.0
        local step = math.min(spd * dtSec, d)

        local mx = (dx / d) * step
        local mz = (dz / d) * step

        self:_FaceXZ(tx, tz)
        pcall(function()
            CharacterController.Move(self._controller, mx, 0, mz)
        end)
        return false
    end,

    _MoveToXZ_Air = function(self, tx, tz, dtSec)
        if dtSec <= 0 then return false end
        local x,y,z = self:GetPosition()
        if x == nil then return true end

        local dx, dz = tx-x, tz-z
        local r = self.AirArriveRadius or 0.25
        local d2 = dx*dx + dz*dz
        if d2 <= r*r then
            self:SetPosition(tx, y, tz)
            self:FacePlayer()
            return true
        end

        local d = math.sqrt(d2)
        if d < 1e-6 then return true end

        local spd = self.AirMoveSpeed or 9.0
        local step = math.min(spd * dtSec, d)

        self:SetPosition(x + (dx/d)*step, y, z + (dz/d)*step)
        self:FacePlayer()
        return false
    end,

    MaintainHover = function(self, dtSec)
        dtSec = toDtSec(dtSec)
        if dtSec <= 0 then return end
        if not self._inAir then return end
        if self._slamActive then return end -- don't hover while slamming down

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

        local baseH = self.HoverHeight or self.AirHeight or 4.0
        local baseTargetY = (gy or y) + baseH
        local targetY = baseTargetY + bob

        local snap = self.HoverSnapSpeed or 8.0
        local dy = targetY - y
        local maxStep = snap * dtSec
        if dy >  maxStep then dy =  maxStep end
        if dy < -maxStep then dy = -maxStep end

        self:SetPosition(x, y + dy, z)
    end,

    BeginSlamDown = function(self, mode)
        if not self._inAir then return end
        self._slamActive = true
        if mode == "Pulldown" then
            self._animator:SetTrigger("Pulldown")
        elseif mode == "DiveSmash" then
            self._animator:SetTrigger("DiveSmash")
        end
    end,

    UpdateSlamDown = function(self, dtSec, mode)
        if not self._slamActive then return false end
        dtSec = toDtSec(dtSec)
        if dtSec <= 0 then return false end

        local x,y,z = self:GetPosition()
        if x == nil then return false end

        local gy = 0
        if Nav and Nav.GetGroundY then
            local g = Nav.GetGroundY(self.entityId)
            if g ~= nil then gy = g end
        else
            gy = y
        end

        local speed = tonumber(self.SlamDownSpeed) or 16.0
        local newY = y - speed * dtSec

        if newY <= gy then
            newY = gy
            self:SetPosition(x, newY, z)
            self._slamActive = false

            if self._animator then
                if mode == "hook_slam" then
                    self:_publishSFX("groundSlam")
                    self._animator:SetTrigger("Slammed")
                end
            end

            -- Dust and rock at the point of impact. This is its own event rather
            -- than SlammedDown, which the flying enemies use: GroundSlamVFX also
            -- listens to that one and shows a crack decal it only hides on the
            -- flying enemy's Stand Up state.
            if _G.event_bus and _G.event_bus.publish then
                _G.event_bus.publish("miniboss_slammed", {
                    targetId = self.entityId,
                    posX = x, posY = newY, posZ = z,
                })
                _G.event_bus.publish("camera_shake", {
                    intensity = self.SlamShakeIntensity or 0.70,
                    duration  = self.SlamShakeDuration or 0.35,
                    frequency = self.SlamShakeFrequency or 40.0,
                })
            end

            return true
        end

        self:SetPosition(x, newY, z)
        return false
    end,

    -------------------------------------------------
    -- Phase logic
    -------------------------------------------------
    GetCurrentPhase = function(self)
        return self._phase or self:GetPhase()
    end,

    _GetHpPct = function(self)
        return (self.health or 0) / math.max(1, (self.MaxHealth or 1))
    end,

    _ComputePhase = function(self)
        local pct = self:_GetHpPct()
        if pct <= (self.Phase3HpPct or 0.33) then 
            return 3 
        end
        if pct <= (self.Phase2HpPct or 0.66) then return 2 end
        return 1
    end,

    StartBossPhaseTransition = function(self, newPhase)
        self._transforming = true
        self._pendingPhase = newPhase

        -- Shout, immune, lock
        self._immuneDamage = true
        self:LockActions("PHASE_TRANSFORM", self.PhaseTransformDuration or 2.2)

        if self._animator then self._animator:SetTrigger("Taunt") end
        --print("[Miniboss] StartBossPhaseTransition ->", newPhase, "duration=", tostring(self.PhaseTransformDuration))
        self:_publishSFX("taunt")

        self:_TriggerBossPhaseShake()
        self:_TriggerBossPhaseFx()
    end,

    FinishBossPhaseTransition = function(self)
        local newPhase = self._pendingPhase or self:_ComputePhase()
        self._phase = newPhase
        self._pendingPhase = nil
        self._transforming = false
        self._immuneDamage = false

        --print("[Miniboss] FinishBossPhaseTransition -> phase=", tostring(newPhase))

        if newPhase == 2 then
            self:EnterPhase2_Air()
        elseif newPhase == 3 then
            self._animator:SetBool("Phase3", true)
            self:EnterPhase3_Air()
        end
    end,

    ForceNextPhase = function(self)
        if self.dead then return end
        if self._inIntro then return end
        if self._transforming then return end

        local curPhase = self._phase or self:_ComputePhase()
        local nextPhase = curPhase + 1

        if nextPhase > 3 then
            --print("[Miniboss][Cheat] Already at final phase.")
            return
        end

        local maxHp = self.MaxHealth or 1

        -- Force HP to the target phase threshold so the internal phase logic matches.
        -- Use a tiny epsilon below threshold so _ComputePhase() definitely returns nextPhase.
        if nextPhase == 2 then
            self.health = (maxHp * (self.Phase2HpPct or 0.66)) - 0.01
        elseif nextPhase == 3 then
            self.health = (maxHp * (self.Phase3HpPct or 0.33)) - 0.01
        end

        -- Optional cleanup so the transition is clean
        self._hitLockTimer = 0
        self._moveQueue = {}
        self:_EndMove()

        --print(string.format(
        --    "[Miniboss][Cheat] Forcing phase %d at hp=%.2f/%.2f",
        --    nextPhase, self.health, maxHp
        --))

        self:StartBossPhaseTransition(nextPhase)
    end,

    -------------------------------------------------
    -- Move selection
    -------------------------------------------------
    ChooseMove = function(self)
        local phase = self._phase
        local pool = {}
        local total = 0

        for i = 1, #MOVE_ORDER do
            local name = MOVE_ORDER[i]
            local move = MOVES[name]
            if move then
                local w = (move.weights and move.weights[phase]) or 0
                if w > 0 and not self:IsMoveOnCooldown(name) then
                    total = total + w
                    pool[#pool+1] = { name=name, weight=w, def=move }
                end
            end
        end

        if total <= 0 then
            return nil
        end

        -- Weighted roll: weight is literally the "odds" in the phase
        local r = math.random() * total
        local acc = 0
        for i = 1, #pool do
            acc = acc + pool[i].weight
            if r <= acc then
                return pool[i].name, pool[i].def, pool[i].weight, total, r, phase
            end
        end

        -- Fallback (shouldn't happen, but safe)
        local last = pool[#pool]
        return last.name, last.def, last.weight, total, r, phase
    end,

    GetMoveWeightForPhase = function(self, moveName, phase)
        local def = MOVES[moveName]
        if not def or not def.weights then return 0 end
        return def.weights[phase] or 0
    end,

    IsMoveOnCooldown = function(self, name)
        return self._moveCooldowns[name] and self._moveCooldowns[name] > 0
    end,

    StartMoveCooldown = function(self, name, cd)
        self._moveCooldowns[name] = cd
    end,

    IsCurrentMoveFinished = function(self)
        return self._moveFinished == true
    end,

    IsInMove = function(self, kind)
        return (self._moveFinished == false) and self._move and (self._move.kind == kind)
    end,

    GetNextMoveReadyTime = function(self)
        local soonest = math.huge
        for _, cd in pairs(self._moveCooldowns) do
            if cd and cd > 0 and cd < soonest then
                soonest = cd
            end
        end
        if soonest == math.huge then return 0 end
        return soonest
    end,

    IsActionLocked = function(self)
        return self._lockAction == true
    end,

    LockActions = function(self, reason, duration)
        reason = reason or "LOCKED"
        duration = duration or 0

        if self._lockAction then
            local curR = self._lockReason
            if _lockPriority(curR) > _lockPriority(reason) then
                -- current lock is stronger; keep it
                return
            end
            if _lockPriority(curR) == _lockPriority(reason) then
                -- same priority: extend (max)
                self._lockTimer = math.max(self._lockTimer or 0, duration)
                self._lockReason = reason
                return
            end
        end

        -- take/replace lock
        self._lockAction = true
        self._lockReason = reason
        self._lockTimer  = duration
    end,

    UnlockActions = function(self)
        self._lockAction = false
        self._lockReason = nil
        self._lockTimer  = 0
    end,

    ApplyHit = function(self, dmg, hitType)
        if self.dead then return end
        
        if self._inIntro then
            --print("[MinibossAI] ApplyHit blocked: _inIntro")
            return
        end
        if self._transforming then
            --print("[MinibossAI] ApplyHit blocked: _transforming")
            return
        end
        if self._immuneDamage then
            --print("[MinibossAI] ApplyHit blocked: _immuneDamage")
            return
        end
        if (self._hitLockTimer or 0) > 0 then
            --print("[MinibossAI] ApplyHit blocked: iFrame", self._hitLockTimer)
            return
        end

        --print("[MinibossAI] ApplyHit called", dmg, hitType)

        if SWING_HIT_TYPES[hitType] then
            self._hitLockTimer = self.ComboHitIFrame or 0.25
        else
            self._hitLockTimer = self.HitIFrame or 0.2
        end
        self.health = math.max(0, (self.health or 0) - (dmg or 1))

        self:_publishBossHealth()

        --print(string.format("[Miniboss][Hit] dmg=%s hp=%.1f/%.1f", tostring(dmg or 1), self.health, self.MaxHealth))

        if self.health <= 0 then
            self:Die()
            return
        end

        -- The boss cannot be stunned. A hit never stops an attack, and between
        -- attacks, with no move under way, it only flinches, which does not
        -- hold up its next move.
        if self:IsCurrentMoveFinished() then
            self:_PlayHurtAnim()
        end

        -- 2) Queue shout ONLY if we crossed a Phase 1 checkpoint
        if (self._phase or self:_ComputePhase()) == 1 then
            local hpPct = self:_GetHpPct()

            local function queueShoutOnce()
                self:EnqueueMove("ShoutAOE", {
                    windup    = self.ShoutWindup or 0.55,
                    postDelay = self.ShoutPostDelay or 0.25,
                    radius    = self.ShoutRadius or 4.0,
                    dmg       = self.ShoutDamage or 2,
                    kb        = self.ShoutKnockback or 240.0,
                })
                --print("[MinibossAI] Queued ShoutAOE")
            end

            if (not self._p1DidShout90) and hpPct <= (self.P1_Shout1Pct or 0.90) then
                self._p1DidShout90 = true
                queueShoutOnce()
            elseif (not self._p1DidShout75) and hpPct <= (self.P1_Shout2Pct or 0.75) then
                self._p1DidShout75 = true
                queueShoutOnce()
            end
        end

        -- Play hurt SFX, at most once every HurtSoundCooldown seconds
        if (self._hurtSoundCd or 0) <= 0 then
            self:_publishSFX("hurt")
            self._hurtSoundCd = self.HurtSoundCooldown or 3.5
        end

        -- If we crossed a phase threshold, start NEW transition immediately
        local computed = self:_ComputePhase()
        if (computed ~= (self._phase or 1)) and (not self._transforming) and (not self.dead) then
            self:StartBossPhaseTransition(computed)
        end
    end,

    -- The chain's hold on the boss. In phase 2 it pulls the boss out of the
    -- air and slams it down, which is the one way to bring it within reach.
    -- Anywhere else it does not hold it: on the ground the boss is too heavy
    -- to be stopped by it, like any other hit, and in phase 3 it cannot be
    -- hooked out of the air at all, which its glow shows.
    ApplyHook = function(self)
        if self.dead then return end
        if self._inIntro then return end
        if self._immuneChain then return end

        if self._phase == 2 and self._inAir then
            -- Cancel current air attack immediately
            self:_EndMove()

            -- Start falling RIGHT NOW
            self:BeginSlamDown("Pulldown")
        end
    end,

    Die = function(self)
        if self.dead then return end
        self.dead = true

        self:_publishBossHealth()
        self:_setBossHealthBarVisible(false)
        self._bossHealthBarShown = false

        -- Play death SFX
        self:_publishSFX("death")

        -- hard-lock forever
        self._lockAction = true
        self._lockReason = "DEAD"
        self._lockTimer = 999999

        -- stop movement (optional)
        if self._controller then
            pcall(function() CharacterController.Move(self._controller, 0, 0, 0) end)
        end
        self._vy = 0

        -- if self.ClipDeath and self.ClipDeath >= 0 and self.PlayClip then
        --     self:PlayClip(self.ClipDeath, false)
        -- end
        self._animator:SetTrigger("Death")

        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("boss_killed")
        end

        --print("[Miniboss][Death] DEAD")
    end,

    -------------------------------------------------
    -- Cleanup (copied style from EnemyAI)
    -------------------------------------------------
    OnDisable = function(self)
        if self._controller then
            pcall(function()
                if CharacterController.DestroyByEntity then
                    CharacterController.DestroyByEntity(self.entityId)
                end
            end)
        end
        self._controller = nil

        if self._rb then
            pcall(function() self._rb.linearVel = { x=0, y=0, z=0 } end)
            pcall(function() self._rb.impulseApplied = { x=0, y=0, z=0 } end)
        end
        self._lastFacingRot = nil

        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._damageSub then pcall(function() _G.event_bus.unsubscribe(self._damageSub) end) end
            if self._comboDamageSub then pcall(function() _G.event_bus.unsubscribe(self._comboDamageSub) end) end
            if self._hookSub then pcall(function() _G.event_bus.unsubscribe(self._hookSub) end) end
            if self._meleeHitSub then pcall(function() _G.event_bus.unsubscribe(self._meleeHitSub) end) end
            if self._freezeEnemySub then pcall(function() _G.event_bus.unsubscribe(self._freezeEnemySub) end) end
            if self._chainEndpointHitSub then pcall(function() _G.event_bus.unsubscribe(self._chainEndpointHitSub) end) end
            if self._chainEnemyHookedSub then pcall(function() _G.event_bus.unsubscribe(self._chainEnemyHookedSub) end) end
            if self._playerDeadSub then pcall(function() _G.event_bus.unsubscribe(self._playerDeadSub) end) self._playerDeadSub = nil end
            if self._respawnPlayerSub then pcall(function() _G.event_bus.unsubscribe(self._respawnPlayerSub) end) self._respawnPlayerSub = nil end
        end
        self._damageSub = nil
        self._comboDamageSub = nil
        self._hookSub = nil
        self._meleeHitSub = nil
        self._freezeEnemySub = nil
        self._chainEndpointHitSub = nil
        self._chainEnemyHookedSub = nil
        self._frozenBycinematic = false
    end,

    OnDestroy = function(self)
        if self._controller then
            pcall(function()
                CharacterController.DestroyByEntity(self.entityId)
            end)
            self._controller = nil
        end
        self._lastFacingRot = nil

        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._damageSub then pcall(function() _G.event_bus.unsubscribe(self._damageSub) end) end
            if self._comboDamageSub then pcall(function() _G.event_bus.unsubscribe(self._comboDamageSub) end) end
            if self._hookSub then pcall(function() _G.event_bus.unsubscribe(self._hookSub) end) end
            if self._meleeHitSub then pcall(function() _G.event_bus.unsubscribe(self._meleeHitSub) end) end
            if self._freezeEnemySub then pcall(function() _G.event_bus.unsubscribe(self._freezeEnemySub) end) end
            if self._chainEndpointHitSub then pcall(function() _G.event_bus.unsubscribe(self._chainEndpointHitSub) end) end
            if self._chainEnemyHookedSub then pcall(function() _G.event_bus.unsubscribe(self._chainEnemyHookedSub) end) end
            if self._playerDeadSub then pcall(function() _G.event_bus.unsubscribe(self._playerDeadSub) end) self._playerDeadSub = nil end
            if self._respawnPlayerSub then pcall(function() _G.event_bus.unsubscribe(self._respawnPlayerSub) end) self._respawnPlayerSub = nil end
        end
        self._damageSub = nil
        self._comboDamageSub = nil
        self._hookSub = nil
        self._meleeHitSub = nil
        self._freezeEnemySub = nil
        self._chainEndpointHitSub = nil
        self._chainEnemyHookedSub = nil
    end,

    -------------------------------------------------
    -- Knife helpers (EnemyAI-style token reservation)
    -------------------------------------------------
    _NewVolleyToken = function(self)
        self._knifeVolleyId = (self._knifeVolleyId or 0) + 1
        return tostring(self.entityId) .. ":" .. tostring(self._knifeVolleyId)
    end,

    _FreeReserved = function(self, knives)
        if not knives then return end
        for i=1,#knives do
            if knives[i] then
                knives[i].reserved = false
                knives[i]._reservedToken = nil
            end
        end
    end,

    _GetPlayerPos = function(self, yOffset)
        local tr = self._playerTr
        if not tr then
            tr = Engine.FindTransformByName(self.PlayerName)
            self._playerTr = tr
        end
        if not tr then return nil end

        local pp = Engine.GetTransformPosition(tr)
        local px, py, pz = unpackPos(pp)

        if not px then
            -- try reacquire once (stale handle)
            tr = Engine.FindTransformByName(self.PlayerName)
            self._playerTr = tr
            if not tr then return nil end

            pp = Engine.GetTransformPosition(tr)
            px, py, pz = unpackPos(pp)
        end

        if not px then return nil end

        yOffset = yOffset or 0.5
        return px, (py or 0) + yOffset, pz
    end,

    _GetSpawnPos = function(self)
        local ex, ey, ez
        if self._controller then
            local pos = CharacterController.GetPosition(self._controller)
            if pos then ex, ey, ez = pos.x, pos.y, pos.z end
        end
        if not ex then
            ex, ey, ez = self:GetPosition()
        end
        if not ex then return nil end
        return ex, (ey or 0) + 1.8, ez
    end,

    _DirToPlayerXZ = function(self)
        local px, py, pz = self:_GetPlayerPos()
        if not px then return nil end

        local ex, ez = self:GetEnemyPosXZ()
        local dx, dz = (px - ex), (pz - ez)
        local len = math.sqrt(dx*dx + dz*dz)
        if len < 1e-6 then len = 1 end
        return dx/len, dz/len
    end,

    _LaunchKnife = function(self, knife, sx, sy, sz, tx, ty, tz, token, tag)
        if not knife then
            --print("[Miniboss][Knife] Launch FAILED: knife=nil")
            return false
        end

        -- Slow ONLY Phase 3 feathers (tags like "P3F1", "P3F9", etc.)
        if tag and tostring(tag):sub(1, 3) == "P3F" then
            local scale = tonumber(self.P3_FeatherSpeedScale) or 1.0

            -- Try common speed fields without hard-crashing if they don't exist
            pcall(function()
                if knife.SetSpeed then knife:SetSpeed(scale) end
            end)
            pcall(function()
                if knife.SetSpeedMultiplier then knife:SetSpeedMultiplier(scale) end
            end)
            pcall(function()
                if knife.speed ~= nil then knife.speed = knife.speed * scale end
            end)
            pcall(function()
                if knife.moveSpeed ~= nil then knife.moveSpeed = knife.moveSpeed * scale end
            end)
            pcall(function()
                if knife.projectileSpeed ~= nil then knife.projectileSpeed = knife.projectileSpeed * scale end
            end)
            pcall(function()
                if knife.flightSpeed ~= nil then knife.flightSpeed = knife.flightSpeed * scale end
            end)
        end

        local ok = knife:Launch(sx, sy, sz, tx, ty, tz, token, tag, "BOSS")
        if not ok then
            --print(string.format("[Miniboss][Knife] Launch FAILED tag=%s token=%s", tostring(tag), tostring(token)))
        end
        return ok
    end,

    -- 3-shot volley: center aimed + L/R perpendicular offsets
    SpawnKnifeVolley3 = function(self, spread)
        spread = spread or 1.0

        local knives = requestUpTo(3)
        if not knives then
            --print("[Miniboss][Knife] No knives available in pool")
            return false
        end

        local px, py, pz = self:_GetPlayerPos()
        if not px then
            self:_FreeReserved(knives)
            return false
        end

        local sx, sy, sz = self:_GetSpawnPos()
        if not sx then
            self:_FreeReserved(knives)
            return false
        end

        local ex, ez = self:GetEnemyPosXZ()
        local dx, dz = (px - ex), (pz - ez)
        local len = math.sqrt(dx*dx + dz*dz)
        if len < 1e-6 then len = 1 end
        dx, dz = dx / len, dz / len
        local rx, rz = -dz, dx

        local token = self:_NewVolleyToken()

        -- IMPORTANT: only stamp token onto the knives we actually got
        for i=1, #knives do
            knives[i]._reservedToken = token
            knives[i].reserved = true
        end

        local targets = {
            { px,              py, pz,              "C" },
            { px - rx*spread,  py, pz - rz*spread,  "L" },
            { px + rx*spread,  py, pz + rz*spread,  "R" },
        }

        local okAny = false
        for i=1, #knives do
            local t = targets[i]
            local ok = self:_LaunchKnife(knives[i], sx, sy, sz, t[1], t[2], t[3], token, t[4])
            okAny = okAny or ok
            if not ok then
                -- If this knife didn't launch, free it immediately
                knives[i]:Reset("LAUNCH_FAIL")
            end
        end

        if not okAny then
            -- Nothing launched -> make sure we don't leak reservations
            self:_FreeReserved(knives)
            return false
        end

        return true
    end,

    -- Single aimed knife at player's current position (aim locked per shot)
    SpawnKnifeSingleAtPlayer = function(self)
        local knives = KnifePool.RequestMany(1)
        if not knives or not knives[1] then 
            --print("[Miniboss][Knife] RequestMany(1) FAILED")
            return false
        end
        local k = knives[1]

        local px, py, pz = self:_GetPlayerPos()
        if not px then
            k.reserved = false
            k._reservedToken = nil
            return false
        end

        local sx, sy, sz = self:_GetSpawnPos()
        if not sx then
            k.reserved = false
            k._reservedToken = nil
            return false
        end

        local token = self:_NewVolleyToken()
        k._reservedToken = token
        k.reserved = true

        local ok = self:_LaunchKnife(k, sx, sy, sz, px, py, pz, token, "S")

        if not ok then
            k:Reset()
            return false
        end
        return true
    end,

    -- Shoot 1 knife at a specific world position (used for Phase 3 feathers)
    SpawnKnifeSingleAtWorld = function(self, tx, ty, tz, tag)
        local knives = KnifePool.RequestMany(1)
        if not knives or not knives[1] then
            --print("[Miniboss][Knife] RequestMany(1) FAILED (world)")
            return false
        end
        local k = knives[1]

        local sx, sy, sz = self:_GetSpawnPos()
        if not sx then
            k.reserved = false
            k._reservedToken = nil
            return false
        end

        local token = self:_NewVolleyToken()
        k._reservedToken = token
        k.reserved = true

        local ok = self:_LaunchKnife(k, sx, sy, sz, tx, ty, tz, token, tag or "P3F")

        if not ok then
            k:Reset()
            return false
        end
        return true
    end,

    -- 8-fan, no centered aimed shot: targets are perpendicular offsets only
    SpawnKnifeFan8_NoCenter = function(self, spread1, spread2, spread3, spread4)
        spread1 = spread1 or 0.8
        spread2 = spread2 or 1.6
        spread3 = spread3 or 2.4
        spread4 = spread4 or 3.2

        local knives = KnifePool.RequestMany(8)
        if not knives then
            --print("[Miniboss][Knife] RequestMany(8) FAILED")
            return false
        end

        local px, py, pz = self:_GetPlayerPos()
        if not px then
            self:_FreeReserved(knives)
            return false
        end

        local sx, sy, sz = self:_GetSpawnPos()
        if not sx then
            self:_FreeReserved(knives)
            return false
        end

        local ex, ez = self:GetEnemyPosXZ()
        local dx, dz = (px - ex), (pz - ez)
        local len = math.sqrt(dx*dx + dz*dz)
        if len < 1e-6 then len = 1 end
        dx, dz = dx/len, dz/len

        local rx, rz = -dz, dx

        local token = self:_NewVolleyToken()
        for i=1,8 do
            knives[i]._reservedToken = token
            knives[i].reserved = true
        end

        local targets = {
            { px - rx*spread4, py, pz - rz*spread4, "L4" },
            { px - rx*spread3, py, pz - rz*spread3, "L3" },
            { px - rx*spread2, py, pz - rz*spread2, "L2" },
            { px - rx*spread1, py, pz - rz*spread1, "L1" },
            { px + rx*spread1, py, pz + rz*spread1, "R1" },
            { px + rx*spread2, py, pz + rz*spread2, "R2" },
            { px + rx*spread3, py, pz + rz*spread3, "R3" },
            { px + rx*spread4, py, pz + rz*spread4, "R4" },
        }

        local okAll = true
        for i=1,8 do
            local t = targets[i]
            okAll = self:_LaunchKnife(knives[i], sx, sy, sz, t[1], t[2], t[3], token, t[4]) and okAll
        end

        if not okAll then
            for i=1,8 do if knives[i] then knives[i]:Reset() end end
            return false
        end
        return true
    end,

    -- forward single shot (not aimed): shoot 1 knife in facing direction
    SpawnForwardSingle = function(self, fx, fz, range, yOffset)
        range = range or 12.0

        local knives = KnifePool.RequestMany(1)
        if not knives or not knives[1] then
            --print("[Miniboss][Knife] RequestMany(1) FAILED")
            return false
        end

        local k = knives[1]

        local sx, sy, sz = self:_GetSpawnPos()
        if not sx then
            k.reserved = false
            k._reservedToken = nil
            return false
        end

        local len = math.sqrt((fx or 0)*(fx or 0) + (fz or 0)*(fz or 0))
        if len < 1e-6 then
            k.reserved = false
            k._reservedToken = nil
            return false
        end
        fx, fz = fx / len, fz / len

        local _, py, _ = self:_GetPlayerPos(yOffset or 0.0)
        local targetY = py or sy

        local tx = sx + fx * range
        local ty = targetY
        local tz = sz + fz * range

        local token = self:_NewVolleyToken()
        k._reservedToken = token
        k.reserved = true

        local ok = self:_LaunchKnife(k, sx, sy, sz, tx, ty, tz, token, "F")

        if not ok then
            k:Reset()
            return false
        end

        return true
    end,

    _DoShoutAOE = function(self)
        if self._animator then self._animator:SetTrigger("Taunt") end
        self:_publishSFX("taunt")

        self:_TriggerBossShoutShake()
        self:_TriggerBossShoutFx()

        if _G.event_bus and _G.event_bus.publish then
            local x,y,z = self:GetPosition()
            --print("[MinibossAI] Casting boss_shout_aoe")
            _G.event_bus.publish("boss_shout_aoe", {
                entityId = self.entityId,
                x=x,y=y,z=z,
                radius = self.ShoutRadius or 4.0,
                dmg = self.ShoutDamage or 2,
                kb = self.ShoutKnockback or 240.0,
            })
        end
    end,

    _TriggerBossShoutShake = function(self)
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("camera_shake", {
                intensity = self.ShoutShakeIntensity or 0.85,
                duration  = self.ShoutShakeDuration or 0.55,
                frequency = self.ShoutShakeFrequency or 24.0,
            })
        end
    end,

    _TriggerBossShoutFx = function(self)
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("fx_chromatic", {
                intensity = self.ShoutFxChromaticIntensity or 4.45,
                duration  = self.ShoutFxChromaticDuration or 0.55,
            })

            _G.event_bus.publish("fx_blur", {
                intensity = self.ShoutFxBlurIntensity or 0.35,
                radius    = self.ShoutFxBlurRadius or 2.5,
                duration  = self.ShoutFxBlurDuration or 0.25,
            })
        end
    end,

    _TriggerBossPhaseShake = function(self)
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("camera_shake", {
                intensity = self.PhaseShakeIntensity or 0.85,
                duration  = self.PhaseShakeDuration or 2.55,
                frequency = self.PhaseShakeFrequency or 48.0,
            })
        end
    end,

    _TriggerBossPhaseFx = function(self)
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("fx_chromatic", {
                intensity = self.PhaseFxChromaticIntensity or 2.75,
                duration  = self.PhaseFxChromaticDuration or 2.55,
            })

            _G.event_bus.publish("fx_blur", {
                intensity = self.PhaseFxBlurIntensity or 0.55,
                radius    = self.PhaseFxBlurRadius or 3.5,
                duration  = self.PhaseFxBlurDuration or 1.25,
            })
        end
    end,

    -- countForLunge: phase 1 counts its melee attacks toward the charged slash
    _DoMeleeAttack = function(self, countForLunge)
        self:_BeginMove("BossMelee", {
            windup = self.BossMeleeWindup or 0.4,
            range  = self.BossMeleeRange or 2.95,
            dmg    = 4,
            postDelay = 1.0,
            countForLunge = countForLunge == true,
        })
        self:_TriggerAttackAnim(self._move, "Melee")
    end,

    -------------------------------------------------
    -- Move runtime
    -------------------------------------------------
    _BeginMove = function(self, kind, data)
        self._move = data or {}
        self._move.kind = kind
        self._move.t = 0
        self._move.step = 0
        self._moveFinished = false

        -- A hurt trigger left by an earlier hit must not pull this attack
        -- into a hurt animation. Only a hit during the attack may stop it.
        if ATTACK_MOVE_KINDS[kind] then
            self:_ClearTriggers(HURT_TRIGGERS)
        end
    end,

    _EndMove = function(self)
        self._move = nil
        self._moveFinished = true
        self.currentMove = nil
        self.currentMoveDef = nil
    end,

    _CancelCurrentAttackMove = function(self, reason)
        if self._moveFinished or not self._move then return end

        local kind = self._move.kind
        if not kind then return end

        -- Only cancel actual attack/cast moves, not passive hurt react
        if ATTACK_MOVE_KINDS[kind] then
            self._move.cancelled = true
            self._move.cancelReason = reason or "INTERRUPTED"
            if ATTACK_MOVE_TRIGGER[kind] then
                self:_ClearTriggers({ ATTACK_MOVE_TRIGGER[kind] })
            end
            self:_EndMove()
        end
    end,

    -- Follows the animator into an attack's animation, so what the attack
    -- does is timed from the animation instead of from a timer that runs
    -- whatever the animator is doing. m.lastState and m.lastStateTime start as
    -- the animator's state from before the attack's trigger was set, so an
    -- animation already playing is not taken for the new one. Returns
    -- "waiting" until the animation starts, "timeout" if it has not started
    -- within AttackAnimStartTimeout, then "playing" with the seconds into it,
    -- and "left" once the animator has moved on from it.
    _FollowAttackAnim = function(self, m, stateName, dtSec)
        if not self._animator then return "playing", m.t end
        local state = self._animator:GetCurrentState()
        local stateTime = self._animator:GetStateTime()
        local waiting = false
        if not m.animStarted then
            -- Entered fresh: from another state, or from the end of an earlier
            -- play of the same one, which restarts the state time.
            if state == stateName
               and (m.lastState ~= stateName or stateTime < (m.lastStateTime or 0)) then
                m.animStarted = true
            else
                waiting = true
                m.animWaitT = (m.animWaitT or 0) + dtSec
            end
        end
        m.lastState, m.lastStateTime = state, stateTime
        if waiting then
            if m.animWaitT >= (self.AttackAnimStartTimeout or 1.0) then return "timeout" end
            return "waiting"
        end
        if state ~= stateName then return "left" end
        return "playing", stateTime
    end,

    -- Sets an attack's animation trigger, first noting the animator's state
    -- for _FollowAttackAnim. The animator takes the trigger later in the
    -- frame, so the state noted here is the one from before the attack.
    _TriggerAttackAnim = function(self, m, trigger)
        m.animStarted, m.animWaitT = false, 0
        if not self._animator then return end
        m.lastState = self._animator:GetCurrentState()
        m.lastStateTime = self._animator:GetStateTime()
        self._animator:SetTrigger(trigger)
    end,

    -- Clears animator triggers. SetBool on a Trigger parameter keeps its
    -- Trigger type and stores false, and a false trigger never fires.
    _ClearTriggers = function(self, names)
        if not self._animator then return end
        for _, name in ipairs(names) do
            self._animator:SetBool(name, false)
        end
    end,

    -- Plays one random hurt animation, if the animator is in a state that
    -- can take it. In any other state the trigger would wait and pull a later
    -- attack into the hurt animation, so none is set.
    _PlayHurtAnim = function(self)
        if not self._animator then return end
        if not TAKES_HURT[self._animator:GetCurrentState()] then return end
        self:_ClearTriggers(HURT_TRIGGERS)
        self._animator:SetTrigger(HURT_TRIGGERS[math.random(1, #HURT_TRIGGERS)])
    end,

    TickMove = function(self, dtSec)
        if self._moveFinished or not self._move then return end
        local m = self._move
        m.t = (m.t or 0) + dtSec

        -- =========================
        -- Queued reaction: Shout AOE (delayed hit)
        -- =========================
        if m.kind == "ShoutAOE" then
            if m.step == 0 then
                m.step = 1
                m.fireAt = m.windup or 0.55

                -- start shout anim now
                if self._animator then self._animator:SetTrigger("Taunt") end
                self:_publishSFX("taunt")
            end

            if not m.didFire and m.t >= (m.fireAt or 0) then
                m.didFire = true

                self:_TriggerBossShoutShake()
                self:_TriggerBossShoutFx()

                if _G.event_bus and _G.event_bus.publish then
                    local x,y,z = self:GetPosition()
                    --print("[MinibossAI] ShoutAOE HIT (queued + delayed)")
                    _G.event_bus.publish("boss_shout_aoe", {
                        entityId = self.entityId,
                        x=x,y=y,z=z,
                        radius = m.radius or (self.ShoutRadius or 5.5),
                        dmg    = m.dmg or (self.ShoutDamage or 2),
                        kb     = m.kb or (self.ShoutKnockback or 18.0),
                    })
                end
            end

            if m.didFire and m.t >= ((m.fireAt or 0) + (m.postDelay or 0.25)) then
                self:_EndMove()
            end
            return
        end

        if m.kind == "BossMelee" then
            -- The slash is timed from the start of the swing animation and
            -- lands only while that swing is playing. A swing held up by a
            -- hurt animation lands later, and a swing the animator leaves
            -- before the claw connects deals no damage.
            if m.step == 0 then
                self:FacePlayer()
                m.step = 1
                m.hitAt = (m.windup or 0.85)
            end

            local anim, swingTime = self:_FollowAttackAnim(m, SWING_STATE, dtSec)
            if anim == "waiting" then return end
            if anim == "timeout" then
                self:_CancelCurrentAttackMove("SWING_NOT_STARTED")
                return
            end
            if anim == "left" and not m.didHit then
                self:_CancelCurrentAttackMove("SWING_LEFT")
                return
            end

            if not m.didHit and swingTime and swingTime >= (m.hitAt or 0) then
                m.didHit = true
                m.hitT = m.t
                -- Counted when the claw comes down, as a throw is when its
                -- knives leave
                if m.countForLunge then
                    self._p1MeleesClose = (self._p1MeleesClose or 0) + 1
                end

                --print("Do u see this?")
                -- CLAW VFX HERE
                if _G.event_bus then
                    local x, y, z = self:GetPosition()
                    local qW, qX, qY, qZ = self:GetRotation()
                    
                    _G.event_bus.publish("miniboss_vfx", {
                        pos = {x = x, y = y, z = z},
                        rot = {w = qW, x = qX, y = qY, z = qZ},
                        entityId = self.entityId,
                    })
                end


                if _G.event_bus and _G.event_bus.publish then
                    local ex, ey, ez = self:GetPosition()
                    _G.event_bus.publish("miniboss_slash", {
                        entityId = self.entityId,
                        x = ex, y = ey, z = ez,
                        radius = m.slashRadius or 1.4,
                        dmg = m.dmg or 4,

                        kbStrength = m.kbStrength or 8.0,
                        kbUp = 0.0,
                    })
                end
                self:_publishSFX("meleeAttack")
            end

            if m.didHit and m.t >= (m.hitT + (m.postDelay or 0.4)) then
                self:_EndMove()
            end
            return
        end

        if m.kind == "P1RangedCharged" then
            -- The knives leave at a set point in the throw animation, and only
            -- while it plays. On a timer alone they left while the boss was
            -- still getting up off the floor after a slam, with no throw.
            if m.step == 0 then
                self:FacePlayer()
                self:_TriggerAttackAnim(m, "Ranged")
                m.step = 1
                m.fireAt = (m.charge or 0.75)
            end
            local throwTime
            if not m.didFire then
                local anim
                anim, throwTime = self:_FollowAttackAnim(m, THROW_STATE, dtSec)
                if anim == "waiting" then return end
                if anim ~= "playing" then
                    self:_CancelCurrentAttackMove(anim == "timeout" and "THROW_NOT_STARTED" or "THROW_LEFT")
                    return
                end
            end
            if not m.didFire and throwTime >= (m.fireAt or 0.75) then
                m.didFire = true
                self:SpawnKnifeVolley3(m.spread or 0.6)
                m.doneAt = m.t + (m.postDelay or 0.35)
                -- Counted when the knives leave, so a throw a hit cancelled
                -- does not bring the charged slash closer
                if m.countForLunge then
                    self._p1ThrowsAway = (self._p1ThrowsAway or 0) + 1
                    -- The player kept away long enough to be thrown at, so
                    -- the melee attacks before it no longer run in a row
                    self._p1MeleesClose = 0
                end
            end
            if m.doneAt and m.t >= m.doneAt then self:_EndMove() end
            return
        end

        -------------------------------------------------
        -- Move1: Basic Attack (single volley)
        -------------------------------------------------
        if m.kind == "Basic" then
            -- fire once at start
            if m.step == 0 then
                self:FacePlayer()
                --print("[MinibossAI] SPAWNING BASIC")
                self:SpawnKnifeVolley3(m.spread or 1.0)
                m.step = 1
                m.doneAt = m.t + (m.postDelay or 0.35)
            end
            if m.doneAt and m.t >= m.doneAt then
                self:_EndMove()
            end
            return
        end

        -------------------------------------------------
        -- Move2: 5 Bursts (aim per burst)
        -------------------------------------------------
        if m.kind == "BurstFire" then
            local burstInterval = m.interval or 0.18
            local bursts = m.bursts or 5

            if m.step == 0 then
                m.nextShotT = 0
                m.shotsDone = 0
                m.step = 1
            end

            if m.t >= (m.nextShotT or 0) and (m.shotsDone or 0) < bursts then
                self:FacePlayer()
                --print("[MinibossAI] SPAWNING BURSTFIRE")
                self:SpawnKnifeSingleAtPlayer()
                m.shotsDone = m.shotsDone + 1
                m.nextShotT = m.t + burstInterval
            end

            if (m.shotsDone or 0) >= bursts then
                if not m.finishT then
                    m.finishT = m.t + (m.postDelay or 0.45)
                elseif m.t >= m.finishT then
                    self:_EndMove()
                end
            end
            return
        end

        -------------------------------------------------
        -- Move3: 4 Fan (no center)
        -------------------------------------------------
        if m.kind == "AntiDodge" then
            if m.step == 0 then
                self:FacePlayer()
                --print("[MinibossAI] SPAWNING AntiDodge")
                self:SpawnKnifeFan8_NoCenter(m.spread1 or 0.9, m.spread2 or 1.8, m.spread3 or 2.7, m.spread4 or 3.6)
                m.step = 1
                m.doneAt = m.t + (m.postDelay or 0.45)
            end
            if m.doneAt and m.t >= m.doneAt then
                self:_EndMove()
            end
            return
        end

        -------------------------------------------------
        -- Move4: Fate Sealed (charge-up -> dash -> slash -> recover)
        -------------------------------------------------
        if m.kind == "FateSealed" then
            local dashDur = m.dashDur or 0.4
            -- Seconds into the dash when the claw connects
            local contactT = dashDur * (m.slashAt or 0.9)

            -- Step 0: charge-up (telegraph)
            if m.step == 0 then
                -- face player during charge (feels intentional)
                self:FacePlayer()

                m.chargeT = (m.chargeT or 0) + dtSec
                local chargeDur = m.chargeDur or 0.45

                if not m.chargeStarted then
                    m.chargeStarted = true

                    -- The wind up warning. This attack crosses the room, so
                    -- the player needs to know it is coming while there is
                    -- still time to be somewhere else.
                    if _G.event_bus and _G.event_bus.publish then
                        local cx, cy, cz = self:GetPosition()
                        -- Placed at ground height, the same way the dive
                        -- warning is.
                        local gy = (Nav and Nav.GetGroundY and Nav.GetGroundY(self.entityId)) or cy
                        _G.event_bus.publish("miniboss_charge_warning", {
                            targetId = self.entityId,
                            posX = cx, posY = gy, posZ = cz,
                            seconds = chargeDur,
                            fast = m.fast == true,
                        })
                        _G.event_bus.publish("camera_shake", {
                            intensity = self.ChargeShakeIntensity or 0.55,
                            duration  = chargeDur,
                            frequency = self.ChargeShakeFrequency or 18.0,
                        })
                    end
                end

                -- The swing is the same clip as the ordinary melee attack,
                -- whose claw connects BossMeleeWindup seconds in. It starts
                -- that long before the dash's contact time so the claw lands
                -- with the damage.
                if not m.swingTriggered
                   and m.chargeT >= chargeDur + contactT - (self.BossMeleeWindup or 1.1) then
                    m.swingTriggered = true
                    if self._animator then self._animator:SetTrigger("Melee") end
                end

                if m.chargeT >= chargeDur then
                    -- lock dash direction at the END of charge (fair + readable)
                    local dx, dz = self:_DirToPlayerXZ()
                    if not dx then
                        self:_CancelCurrentAttackMove("NO_TARGET")
                        return
                    end

                    -- face dash direction
                    local q = { yawQuatFromDir(dx, dz) }
                    if #q > 0 then self:ApplyRotation(q[1], q[2], q[3], q[4]) end

                    m.dx, m.dz = dx, dz
                    m.dashT = 0

                    -- The dash stays inside the arena
                    local ex, ez = self:GetEnemyPosXZ()
                    m.maxTravel = self:_ArenaTravelLimit(ex, ez, dx, dz)
                    m.travelled = 0

                    m.step = 1
                    m.chargeT = 0
                end

                return
            end

            -- Step 1: dash along the locked direction until level with the
            -- player, then the claw lands at the contact time. A player who
            -- steps off the line during the dash is missed.
            if m.step == 1 then
                m.dashT = (m.dashT or 0) + dtSec

                local speed = 0
                if not m.reached then
                    local px, _, pz = self:GetPlayerPosForAI()
                    local ex, ez = self:GetEnemyPosXZ()
                    if px and ex then
                        -- How far ahead of the boss the player is along the dash
                        local along = (px - ex) * m.dx + (pz - ez) * m.dz
                        local room = math.min(along - (m.stopDist or 0.8),
                                              (m.maxTravel or 0) - m.travelled)
                        if room <= 0 or m.dashT > contactT then
                            m.reached = true
                        else
                            -- Units per second, and never past the stop point
                            -- within this frame
                            speed = math.min(m.dashSpeed or 40.0, room / math.max(dtSec, 1e-4))
                        end
                    else
                        m.reached = true
                    end
                end

                if speed > 0 then
                    m.travelled = m.travelled + speed * dtSec
                    if self._controller then
                        CharacterController.Move(self._controller, m.dx * speed, 0, m.dz * speed)
                    else
                        local x, y, z = self:GetPosition()
                        if x then
                            self:SetPosition(x + m.dx * speed * dtSec, y, z + m.dz * speed * dtSec)
                        end
                    end
                else
                    -- Stopped: face the player while the claw comes down
                    self:FacePlayer()
                end

                if not m.slashed and m.dashT >= contactT then
                    m.slashed = true
                    self:_publishSFX("meleeAttack")

                    if _G.event_bus and _G.event_bus.publish then
                        local ex, ey, ez = self:GetPosition()
                        local qW, qX, qY, qZ = self:GetRotation()
                        _G.event_bus.publish("miniboss_vfx", {
                            pos = { x = ex, y = ey, z = ez },
                            rot = { w = qW, x = qX, y = qY, z = qZ },
                            entityId = self.entityId,
                        })
                        _G.event_bus.publish("miniboss_slash", {
                            entityId = self.entityId,
                            x = ex, y = ey, z = ez,
                            radius = m.slashRadius or 1.4,
                            dmg = m.dmg or 4,

                            kbStrength = m.kbStrength or 8.0,
                            kbUp = 0.0,
                        })
                    end
                end

                if m.dashT >= dashDur then
                    m.step = 2
                    m.recoverT = 0
                    m.dashT = 0
                end

                return
            end

            -- Step 2: recovery
            if m.step == 2 then
                m.recoverT = (m.recoverT or 0) + dtSec
                if m.recoverT >= (m.postDelay or 0.55) then
                    self:_EndMove()
                    m.recoverT = 0
                end
                return
            end
        end

        -------------------------------------------------
        -- Feather flurry: feathers at the player in quick succession
        -------------------------------------------------
        -- One throw animation after another until every feather is out. A
        -- feather leaves only while a throw plays, so none leaves between
        -- throws or while the boss is doing anything else.
        if m.kind == "FeatherFlurry" then
            if m.step == 0 then
                self:FacePlayer()
                self:_TriggerAttackAnim(m, "Ranged")
                m.throws = (m.throws or 0) + 1
                m.nextShotAt = m.releaseAt or 0.2
                m.step = 1
            end
            if m.step == 1 then
                local anim, throwTime = self:_FollowAttackAnim(m, THROW_STATE, dtSec)
                if anim == "waiting" then return end
                if anim == "timeout" then
                    self:_CancelCurrentAttackMove("THROW_NOT_STARTED")
                    return
                end
                if anim == "left" then
                    -- This throw is over. Another, unless all are out or the
                    -- throws have run out.
                    if (m.shotsDone or 0) < (m.shots or 12) and m.throws < (m.maxThrows or 6) then
                        m.step = 0
                    else
                        m.step = 2
                        m.doneAt = m.t + (m.postDelay or 0.4)
                    end
                    return
                end
                self:FacePlayer()
                if (m.shotsDone or 0) < (m.shots or 12) and throwTime >= m.nextShotAt then
                    self:SpawnKnifeSingleAtPlayer()
                    m.shotsDone = (m.shotsDone or 0) + 1
                    m.nextShotAt = throwTime + (m.interval or 0.1)
                    if m.shotsDone == 1 or m.shotsDone % 4 == 0 then
                        self:_publishSFX("rangedAttack")
                    end
                end
                return
            end
            if m.step == 2 and m.t >= m.doneAt then
                self:_EndMove()
            end
            return
        end

        -------------------------------------------------
        -- Move5: Death Lotus (spin + forward sprays)
        -------------------------------------------------
        if m.kind == "DeathLotus" then
            if m.step == 0 then
                m.spinYaw = m.spinYaw or 0
                m.fireAcc = 0
                m.step = 1
            end

            local spinSpeed = m.spinSpeed or (math.pi * 1.8) -- rad/s
            local dur = m.duration or 2.8
            local fireInterval = m.fireInterval or 0.10

            -- advance yaw
            m.spinYaw = (m.spinYaw or 0) + spinSpeed * dtSec

            -- apply rotation visually (yaw-only)
            local half = (m.spinYaw or 0) * 0.5
            self:ApplyRotation(math.cos(half), 0, math.sin(half), 0)

            -- forward vector from yaw
            local fx = math.sin(m.spinYaw or 0)
            local fz = math.cos(m.spinYaw or 0)

            -- shoot forward, not aimed
            m.fireAcc = (m.fireAcc or 0) + dtSec
            while m.fireAcc >= fireInterval do
                m.fireAcc = m.fireAcc - fireInterval
                --print("[MinibossAI] SPAWNING DEATHLOTUS")
                self:SpawnForwardSingle(fx, fz, m.range or 12.0, m.lotusYOffset or 0.0)
            end
            
            if self._animator:GetCurrentState() == "Recovery" then
                self._animator:SetTrigger("Ranged")
            end

            if m.t >= dur then
                self:_EndMove()
            end
            return
        end
    end,

    EnqueueMove = function(self, kind, data)
        self._moveQueue = self._moveQueue or {}
        self._moveQueue[#self._moveQueue + 1] = { kind = kind, data = data or {} }
    end,

    TryStartQueuedMove = function(self)
        if not self:IsCurrentMoveFinished() then return false end

        if self:IsActionLocked() then
            return false
        end

        if not self._moveQueue or #self._moveQueue == 0 then return false end

        local item = table.remove(self._moveQueue, 1)
        self:_BeginMove(item.kind, item.data)
        return true
    end,

    _UpdatePhase1 = function(self, dtSec)
        -- =========================================================
        -- 1. ALWAYS TRACK THE PLAYER (Even during other attacks)
        -- =========================================================
        local px,py,pz = self:GetPlayerPosForAI()
        local inMeleeRange = false
        
        if px then
            local ex,ez = self:GetEnemyPosXZ()
            local dx,dz = px-ex, pz-ez
            local d2 = dx*dx + dz*dz
            local meleeR = self.BossMeleeRange or 2.2
            
            if d2 <= meleeR*meleeR then
                inMeleeRange = true
            end
        end

        if inMeleeRange then
            -- Only throws in a row at a player who keeps away count
            self._p1ThrowsAway = 0
        end

        -- =========================================================
        -- 2. GUARDS: Stop here if the boss is mid-attack or locked
        -- =========================================================
        if self:TryStartQueuedMove() then return end
        if not self:IsCurrentMoveFinished() then return end
        if self:IsActionLocked() then return end
        if not px then return end

        -- =========================================================
        -- 3. CHOOSE NEXT MOVE (Boss is idle and ready)
        -- =========================================================
        
        -- PRIORITY 1: the player stays in melee range. After a few melee
        -- attacks the boss answers with the charged slash, so trading blows up
        -- close is not safe for ever either. The slash restarts the melee
        -- cooldown, since it leaves the boss beside the player.
        if inMeleeRange
           and (self._p1MeleesClose or 0) >= (self._p1MeleeTarget or 3) then
            self._p1MeleesClose = 0
            self._p1MeleeTarget = self:_RollP1MeleeTarget()
            self._meleeCdT = self.BossMeleeCooldown or 2.5

            self:FateSealed(self.P1_ChargedSlashCharge or 1.10)
            return
        end

        -- PRIORITY 1b: the player keeps away. After one or two throws the boss
        -- charges and crosses the room, so walking away is not a safe answer.
        -- The throws set the pace here, and the slash also restarts the
        -- melee cooldown and count, since it leaves the boss beside the player.
        if (not inMeleeRange)
           and (self._p1ThrowsAway or 0) >= (self._p1ThrowTarget or 1) then
            self._p1ThrowsAway = 0
            self._p1ThrowTarget = self:_RollP1ThrowTarget()
            self._p1MeleesClose = 0
            self._meleeCdT = self.BossMeleeCooldown or 2.5

            self:FateSealed(self.P1_ChargedSlashCharge or 1.10)
            return
        end

        -- PRIORITY 2: Normal Melee
        if inMeleeRange then
            if (self._meleeCdT or 0) <= 0 then
                self._meleeCdT = self.BossMeleeCooldown or 2.5
                self:_DoMeleeAttack(true)
            end
            return
        end

        -- PRIORITY 3: Normal Ranged
        self:_BeginMove("P1RangedCharged", {
            charge = self.P1_RangedCharge or 0.75,
            spread = 0.6,
            postDelay = 0.35,
            countForLunge = true,
        })
    end,

    -- How many throws at a player who keeps away come before the next charged
    -- slash. Scene values are stored as floats, so they are floored.
    _RollP1ThrowTarget = function(self)
        local lo = math.max(1, math.floor(self.P1_ThrowsBeforeLungeMin or 1))
        local hi = math.max(lo, math.floor(self.P1_ThrowsBeforeLungeMax or 2))
        return math.random(lo, hi)
    end,

    -- How many melee attacks at a player who stays in melee range come before
    -- the next charged slash
    _RollP1MeleeTarget = function(self)
        local lo = math.max(1, math.floor(self.P1_MeleesBeforeLungeMin or 3))
        local hi = math.max(lo, math.floor(self.P1_MeleesBeforeLungeMax or 3))
        return math.random(lo, hi)
    end,

    EnterPhase2_Air = function(self)
        --print("[Miniboss] EnterPhase2_Air")
        self:_SetInAir(true)

        local x,y,z = self:GetPosition()
        local gy = (Nav and Nav.GetGroundY and Nav.GetGroundY(self.entityId)) or y or 0
        local newY = gy + (self.AirHeight or 4.0)

        self:SetPosition(x, newY, z)

        self._phase2BurstRoundsDone = 0
        self._phase2BurstGapT = 0
        self:_SetChainImmune(false)
        self._phase2Numpad = self:_PickRandomAirNumpad(nil)
        self._phase2State = "MOVE"
        self._phase2AfterAttackT = 0
        self._phase2BurstStarted = false
        -- Only the hook path sets this, so anything else that puts the boss on
        -- the ground would otherwise inherit whatever the last hook left.
        self._phase2GroundAttacksLeft = 0
        self._phase2GroundT = 0
    end,

    _UpdatePhase2 = function(self, dtSec)
        -- If slamming down, keep falling until landed
        if self._slamActive then
            local landed = self:UpdateSlamDown(dtSec, "hook_slam")
            if not landed then
                return -- still slamming
            end

            -- landed this frame: switch to ground mode (creates CC)
            self:_SetInAir(false)

            -- On the ground it gets up, takes a couple of attacks, answers with
            -- the charged slash, and lifts off once P2_MinGroundTime has passed.
            -- Being hooked down is the player's reward for landing the hook,
            -- and this is the window it opens.
            self._phase2GroundAttacksLeft = self.P2_GroundAttacksAfterSlam or 2
            self._phase2LungeDone = false
            self._phase2GroundT = 0
            self._phase2State = "GROUND"
            return
        end

        -- Ground handling: get up, attack, the charged slash, then back to air
        if not self._inAir then
            self._phase2State = "GROUND"
            self._phase2GroundT = (self._phase2GroundT or 0) + dtSec

            -- Nothing starts until it is back on its feet. The fall, the impact
            -- and getting up all play with the boss on the floor, and anything
            -- started then came out of a body lying on the ground.
            if self:_IsGettingUp() then return end
            if not self:IsCurrentMoveFinished() or self:IsActionLocked() then return end

            if (self._phase2GroundAttacksLeft or 0) > 0 then
                self._phase2GroundAttacksLeft = self._phase2GroundAttacksLeft - 1

                -- Melee if the player stayed to trade, a throw if they backed
                -- off. Either way the boss spends the time on the ground.
                local px, _, pz = self:GetPlayerPosForAI()
                local inRange = false
                if px then
                    local ex, ez = self:GetEnemyPosXZ()
                    local dx, dz = px - ex, pz - ez
                    local r = self.BossMeleeRange or 2.2
                    inRange = (dx*dx + dz*dz) <= (r*r)
                end

                if inRange then
                    self:_DoMeleeAttack()
                else
                    self:_BeginMove("P1RangedCharged", {
                        charge = self.P1_RangedCharge or 0.75,
                        spread = 0.6,
                        postDelay = 0.35
                    })
                end
                return
            end

            if not self._phase2LungeDone then
                self._phase2LungeDone = true
                self:FateSealed(self.P1_ChargedSlashCharge or 1.10)
                return
            end

            -- Attacks done and the minimum time served: lift back into the air
            -- and pick a new point
            if (self._phase2GroundT or 0) >= (self.P2_MinGroundTime or 8.0) then
                self:_SetInAir(true)
                self._phase2Numpad = self:_PickRandomAirNumpad(self._phase2Numpad)
                self._phase2State = "MOVE"
            end

            return
        end

        -- Ensure state initialized
        if not self._phase2State then
            self._phase2State = "MOVE"
        end

        -- MOVE: go to waypoint
        if self._phase2State == "MOVE" then
            local tx, ty, tz = self:_GetAirWaypoint(self._phase2Numpad or 5)
            local x,y,z = self:GetPosition()
            if self._controller and CharacterController.GetPosition then
                local p = CharacterController.GetPosition(self._controller)
                if p then x,y,z = p.x,p.y,p.z end
            end
            local arrived = self:_MoveToXZ_Air(tx, tz, dtSec)
            if not arrived then return end

            -- Arrived -> switch to ATTACK
            self._phase2State = "ATTACK"
            self._phase2AfterAttackT = 0
            self._phase2BurstStarted = false
            self._phase2BurstRoundsDone = 0
            self._phase2BurstGapT = 0
            return
        end

        -- ATTACK: start BurstFire, wait for it to finish, then wait a short delay, then relocate
        if self._phase2State == "ATTACK" then
            -- during phase recover, do NOT start BurstFire yet
            if self._phaseRecoverActive then
                self:FacePlayer()
                return
            end

            if self:IsActionLocked() then return end

            -- If any queued reaction exists, wait (keeps it fair)
            if (self._moveQueue and #self._moveQueue > 0) then
                self:FacePlayer()
                return
            end

            local roundsTarget = tonumber(self.P2_BurstRounds) or 3
            local gap = tonumber(self.P2_BurstGap) or 0

            -- If BurstFire is currently running, just wait
            if self:IsInMove("BurstFire") then
                return
            end

            -- If we finished a burst, apply a small gap before starting next one
            if (self._phase2BurstGapT or 0) > 0 then
                self._phase2BurstGapT = self._phase2BurstGapT - dtSec
                self:FacePlayer()
                return
            end

            -- Start next burst if we still have rounds left
            if (self._phase2BurstRoundsDone or 0) < roundsTarget then
                if self:IsCurrentMoveFinished() then
                    self._phase2BurstRoundsDone = (self._phase2BurstRoundsDone or 0) + 1
                    self:BurstFire()
                    self._phase2BurstGapT = gap
                end
                return
            end

            -- All rounds done -> wait a bit before moving again
            self._phase2AfterAttackT = (self._phase2AfterAttackT or 0) + dtSec
            if self._phase2AfterAttackT >= (self.AirWaitAfterAttack or 0.45) then
                self._phase2AfterAttackT = 0

                local old = self._phase2Numpad
                self._phase2Numpad = self:_PickRandomAirNumpad(old)
                self._phase2State = "MOVE"
            end
            return
        end

        -- Fallback: reset state if corrupted
        self._phase2State = "MOVE"
    end,

    EnterPhase3_Air = function(self)
        self:_SetInAir(true)
        self:_SetChainImmune(true)
        self._phase3Step = 0
        self._phase3RainCount = 0
        self._phase3DiveStarted = false
    end,

    _PickRainCells5 = function(self)
        local cells = {1,2,3,4,5,6,7,8,9}
        -- shuffle
        for i=#cells,2,-1 do
            local j = math.random(1,i)
            cells[i], cells[j] = cells[j], cells[i]
        end
        local pick = {}
        for i=1,5 do pick[i] = cells[i] end
        return pick
    end,

    _DoRainExplosives = function(self)
        -- -- Phase 3 "feathers": shoot knives to 5 random cells
        -- local cells = self:_PickRainCells5()
        -- local yOff = self.P3_FeatherTargetYOffset or 0.25

        -- for i=1,#cells do
        --     local n = cells[i]
        --     local gx, gz = self:_GetGridXZ(n)
        --     local gy = (Nav and Nav.GetGroundY and Nav.GetGroundY(self.entityId)) or select(2, self:GetPosition()) or 0
        --     self:SpawnKnifeSingleAtWorld(gx, (gy or 0) + yOff, gz, "P3F"..tostring(n))
        -- end

        -- -- Queue the explosion check to happen when bombs "land"
        -- local delay = self.P3_FeatherActivateDelay or 0.90

        -- self._pendingRainExplosions[#self._pendingRainExplosions + 1] = {
        --     t = delay,
        --     payload = {
        --         entityId = self.entityId,
        --         cells = cells,
        --         dmg = 2,

        --         -- grid config so PlayerHealth can compute what cell they’re in
        --         step = self.GridStep or 4.0,
        --         cx = self.GridCenterX or 0.0,
        --         cz = self.GridCenterZ or 0.0,
        --     }
        -- }

        local cells = self:_PickRainCells5()
        local yOff = self.P3_FeatherTargetYOffset or 0.25
        local sx, sy, sz = self:_GetSpawnPos()

        for i=1,#cells do
            local cellNum = cells[i]
            local gx, gz = self:_GetGridXZ(cellNum)
            local gy = (Nav and Nav.GetGroundY and Nav.GetGroundY(self.entityId)) or select(2, self:GetPosition()) or 0
            
            -- 1. Spawn the new smart projectile
            local bombId = Prefab.InstantiatePrefab(self.FeatherBombProjectilePrefab)
            
            -- 2. Leave a note on the Global Blackboard for when the bomb wakes up
            _G.PendingFeatherBombs = _G.PendingFeatherBombs or {}
            _G.PendingFeatherBombs[bombId] = {
                sx = sx, sy = sy, sz = sz,
                tx = gx, ty = gy + yOff, tz = gz,
                targetCell = cellNum
            }
        end
    end,

    _DoDiveToPlayerGrid = function(self, dtSec)
        dtSec = toDtSec(dtSec)
        if dtSec <= 0 then return false end

        -- 1. POST-LANDING DELAY (On Ground)
        -- If we have already landed, just count down the delay. Do NOT execute air logic!
        if self._diveSlamLanded then
            self._p3_dive_postdelay = self._p3_dive_postdelay - dtSec
            if self._p3_dive_postdelay > 0.0 then
                return false
            else 
                self._phase3Dive = nil
                self._p3_dive_postdelay = self.P3_DivePostDelay
                self._diveSlamLanded = false
                return true -- Done! Transition to DeathLotus
            end
        end

        -- 2. SLAMMING DOWN (Falling)
        if self._slamActive then
            local landed = self:UpdateSlamDown(dtSec, "dive_attack")
            if not landed then
                return false
            end

            -- Landed: switch to ground mode and snap onto exact target position
            self:_SetInAir(false)

            local gy = (Nav and Nav.GetGroundY and Nav.GetGroundY(self.entityId)) or select(2, self:GetPosition()) or 0
            self:SetPosition(self._phase3Dive.gx, gy, self._phase3Dive.gz)
            if self._controller and CharacterController.SetPosition then
                pcall(function() CharacterController.SetPosition(self._controller, self._phase3Dive.gx, gy, self._phase3Dive.gz) end)
            end

            if _G.event_bus and _G.event_bus.publish then
                _G.event_bus.publish("boss_dive_impact", {
                    entityId = self.entityId,
                    x = self._phase3Dive.gx, y = gy, z = self._phase3Dive.gz,
                    dmg = 2,
                    radius = 1.4,
                })
            end

            self._diveSlamLanded = true
            return false
        end

        -- 3. APPROACHING TARGET (In Air)
        -- Ensure we're in air ONLY while we are actively flying towards the player
        self:_SetInAir(true)

        local px, py, pz = self:GetPlayerPosForAI()
        if not px then
            return false
        end

        local x, y, z = self:GetPosition()
        if x == nil then return false end

        local stopOffset = 0.6

        -- Direction from boss -> player
        local dxp = px - x
        local dzp = pz - z
        local dist2 = dxp*dxp + dzp*dzp
        local dist = math.sqrt(dist2)

        local tx, tz = px, pz

        -- Stop a bit before the player instead of directly on them
        if dist > 1e-6 then
            local nx = dxp / dist
            local nz = dzp / dist
            tx = px - nx * stopOffset
            tz = pz - nz * stopOffset
        end

        -- Phase 3 dive state init
        if not self._phase3Dive then
            self._phase3Dive = {
                gx = tx,
                gz = tz,
            }
        end

        local d = self._phase3Dive

        -- Continuously update target until slam commit
        d.gx = tx
        d.gz = tz

        -- Approach offset target instead of exact player position
        local dx, dz = d.gx - x, d.gz - z
        local r = self.P3_DiveCommitRadius or 0.20

        -- reached dive position
        if (dx*dx + dz*dz) <= (r*r) then

            -- start predelay timer if first arrival
            if not self._p3_dive_predelay then
                self._p3_dive_predelay = self.P3_DivePreDelay or 0.5
            end

            -- Mark the ground it is about to come down on, and move the mark
            -- when the landing spot moves. The boss keeps tracking the player
            -- until it drops, so a mark placed once on first arrival can end up
            -- somewhere the slam never lands.
            local moved = (d.markX == nil)
                or ((d.gx - d.markX)^2 + (d.gz - d.markZ)^2 > 0.25)
            if moved and _G.event_bus and _G.event_bus.publish then
                d.markX, d.markZ = d.gx, d.gz
                local gy = (Nav and Nav.GetGroundY and Nav.GetGroundY(self.entityId)) or 0
                _G.event_bus.publish("miniboss_slam_warning", {
                    targetId = self.entityId,
                    posX = d.gx, posY = gy, posZ = d.gz,
                    seconds = math.max(0.2, self._p3_dive_predelay),
                })
            end

            -- wait in air above player
            if self._p3_dive_predelay > 0 then
                self._p3_dive_predelay = self._p3_dive_predelay - dtSec
                return false
            end

            -- commit dive
            self._p3_dive_predelay = nil
            self:BeginSlamDown("DiveSmash")
            return false
        end

        self:_MoveToXZ_Air(d.gx, d.gz, dtSec)
        return false
    end,

    _UpdatePhase3 = function(self, dtSec)
        -- step 0: go to the middle of the arena in the air. From here until it
        -- lands from the dive it cannot be hooked, and glows to say so.
        if self._phase3Step == 0 then
            self:_SetInAir(true)
            self:_SetChainImmune(true)
            self._phase3Dive = nil
            self._slamActive = false
            self._phase3DiveStarted = false
            self._p3_dive_predelay = nil

            local tx,ty,tz = self:_GetAirWaypoint(5)
            local arrived = self:_MoveToXZ_Air(tx,tz,dtSec)
            if arrived then
                self._phase3RainCount = 0
                self._phase3Step = 1
                self._phase3RainT = nil
                self._phase3FeatherCastT = nil
            end
            return
        end

        -- step 1: shoot feathers to 5 random grids twice (with cast + cooldown)
        if self._phase3Step == 1 then
            -- Start casting if not already
            if not self._phase3FeatherCastT and not self._phase3RainT then
                self._phase3FeatherCastT = self.P3_FeatherCastTime or 0.05

                if self._animator then self._animator:SetTrigger("FeatherBomb") end
                self:_publishSFX("rangedAttack")

                return
            end

            -- During cast: wait before firing all 5 at once
            if self._phase3FeatherCastT then
                self:FacePlayer()
                self._phase3FeatherCastT = self._phase3FeatherCastT - dtSec
                if self._phase3FeatherCastT > 0 then
                    return
                end

                -- Cast finished -> fire all 5 now
                self._phase3FeatherCastT = nil
                self:_DoRainExplosives()

                -- cooldown after firing
                self._phase3RainT = self.P3_FeatherCooldown or (self.P3_FeatherRoundGap or 0.90)
                self._phase3RainCount = (self._phase3RainCount or 0) + 1
                return
            end

            -- Cooldown timer between rounds
            if self._phase3RainT then
                self._phase3RainT = self._phase3RainT - dtSec
                if self._phase3RainT > 0 then return end
                self._phase3RainT = nil

                if (self._phase3RainCount or 0) >= (self.P3_FeatherRounds or 2) then
                    self._phase3Step = 2
                end
                return
            end

            return
        end

        -- step 2: dive onto player's grid (approach then slam)
        if self._phase3Step == 2 then
            local done = self:_DoDiveToPlayerGrid(dtSec)
            if done then
                self:_SetChainImmune(false)
                self._phase3Step = 3
                self._phase3GroundT = 0
                self._phase3GroundNext = 1
            end
            return
        end

        -- step 3: on the ground, where the player can hit it. The spinning
        -- throw, the charged slash, the feather flurry and the charged slash
        -- again, in turn, then back up once P3_MinGroundTime has passed. None
        -- of them can be cut short by the player, so one hit no longer sends
        -- it back into the air.
        if self._phase3Step == 3 then
            self._phase3GroundT = (self._phase3GroundT or 0) + dtSec
            if not self:IsCurrentMoveFinished() or self:IsActionLocked() then return end

            local nextMove = P3_GROUND_PLAN[self._phase3GroundNext or 1]
            if nextMove then
                self._phase3GroundNext = (self._phase3GroundNext or 1) + 1
                if nextMove == "DeathLotus" then
                    self:DeathLotus()
                elseif nextMove == "FeatherFlurry" then
                    self:FeatherFlurry()
                elseif nextMove == "Lunge" then
                    self:FateSealed(self.P3_LungeCharge or 0.50, self.P3_LungeSpeed or 60.0, true)
                end
                return
            end

            if self._phase3GroundT >= (self.P3_MinGroundTime or 9.0) then
                self._phase3Step = 0
            end
            return
        end
    end,

    ResetBossToIdle = function(self)
        --print("[MinibossAI] ResetBossToIdle")

        self._move = nil
        self._moveFinished = true
        self.currentMove = nil
        self.currentMoveDef = nil
        self._moveQueue = {}

        self:UnlockActions()
        self._transforming = false
        self._pendingPhase = nil
        self._immuneDamage = false
        self:_SetChainImmune(false)

        self._phase2BurstRoundsDone = 0
        self._phase2BurstGapT = 0
        self._phase2Numpad = nil
        self._phase3Dive = nil
        self._phase3DiveStarted = false
        self._slamActive = false
        self._diveSlamLanded = false
        self._p3_dive_predelay = nil

        self._moveCooldowns = {}
        self._meleeCdT = 0

        -- no cutscene replay, but boss must re-aggro later
        -- Keep _introDone as-is: if intro never played, stay false so HP bar gate (line 521) stays blocked
        -- self._introDone = true
        self._inIntro = false
        self._combatActive = false

        if self.StopCC then
            pcall(function() self:StopCC() end)
        end
        if self._rb then
            pcall(function() self._rb.linearVel = { x=0, y=0, z=0 } end)
            pcall(function() self._rb.impulseApplied = { x=0, y=0, z=0 } end)
        end

        if self._animator then
            pcall(function() self._animator:SetBool("PlayerInDetectionRange", false) end)
            pcall(function() self._animator:SetBool("PlayerInAttackRange", false) end)
            pcall(function() self._animator:SetBool("ReadyToAttack", false) end)
            self:_ClearTriggers({ "Melee", "Ranged", "Taunt", "Hooked" })
            self:_ClearTriggers(HURT_TRIGGERS)
        end

        self:_publishBossHealth()
        self:_setBossHealthBarVisible(false)
        self._bossHealthBarShown = false
    end,

    -------------------------------------------------
    -- Move implementations (entry points)
    -------------------------------------------------
    BasicAttack = function(self)
        self._animator:SetTrigger("Ranged")
        self:_BeginMove("Basic", {
            spread = 0.6,
            postDelay = 0.35
        })
    end,

    BurstFire = function(self)
        self._animator:SetTrigger("Ranged")
        self:_BeginMove("BurstFire", {
            bursts = 5,
            interval = self.P2_BurstInterval or 0.12,
            postDelay = 0.45
        })
    end,

    AntiDodge = function(self)
        self._animator:SetTrigger("Ranged")
        self:_BeginMove("AntiDodge", {
            spread1 = 0.25,
            spread2 = 0.35,
            spread3 = 0.65,
            spread4 = 1.15,
            postDelay = 0.45
        })
    end,

    -- fast: phase 3's quicker lunge, which the boss's glow tells apart from
    -- the slower one in phases 1 and 2
    FateSealed = function(self, chargeTime, dashSpeed, fast)
        self:_BeginMove("FateSealed", {
            chargeDur = chargeTime,
            dashDur = 0.4,
            dashSpeed = dashSpeed or self.LungeSpeed or 40.0,
            fast = fast == true,
            -- A dashing boss carries the player along if it reaches them: the
            -- player's controller takes the boss's speed from the contact,
            -- and was thrown about 2 units clear of the claw. The capsules
            -- meet between 0.8 and 1.3 units apart, so the dash stops at 1.3
            -- and the claw reaches 1.8, a stride further.
            stopDist = 1.3,
            slashAt = 0.90,
            slashRadius = 1.8,
            dmg = 4,
            kbStrength = 8.0,
            postDelay = 2.60
        })
    end,

    FeatherFlurry = function(self)
        self:_BeginMove("FeatherFlurry", {
            shots = math.floor(self.P3_FlurryShots or 12),
            interval = self.P3_FlurryInterval or 0.10,
            releaseAt = 0.2,
            maxThrows = 6,
            postDelay = 0.4,
        })
    end,

    DeathLotus = function(self)
        self:_BeginMove("DeathLotus", {
            duration = 4.5,
            spinSpeed = math.pi * 1.0,
            fireInterval = 0.20,
            range = 12.0,
            lotusYOffset = -5.0,
        })
        self._animator:SetTrigger("Ranged")
    end,
}