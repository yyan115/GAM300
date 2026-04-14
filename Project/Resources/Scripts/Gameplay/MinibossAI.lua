-- Resources/Scripts/Gameplay/MinibossAI.lua
require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local StateMachine = require("Gameplay.StateMachine")
local ChooseState  = require("Gameplay.MinibossChooseState")
local ExecuteState = require("Gameplay.MinibossExecuteState")
local RecoverState = require("Gameplay.MinibossRecoverState")
local BattlecryState = require("Gameplay.MinibossBattlecryState")

local KnifePool = require("Gameplay.KnifePool")

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
    if reason == "HOOKED"          then return 80  end
    if reason == "HIT_STUN"        then return 10  end
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
        HookedDuration = 4.0,

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
        -- Duration in melee range before firing the feather bomb punisher move in phase 1
        P1_MeleePunishTime = 5.0,

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
        P2_BurstGap = 1.25, -- small pause between bursts

        -- Phase 3 tuning
        P3_FeatherCellsPerRound = 5,
        P3_FeatherRounds = 2,
        P3_FeatherRoundGap = 0.90,        -- time between rounds (telegraph/explode window)
        P3_FeatherTargetYOffset = 0.25,   -- aim slightly above ground

        P3_DiveCommitRadius = 0.20,       -- how close in XZ before slamming down
        P3_DivePreDelay = 1.00,           -- how long to wait before the dive smash
        P3_DivePostDelay = 0.50,          -- how long to wait after the dive smash
        P3_FateAfterHookDelay = 2.00,     -- wait after interrupt before casting Fate Sealed

        P3_FeatherCastTime = 0.25,         -- longer "windup" before firing all 5
        P3_FeatherCooldown = 2.00,         -- longer cooldown after firing (between rounds)
        FeatherBombProjectilePrefab = "Resources/Prefabs/Knife_FeatherBomb.prefab",
        P3_FeatherBombExplosionPrefabPath = "Resources/Prefabs/MinibossFeatherBombExplosion.prefab",

        -- Tile activation timing (keep activating tile, but slower)
        P3_FeatherActivateDelay = 0.90,    -- telegraph delay before tile becomes dangerous
        P3_FeatherActiveDuration = 1.25, -- how long the tile is dangerous after activation

        -- Feather travel speed (slower fall/flight)
        P3_FeatherSpeedScale = 0.2,       -- multiplier on knife speed for P3F tags (0.2~0.6 feels good)

        HurtReactDuration = 0.35,   -- how long we "reserve" time for hurt anim to play

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

        -- hooked tracking
        self._hooked = false

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
                self:ApplyHit(damage, "COMBO")
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
                self:ApplyHook(payload.duration or self.HookedDuration or 4.0)
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
            pcall(function() self:ApplyHook(payload.duration or self.HookedDuration) end)
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

        if Keyboard.IsDigitPressed(2) then
            self:ApplyHook(self.HookedDuration)
        end

        if Keyboard.IsDigitPressed(4) then
            self:ApplyHit(10)
        end

        if Keyboard.IsDigitPressed(6) then
            self:ForceNextPhase()
        end

        -- Show boss HP bar only after the intro/cinematic is fully over
        if self._introDone and (not self._inIntro) and (not self.dead) and (not self._bossHealthBarShown) then
            self._bossHealthBarShown = true
            self:_publishBossHealth()
            self:_setBossHealthBarVisible(true)
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

        if self._frozenBycinematic then
            --print("[Miniboss] FROZEN by cinematic, skipping Update. lockReason=", tostring(self._lockReason), "lockT=", tostring(self._lockTimer))
            return
        end

        -- Freeze movement during cinematic
        if self._frozenBycinematic then return end

        -- 2) Tick timers always
        self._hitLockTimer = math.max(0, (self._hitLockTimer or 0) - dtSec)
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
            if px then
                local ex, ez = self:GetEnemyPosXZ()
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

        local yaw = math.deg(atan2(dx, dz))
        local q = eulerToQuat(0, yaw, 0)
        self:SetRotation(q.w, q.x, q.y, q.z)
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

        self._hitLockTimer = self.HitIFrame or 0.2
        self.health = math.max(0, (self.health or 0) - (dmg or 1))

        self:_publishBossHealth()

        local myRandomValue = math.random(1, 3)
        if myRandomValue == 1 then
            self._animator:SetTrigger("Hurt1")
        elseif myRandomValue == 2 then
            self._animator:SetTrigger("Hurt2")
        else
            self._animator:SetTrigger("Hurt3")
        end

        --print(string.format("[Miniboss][Hit] dmg=%s hp=%.1f/%.1f", tostring(dmg or 1), self.health, self.MaxHealth))

        if self.health <= 0 then
            self:Die()
            return
        end

        -- [NEW] Check for Super Armor!
        local isUninterruptible = self:IsInMove("P1FeatherBombPunish")

        -- Only play hurt animations and interrupt moves if NOT in super armor
        if not isUninterruptible then
            local myRandomValue = math.random(1, 3)
            if myRandomValue == 1 then
                self._animator:SetTrigger("Hurt1")
            elseif myRandomValue == 2 then
                self._animator:SetTrigger("Hurt2")
            else
                self._animator:SetTrigger("Hurt3")
            end

            -- Interrupt current attack/cast so delayed hitboxes/projectiles do not fire
            self:_CancelCurrentAttackMove("HURT")

            -- Always queue a short “hurt reaction window”
            self:EnqueueMoveFront("HurtReact", { duration = self.HurtReactDuration or 0.35 })
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

        -- Play hurt SFX
        self:_publishSFX("hurt")

        -- If we crossed a phase threshold, start NEW transition immediately
        local computed = self:_ComputePhase()
        if (computed ~= (self._phase or 1)) and (not self._transforming) and (not self.dead) then
            self:StartBossPhaseTransition(computed)
        end

        -- If transforming now, don't apply HIT_STUN (avoid overwriting lock intent)
        if self._transforming or (self._lockReason == "PHASE_TRANSFORM") then
            return
        end

        -- Optional: tiny hit-stun lock
        self:LockActions("HIT_STUN", 0.15)

        -- if self.ClipHurt and self.ClipHurt >= 0 and self.PlayClip then
        --     self:PlayClip(self.ClipHurt, false)
        -- end
    end,

    ApplyHook = function(self, duration)
        if self.dead then return end
        if self._inIntro then return end
        if self._immuneChain then return end

        -- phase 2: hooking forces boss down
        if self._phase == 2 and self._inAir then
            self._hooked = true
            self._hookedDownRequested = true

            -- Cancel current air attack immediately
            self:_EndMove()

            -- Start falling RIGHT NOW
            self:BeginSlamDown("Pulldown")

            --print("[Miniboss][Hooked] Phase 2 air hook -> immediate slam")
            return
        end

        if self._phase == 3 and not self._inAir then
            -- If hooked during Death Lotus, interrupt it immediately and schedule Fate Sealed
            if self:IsInMove("DeathLotus") then
                --print("[Miniboss][P3] Hooked DURING DeathLotus -> INTERRUPT")
                self._p3LotusInterrupted = true
                self._p3PendingFate = true
                self._p3FateDelayT = self.P3_FateAfterHookDelay or 2.0
                self:_EndMove() -- hard stop lotus right now
            else
                self._p3WasHooked = true
            end
        end

        self._hooked = true

        local dur = duration or self.HookedDuration or 4.0
        self:LockActions("HOOKED", dur)

        if self.ClipHooked and self.ClipHooked >= 0 and self.PlayClip then
            self:PlayClip(self.ClipHooked, true)
        end

        --print(string.format("[Miniboss][Hooked] START %.2fs", dur))
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

    _DoMeleeAttack = function(self)

        -- longer windup + further range
        --print("[Miniboss] _DoMeleeAttack: SetTrigger(Melee)")
        if self._animator then self._animator:SetTrigger("Melee") end
        self:_BeginMove("BossMelee", {
            windup = self.BossMeleeWindup or 0.4,
            range  = self.BossMeleeRange or 2.95,
            dmg    = 4,
            postDelay = 1.0
        })
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
        local cancelKinds = {
            BossMelee       = true,
            P1RangedCharged = true,
            ShoutAOE        = true,
            Basic           = true,
            BurstFire       = true,
            AntiDodge       = true,
            FateSealed      = true,
            DeathLotus      = true,
        }

        if cancelKinds[kind] then
            self._move.cancelled = true
            self._move.cancelReason = reason or "INTERRUPTED"
            self:_EndMove()
        end
    end,

    TickMove = function(self, dtSec)
        if self._moveFinished or not self._move then return end
        local m = self._move
        m.t = (m.t or 0) + dtSec

        -- =========================
        -- Queued reaction: Hurt
        -- =========================
        if m.kind == "HurtReact" then
            if m.step == 0 then
                m.step = 1
                m.endAt = (m.duration or 0.35)
            end
            if m.t >= (m.endAt or 0.35) then
                self:_EndMove()
            end
            return
        end

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
            if m.step == 0 then
                self:FacePlayer()
                m.step = 1
                m.hitAt = (m.windup or 0.85)
            end

            if not m.didHit and m.t >= (m.hitAt or 0) then
                m.didHit = true

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

            if m.t >= (m.hitAt + (m.postDelay or 0.4)) then
                self:_EndMove()
            end
            return
        end

        -------------------------------------------------
        -- Phase 1: Melee Punish (Single Feather Bomb)
        -------------------------------------------------
        if m.kind == "P1FeatherBombPunish" then
            if m.step == 0 then
                self:FacePlayer()
                if self._animator then self._animator:SetTrigger("FeatherBomb") end
                self:_publishSFX("rangedAttack")
                
                m.step = 1
                m.fireAt = (m.charge or 0.5)
            end

            if not m.didFire and m.t >= m.fireAt then
                m.didFire = true
                
                -- Find the specific grid cell the player is standing on
                local cellNum = self:_GetPlayerGridNumpad()
                local yOff = self.P3_FeatherTargetYOffset or 0.25
                local gx, gz = self:_GetGridXZ(cellNum)
                local gy = (Nav and Nav.GetGroundY and Nav.GetGroundY(self.entityId)) or select(2, self:GetPosition()) or 0
                local sx, sy, sz = self:_GetSpawnPos()
                
                -- 1. Spawn the new smart projectile
                local bombId = Prefab.InstantiatePrefab(self.FeatherBombProjectilePrefab)
                
                -- 2. Leave a note on the Global Blackboard
                _G.PendingFeatherBombs = _G.PendingFeatherBombs or {}
                _G.PendingFeatherBombs[bombId] = {
                    sx = sx, sy = sy, sz = sz,
                    tx = gx, ty = gy + yOff, tz = gz,
                    targetCell = cellNum
                }
                
                -- -- Shoot the knife to that exact cell
                -- self:SpawnKnifeSingleAtWorld(gx, gy + (self.P3_FeatherTargetYOffset or 0.25), gz, "P3F_Punish")

                -- -- Queue the explosion when the knife "lands"
                -- local delay = self.P3_FeatherActivateDelay or 0.90
                -- self._pendingRainExplosions[#self._pendingRainExplosions + 1] = {
                --     t = delay,
                --     payload = {
                --         entityId = self.entityId,
                --         cells = { cellNum }, -- Targets only the player's cell
                --         dmg = 2,
                --         step = self.GridStep or 4.0,
                --         cx = self.GridCenterX or 0.0,
                --         cz = self.GridCenterZ or 0.0,
                --     }
                -- }
                
                m.doneAt = m.t + (m.postDelay or 0.75)
            end

            if m.doneAt and m.t >= m.doneAt then
                self:_EndMove()
            end
            return
        end

        if m.kind == "P1RangedCharged" then
            if m.step == 0 then
                self:FacePlayer()
                if self._animator then self._animator:SetTrigger("Ranged") end
                m.step = 1
                m.fireAt = (m.charge or 0.75)
            end
            if not m.didFire and m.t >= (m.fireAt or 0.75) then
                m.didFire = true
                self:SpawnKnifeVolley3(m.spread or 0.6)
                m.doneAt = m.t + (m.postDelay or 0.35)
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

            -- Step 0: charge-up (telegraph)
            if m.step == 0 then
                -- face player during charge (feels intentional)
                self:FacePlayer()
                
                m.chargeT = (m.chargeT or 0) + dtSec
                local chargeDur = m.chargeDur or 0.45
                
                -- OPTIONAL: play charge animation / VFX / SFX once
                if not m.chargeStarted then
                    m.chargeStarted = true
                    --print("[Miniboss] FateSealed: SetTrigger(Melee)")
                    self._animator:SetTrigger("Melee")
                end

                if m.chargeT >= chargeDur then
                    -- lock dash direction at the END of charge (fair + readable)
                    local dx, dz = self:_DirToPlayerXZ()
                    if not dx then
                        self:_EndMove()
                        return
                    end

                    -- face dash direction
                    local q = { yawQuatFromDir(dx, dz) }
                    if #q > 0 then self:ApplyRotation(q[1], q[2], q[3], q[4]) end

                    m.dx, m.dz = dx, dz
                    m.dashT = 0

                    m.step = 1
                    m.chargeT = 0
                end

                return
            end

            -- Step 1: dash phase (stop early if we reach player, but keep timer for slash)
            local dashDur   = m.dashDur or 0.22
            local dashSpeed = m.dashSpeed or 18.0
            local stopDist  = m.stopDist or 1.8

            if m.step == 1 then
                m.dashT = (m.dashT or 0) + dtSec

                -- Check distance to player in XZ
                local px, _, pz = self:GetPlayerPosForAI()
                if px then
                    local ex, ez = self:GetEnemyPosXZ()
                    local ddx, ddz = px - ex, pz - ez
                    if (ddx*ddx + ddz*ddz) <= (stopDist*stopDist) then
                        m.reached = true
                    end
                end

                -- Only move if not reached yet
                if (not m.reached) then
                    if self._controller then
                        CharacterController.Move(
                            self._controller,
                            m.dx * dashSpeed * dtSec,
                            0,
                            m.dz * dashSpeed * dtSec
                        )
                    else
                        -- fallback (just in case CC is missing)
                        local x, y, z = self:GetPosition()
                        if x then
                            self:SetPosition(x + m.dx * dashSpeed * dtSec, y, z + m.dz * dashSpeed * dtSec)
                        end
                    end
                else
                    -- reached player: stop moving, just wait out slash timing
                    self:FacePlayer()
                end

                -- slash timing: keep as-is (it will now happen while standing still if reached early)
                local slashAt = m.slashAt or 0.85 -- fraction of dash
                if not m.slashed and m.dashT >= (dashDur * slashAt) then
                    m.slashed = true
                    self:_publishSFX("meleeAttack")

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

    EnqueueMoveFront = function(self, kind, data)
        self._moveQueue = self._moveQueue or {}
        table.insert(self._moveQueue, 1, { kind = kind, data = data or {} })
    end,

    TryStartQueuedMove = function(self)
        if not self:IsCurrentMoveFinished() then return false end

        -- Allow queued reactions during HIT_STUN (but still block for HOOKED/PHASE_TRANSFORM/DEAD/etc)
        if self:IsActionLocked() and (self._lockReason ~= "HIT_STUN") then
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
            self._p1MeleeTimer = (self._p1MeleeTimer or 0) + dtSec
        else
            self._p1MeleeTimer = 0 -- Reset instantly if they run away
        end

        -- =========================================================
        -- 2. GUARDS: Stop here if the boss is mid-attack or locked
        -- =========================================================
        if self:TryStartQueuedMove() then return end
        if not self:IsCurrentMoveFinished() then return end
        if self:IsActionLocked() then return end
        if not px then return end
        if self:IsInMove("HurtReact") or (self._hitLockTimer > 0) then return end

        -- =========================================================
        -- 3. CHOOSE NEXT MOVE (Boss is idle and ready)
        -- =========================================================
        
        -- PRIORITY 1: Anti-Camper Punish
        if self._p1MeleeTimer >= (self.P1_MeleePunishTime or 3.0) then
            self._p1MeleeTimer = 0 -- Reset so it doesn't chain-cast
            self._meleeCdT = self.BossMeleeCooldown or 2.5 -- Reset normal melee cooldown so it doesn't chain cast
            
            self:_BeginMove("P1FeatherBombPunish", {
                charge = 1.0,
                postDelay = 0.75
            })
            return
        end

        -- PRIORITY 2: Normal Melee
        if inMeleeRange then
            if (self._meleeCdT or 0) <= 0 then
                self._meleeCdT = self.BossMeleeCooldown or 2.5
                self:_DoMeleeAttack()
            end
            return
        end

        -- PRIORITY 3: Normal Ranged
        self:_BeginMove("P1RangedCharged", {
            charge = self.P1_RangedCharge or 0.75,
            spread = 0.6,
            postDelay = 0.35
        })
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
        self._immuneChain = false
        self._phase2Numpad = self:_PickRandomAirNumpad(nil)
        self._phase2State = "MOVE"
        self._phase2AfterAttackT = 0
        self._phase2BurstStarted = false
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

            -- brief hooked lock on landing
            self:LockActions("HOOKED", math.min(self.HookedDuration or 4.0, 0.9))

            -- queue FateSealed after hook lock ends (your existing design)
            self._phase2QueuedFate = true
            self._phase2State = "GROUND"
            return
        end

        -- Ground handling: run queued FateSealed, then go back to air
        if not self._inAir then
            self._phase2State = "GROUND"

            if self._phase2QueuedFate and self:IsCurrentMoveFinished() and (not self:IsActionLocked()) then
                self._phase2QueuedFate = false
                self:FateSealed(2.0)
                return
            end

            -- Once FateSealed ends (or nothing queued), lift back into air and pick a new point
            if self:IsCurrentMoveFinished() and (not self:IsActionLocked()) and (not self._phase2QueuedFate) then
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
        self._immuneChain = true
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

    _GetPlayerGridNumpad = function(self)
        local px,py,pz = self:GetPlayerPosForAI()
        if not px then return 5 end
        local cx = self.GridCenterX or 0
        local cz = self.GridCenterZ or 0
        local step = self.GridStep or 4.0

        local ix = math.floor((px - cx)/step + 0.5)
        local iz = math.floor((pz - cz)/step + 0.5)
        ix = math.max(-1, math.min(1, ix))
        iz = math.max(-1, math.min(1, iz))

        local map = {
            ["-1,-1"]=1, ["0,-1"]=2, ["1,-1"]=3,
            ["-1,0"]=4,  ["0,0"]=5,  ["1,0"]=6,
            ["-1,1"]=7,  ["0,1"]=8,  ["1,1"]=9,
        }
        return map[tostring(ix)..","..tostring(iz)] or 5
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
        -- step 0: go to center in air
        if self._phase3Step == 0 then
            self:_SetInAir(true)
            self._immuneChain = true
            self._p3WasHooked = false
            self._p3LotusInterrupted = false
            self._p3PendingFate = false
            self._p3FateDelayT = nil
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
            end
            return
        end

        -- step 1: shoot feathers to 5 random grids twice (with cast + cooldown)
        if self._phase3Step == 1 then
            -- Start casting if not already
            if not self._phase3FeatherCastT and not self._phase3RainT then
                self._phase3FeatherCastT = self.P3_FeatherCastTime or 0.05

                --print("[Miniboss] Phase 3 Step 1 SetTrigger(FeatherBomb)")
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
            self._immuneChain = true -- not hookable in air
            local done = self:_DoDiveToPlayerGrid(dtSec)
            if done then
                self._immuneChain = false -- hookable on ground during lotus
                self._phase3Step = 3
            end
            return
        end

        -- step 3: Death Lotus on ground
        if self._phase3Step == 3 then
            if self:IsCurrentMoveFinished() and (not self:IsActionLocked()) then
                self:DeathLotus()
                self._phase3Step = 4
            end
            return
        end

        -- step 4: while lotus is running OR after lotus ends / interrupted
        if self._phase3Step == 4 then
            -- If lotus is still running, just wait (hook interrupt is handled in ApplyHook)
            if self:IsInMove("DeathLotus") then
                return
            end

            -- If lotus was interrupted by hook: wait a bit, then Fate Sealed
            if self._p3PendingFate then
                self._p3FateDelayT = (self._p3FateDelayT or (self.P3_FateAfterHookDelay or 2.0)) - dtSec
                if self._p3FateDelayT <= 0 and self:IsCurrentMoveFinished() and (not self:IsActionLocked()) then
                    self._p3PendingFate = false
                    self._p3FateDelayT = nil
                    self:FateSealed(1.0)
                    return
                end
                return
            end

            -- After Fate Sealed ends (or no fate), repeat cycle
            if self:IsCurrentMoveFinished() and (not self:IsActionLocked()) then
                self._phase3Step = 0
                self._immuneChain = true
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
        self._immuneChain = false

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
            pcall(function() self._animator:ResetTrigger("Melee") end)
            pcall(function() self._animator:ResetTrigger("Ranged") end)
            pcall(function() self._animator:ResetTrigger("Taunt") end)
            pcall(function() self._animator:ResetTrigger("Hooked") end)
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
            interval = 0.18,   -- adjust for difficulty
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

    FateSealed = function(self, chargeTime)
        self:_BeginMove("FateSealed", {
            chargeDur = chargeTime,
            dashDur = 0.4,
            dashSpeed = 700.0,
            stopDist = 0.8,
            slashAt = 0.90,
            slashRadius = 1.4,
            dmg = 4,
            kbStrength = 8.0,
            postDelay = 2.60
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