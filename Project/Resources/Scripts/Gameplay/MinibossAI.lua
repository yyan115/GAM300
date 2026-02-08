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

-- Play random SFX from array
local function playRandomSFX(audio, clips)
    local count = clips and #clips or 0
    
    if count > 0 and audio then
        print("[MinibossAI] playRandomSFX count =", count)
        audio:PlayOneShot(clips[math.random(1, count)])
    end
end

-------------------------------------------------
-- Phase definition (HP-based)
-------------------------------------------------
local PHASE_THRESHOLDS = {
    { id = 4, hpPct = 0.10 },
    { id = 3, hpPct = 0.35 },
    { id = 2, hpPct = 0.70 },
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
    -- Move4 = { cooldown = 4.0, weights = { [1]=10,  [2]=10,  [3]=30, [4]=30 }, execute = function(ai) print("[Miniboss] Move4: Fate Sealed") ai:FateSealed() end },
    -- Move5 = { cooldown = 5.0, weights = { [1]=0,  [2]=0,  [3]=0,  [4]=30 }, execute = function(ai) print("[Miniboss] Move5: Death Lotus") ai:DeathLotus() end },
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

        -- Damage / Hook / Death
        HitIFrame      = 0.2,
        HookedDuration = 4.0,

        -- “Transformation” (phase transition) lock
        PhaseTransformDuration = 3.2,

        PlayerName = "Player",

        -- Gravity tuning
        Gravity      = -9.81,
        MaxFallSpeed = -25.0,

        -- Optional: small “stick to ground” behaviour
        GroundStickVel = -0.2,

        IntroDuration = 5.0,
        AggroRange    = 15.0,  -- distance to trigger intro

        -- SFX clip arrays (populate in editor with audio GUIDs)
        enemyHurtSFX = {},          -- EnemyHurt
        enemyDeathSFX = {},         -- Dying
        enemyMeleeAttackSFX = {},   -- EnemyScratchWoosh
        enemyMeleeHitSFX = {},      -- EnemyScratchHit
        enemyRangedAttackSFX = {},  -- EnemyThrowWoosh
        enemyRangedHitSFX = {},     -- EnemyThrowHit
        enemyTauntSFX = {},         -- EnemyGrowlAlert
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
        self._inIntro    = false
        self._introDone  = false
        self._lockAction = false
        self._lockReason = nil
        self._lockTimer  = 0

        -- phase tracking
        self._phase = 1                 -- current phase id
        self._lastPhaseProcessed = 1
        self._pendingPhase = nil        -- when transforming
        self._transforming = false

        -- hooked tracking
        self._hooked = false

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
    end,

    Start = function(self)
        -- Grab components (same pattern as EnemyAI)
        self._collider  = self:GetComponent("ColliderComponent")
        self._transform = self:GetComponent("Transform")
        self._rb        = self:GetComponent("RigidBodyComponent")
        self._animator  = self:GetComponent("AnimationComponent")
        self._audio     = self:GetComponent("AudioComponent")

        -- (Re)create controller safely
        if self._controller then
            pcall(function() CharacterController.DestroyByEntity(self.entityId) end)
            self._controller = nil
        end

        if not self._controller and self._collider and self._transform then
            local ok, ctrl = pcall(function()
                return CharacterController.Create(self.entityId, self._collider, self._transform)
            end)
            if ok then
                self._controller = ctrl
            else
                print("[MinibossAI] CharacterController.Create failed")
                self._controller = nil
            end
        end

        -- Set RB kinematic-ish like EnemyAI
        if self._rb then
            pcall(function() self._rb.motionID = 1 end)
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

            self._comboDamageSub = _G.event_bus.subscribe("deal_damage", function(payload)
                if not payload then return end
                
                -- Check if this enemy is the target
                if payload.targetEntityId ~= self.entityId then
                    return
                end
                
                -- Extract attack data from ComboManager
                local attackData = payload.attackData or {}
                local damage = attackData.damage or 10
                local knockback = attackData.knockback or 0
                local attackState = attackData.state or "unknown"
                local direction = payload.direction or { x = 0, y = 0, z = 1 }
                
                print(string.format("[EnemyAI] Taking damage: %d from attack '%s' (knockback: %.1f)", 
                                damage, attackState, knockback))
                
                -- Apply damage through existing system
                self:ApplyHit(damage, "COMBO")
                
                -- Apply knockback using CC system, not RB impulse
                if knockback and knockback > 0 then
                    -- direction should be from attacker -> enemy (or similar)
                    -- if it's opposite, just negate it
                    local dir = direction or { x = 0, z = 1 }
                    self._kbVX = (dir.x or 0) * knockback
                    self._kbVZ = (dir.z or 0) * knockback
                    self._kbT  = self.KnockbackDuration
                end
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
                playRandomSFX(self._audio, self.enemyMeleeHitSFX)
            end)
        end

        -- Seed prevY for grounded heuristic
        local _, y, _ = self:GetPosition()
        self._prevY = y

        self._phase = self:GetPhase()

        self.fsm:Change("Recover", self.states.Recover)
        self._recoverTimer = 999999 -- idle until aggro

        print("[Miniboss] KnifePool size =", #KnifePool.knives)
        print("[Miniboss KnifePool dbg] KnifePool table:", KnifePool, "activeCount:", KnifePool.activeCount, "poolSize:", KnifePool.poolSize)
    end,

    Update = function(self, dt)
        local dtSec = toDtSec(dt)

        -- 1. ALWAYS ENSURE COMPONENTS & APPLY GRAVITY
        -- This ensures the boss falls to the ground immediately on game start.
        self:EnsureController()
        self:ApplyGravity(dtSec)

        -- 2. TICK SYSTEM TIMERS (Always run)
        self._hitLockTimer = math.max(0, (self._hitLockTimer or 0) - dtSec)
        for k, v in pairs(self._moveCooldowns) do
            self._moveCooldowns[k] = math.max(0, v - dtSec)
        end

        -- 3. ACTION LOCK SYSTEM
        if self._lockAction and (self._lockReason ~= "DEAD") then
            self._lockTimer = math.max(0, (self._lockTimer or 0) - dtSec)

            if self._lockTimer <= 0 then
                local reason = self._lockReason
                self:UnlockActions()

                if reason == "PHASE_TRANSFORM" then
                    self:FinishPhaseTransform()
                    self.fsm:Change("Recover", self.states.Recover)
                elseif reason == "HOOKED" then
                    self._hooked = false
                    self._recoverTimer = math.max(self.RecoverDuration or 0.6, 0.35)
                    self.fsm:Change("Recover", self.states.Recover)
                end
            end
        end

        -- 4. PHYSICS SYNC (Crucial for preventing teleports)
        -- We sync the Transform to the CharacterController every frame.
        if self._controller then
            local pos = CharacterController.GetPosition(self._controller)
            if pos then
                self:SetPosition(pos.x, pos.y, pos.z)
                -- Only apply facing rotation if not in a special move like DeathLotus
                if (not self:IsInMove("DeathLotus")) and self._lastFacingRot then
                    local r = self._lastFacingRot
                    self:SetRotation(r.w, r.x, r.y, r.z)
                end
            end
        end

        -- 5. BOSS DEATH: fade to black then return to main menu
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
                        Scene.Load("Resources/Scenes/01_MainMenu.scene")
                    end
                end
            end
            return
        end

        -- EARLY EXIT FOR NON-INTRO LOCKS
        -- We allow the "INTRO" reason to pass through so the BattlecryState can update.
        local locked = self:IsActionLocked()
        local lockReason = self._lockReason
        if locked and lockReason ~= "INTRO" then
            return
        end

        -- 6. PHASE TRANSITION CHECK
        self:CheckPhaseTransition()
        if self:IsActionLocked() and self._lockReason == "PHASE_TRANSFORM" then
            return
        end

        -- 7. AGGRO TRIGGER LOGIC
        if not self._introDone then
            self:FacePlayer() -- Keep facing while waiting

            local px, py, pz = self:GetPlayerPosForAI()
            if px then
                local ex, ez = self:GetEnemyPosXZ()
                local dx, dz = px - ex, pz - ez
                local r = self.AggroRange or 15.0
                
                if (dx*dx + dz*dz) <= (r*r) then
                    -- IMPORTANT: Set these BEFORE changing state to prevent re-triggering
                    self._introDone = true 
                    self._inIntro = true
                    
                    print(string.format("[Miniboss][Aggro] Player in range. Starting Battlecry."))
                    self.fsm:Change("Battlecry", self.states.Battlecry)
                    return
                end
            end
            return -- Stay in Idle/Recover until player enters range
        end

        -- 8. NORMAL COMBAT BEHAVIOR
        self.fsm:Update(dtSec)
        self:TickMove(dtSec)

        -- 9. EVENT BROADCAST
        if _G.event_bus and _G.event_bus.publish then
            local x, y, z = self:GetPosition()
            _G.event_bus.publish("enemy_position", {
                entityId = self.entityId,
                x = x, y = y, z = z
            })
        end
    end,

    -------------------------------------------------
    -- Gravity (controller-based)
    -------------------------------------------------
    ApplyGravity = function(self, dtSec)
        if not self._controller then return end
        if dtSec <= 0 then return end

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
        if self._controller then return true end

        self._collider  = self._collider  or self:GetComponent("ColliderComponent")
        self._transform = self._transform or self:GetComponent("Transform")

        if not (self._collider and self._transform) then
            return false
        end

        local ok, ctrl = pcall(function()
            return CharacterController.Create(self.entityId, self._collider, self._transform)
        end)

        if ok and ctrl then
            self._controller = ctrl
            -- sync scripted position to CC immediately so there’s no later “snap”
            local x, y, z = self:GetPosition()
            pcall(function()
                CharacterController.SetPosition(self._controller, x, y, z)
            end)
            return true
        end

        return false
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
            print("[Miniboss] Can't find player")
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

    GetEnemyPosXZ = function(self)
        if self._controller then
            local pos = CharacterController.GetPosition(self._controller)
            if pos then return pos.x, pos.z end
        end
        local x, _, z = self:GetPosition()
        return x, z
    end,

    GetPlayerPosForAI = function(self)
        local tr = self._playerTr
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

    -------------------------------------------------
    -- Phase logic
    -------------------------------------------------
    GetPhase = function(self)
        local hpPct = (self.health or 0) / (self.MaxHealth or 1)

        for i = 1, #PHASE_THRESHOLDS do
            if hpPct <= PHASE_THRESHOLDS[i].hpPct then
                return PHASE_THRESHOLDS[i].id
            end
        end

        return 1
    end,

    GetCurrentPhase = function(self)
        return self._phase or self:GetPhase()
    end,

    CheckPhaseTransition = function(self)
        if self.dead then return end
        if self._transforming then return end

        local computedPhase = self:GetPhase()
        local last = self._lastPhaseProcessed or 1

        -- Only react to the *next* unprocessed phase
        if computedPhase > last then
            self:StartPhaseTransform(last + 1)
        end
    end,

    StartPhaseTransform = function(self, newPhase)
        if self.dead then return end
        if self._transforming then return end
        if newPhase ~= (self._lastPhaseProcessed + 1) then return end

        self._pendingPhase = newPhase
        self._transforming = true

        local dur = self.PhaseTransformDuration or 1.2
        self:LockActions("PHASE_TRANSFORM", dur)     
        print(string.format(
            "[Miniboss][Phase] START %d -> %d (%.2fs)",
            self._lastPhaseProcessed,
            newPhase,
            dur
        ))

        self._animator:SetTrigger("Taunt")
        -- Play taunt SFX during phase transition
        playRandomSFX(self._audio, self.enemyTauntSFX)
    end,

    FinishPhaseTransform = function(self)
        if not self._transforming then return end

        local newPhase = self._pendingPhase
        if newPhase then
            self._phase = newPhase
            self._lastPhaseProcessed = newPhase

            print(string.format(
                "[Miniboss][Phase] DONE -> phase=%d (hp=%.1f/%.1f)",
                newPhase,
                self.health or 0,
                self.MaxHealth or 1
            ))
        end

        self._pendingPhase = nil
        self._transforming = false

        self._recoverTimer = math.max(self.RecoverDuration or 0.6, 0.35)
    end,

    -------------------------------------------------
    -- Move selection
    -------------------------------------------------
    ChooseMove = function(self)
        local phase = self:GetPhase()
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
        if self._inIntro then return end
        if (self._hitLockTimer or 0) > 0 then return end
        if self._transforming then return end

        self._hitLockTimer = self.HitIFrame or 0.2
        self.health = math.max(0, (self.health or 0) - (dmg or 1))

        local myRandomValue = math.random(1, 3)
        if myRandomValue == 1 then
            self._animator:SetTrigger("Hurt1")
        elseif myRandomValue == 2 then
            self._animator:SetTrigger("Hurt2")
        else
            self._animator:SetTrigger("Hurt3")
        end

        print(string.format("[Miniboss][Hit] dmg=%s hp=%.1f/%.1f", tostring(dmg or 1), self.health, self.MaxHealth))

        if self.health <= 0 then
            self:Die()
            return
        end

        -- Play hurt SFX
        playRandomSFX(self._audio, self.enemyHurtSFX)

        -- if a hit causes a phase threshold to be crossed, transform immediately
        self:CheckPhaseTransition()

        -- IMPORTANT: if we just started transforming, don't apply HIT_STUN (it would overwrite the lock)
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

        self._hooked = true

        local dur = duration or self.HookedDuration or 4.0
        self:LockActions("HOOKED", dur)

        if self.ClipHooked and self.ClipHooked >= 0 and self.PlayClip then
            self:PlayClip(self.ClipHooked, true)
        end

        print(string.format("[Miniboss][Hooked] START %.2fs", dur))
    end,

    Die = function(self)
        if self.dead then return end
        self.dead = true

        -- Play death SFX
        playRandomSFX(self._audio, self.enemyDeathSFX)

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

        print("[Miniboss][Death] DEAD")
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
        end
        self._damageSub = nil
        self._comboDamageSub = nil
        self._hookSub = nil
        self._meleeHitSub = nil
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
        end
        self._damageSub = nil
        self._comboDamageSub = nil
        self._hookSub = nil
        self._meleeHitSub = nil
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
        return ex, (ey or 0) + 1.0, ez
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
            print("[Miniboss][Knife] Launch FAILED: knife=nil")
            return false
        end
        -- Pick a random ranged attack SFX for the knife to play from its position
        local sfxClip = nil
        local clips = self.enemyRangedAttackSFX
        if clips and #clips > 0 then
            sfxClip = clips[math.random(1, #clips)]
        end
        local ok = knife:Launch(sx, sy, sz, tx, ty, tz, token, tag, sfxClip)
        if not ok then
            print(string.format("[Miniboss][Knife] Launch FAILED tag=%s token=%s", tostring(tag), tostring(token)))
        end
        return ok
    end,

    -- 3-shot volley: center aimed + L/R perpendicular offsets
    SpawnKnifeVolley3 = function(self, spread)
        spread = spread or 1.0

        local knives = requestUpTo(3)
        if not knives then
            print("[Miniboss][Knife] No knives available in pool")
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
            print("[Miniboss][Knife] RequestMany(1) FAILED")
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

    -- 4-fan, no centered aimed shot: targets are perpendicular offsets only
    SpawnKnifeFan4_NoCenter = function(self, spread1, spread2)
        spread1 = spread1 or 0.8
        spread2 = spread2 or 1.6

        local knives = KnifePool.RequestMany(4)
        if not knives then
            print("[Miniboss][Knife] RequestMany(3) FAILED")
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
        for i=1,4 do
            knives[i]._reservedToken = token
            knives[i].reserved = true
        end

        local targets = {
            { px - rx*spread2, py, pz - rz*spread2, "L2" },
            { px - rx*spread1, py, pz - rz*spread1, "L1" },
            { px + rx*spread1, py, pz + rz*spread1, "R1" },
            { px + rx*spread2, py, pz + rz*spread2, "R2" },
        }

        local okAll = true
        for i=1,4 do
            local t = targets[i]
            okAll = self:_LaunchKnife(knives[i], sx, sy, sz, t[1], t[2], t[3], token, t[4]) and okAll
        end

        if not okAll then
            for i=1,4 do if knives[i] then knives[i]:Reset() end end
            return false
        end
        return true
    end,

    -- forward spray (not aimed): shoot 3 knives in facing direction with sideways offsets
    SpawnForwardSpray3 = function(self, fx, fz, range, spread, yOffset)
        range = range or 12.0
        spread = spread or 0.6

        local knives = KnifePool.RequestMany(3)
        if not knives then
            print("[Miniboss][Knife] RequestMany(3) FAILED")
            return false
        end

        local sx, sy, sz = self:_GetSpawnPos()
        if not sx then
            self:_FreeReserved(knives)
            return false
        end

        local len = math.sqrt((fx or 0)*(fx or 0) + (fz or 0)*(fz or 0))
        if len < 1e-6 then
            self:_FreeReserved(knives)
            return false
        end
        fx, fz = fx/len, fz/len
        local rx, rz = -fz, fx

        local token = self:_NewVolleyToken()
        for i=1,3 do
            knives[i]._reservedToken = token
            knives[i].reserved = true
        end

        local _, py, _ = self:_GetPlayerPos(yOffset or 0.0)
        local targetY = py or sy
        local baseTx, baseTy, baseTz = sx + fx*range, targetY, sz + fz*range

        local t0x, t0y, t0z = baseTx, baseTy, baseTz
        local t1x, t1y, t1z = baseTx - rx*spread, baseTy, baseTz - rz*spread
        local t2x, t2y, t2z = baseTx + rx*spread, baseTy, baseTz + rz*spread

        local ok1 = self:_LaunchKnife(knives[1], sx, sy, sz, t0x, t0y, t0z, token, "F")
        local ok2 = self:_LaunchKnife(knives[2], sx, sy, sz, t1x, t1y, t1z, token, "FL")
        local ok3 = self:_LaunchKnife(knives[3], sx, sy, sz, t2x, t2y, t2z, token, "FR")

        if not (ok1 and ok2 and ok3) then
            for i=1,3 do if knives[i] then knives[i]:Reset() end end
            return false
        end
        return true
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

    TickMove = function(self, dtSec)
        if self._moveFinished or not self._move then return end
        local m = self._move
        m.t = (m.t or 0) + dtSec

        -------------------------------------------------
        -- Move1: Basic Attack (single volley)
        -------------------------------------------------
        if m.kind == "Basic" then
            -- fire once at start
            if m.step == 0 then
                self:FacePlayer()
                print("[MinibossAI] SPAWNING BASIC")
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
                print("[MinibossAI] SPAWNING BURSTFIRE")
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
                print("[MinibossAI] SPAWNING AntiDodge")
                self:SpawnKnifeFan4_NoCenter(m.spread1 or 0.9, m.spread2 or 1.8)
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
                    -- Example hooks (only if you have them):
                    -- if self.PlayClip and self.ClipCharge then self:PlayClip(self.ClipCharge, false) end
                    -- if _G.event_bus and _G.event_bus.publish then _G.event_bus.publish("miniboss_charge", { entityId=self.entityId }) end
                    if _G.event_bus and _G.event_bus.publish then
                        _G.event_bus.publish("meleeHitPlayerDmg", {
                            dmg = 2,
                            src = "Miniboss",
                            enemyEntityId = self.entityId,
                        })
                    end
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
                end

                return
            end

            -- Step 1: dash phase
            local dashDur   = m.dashDur or 0.22
            local dashSpeed = m.dashSpeed or 18.0

            if m.step == 1 then
                m.dashT = (m.dashT or 0) + dtSec

                if self._controller then
                    CharacterController.Move(
                        self._controller,
                        m.dx * dashSpeed * dtSec,
                        0,
                        m.dz * dashSpeed * dtSec
                    )
                end

                -- slash timing: you can do "at end" or "near end"
                local slashAt = m.slashAt or 0.85 -- fraction of dash
                if not m.slashed and m.dashT >= (dashDur * slashAt) then
                    m.slashed = true

                    if _G.event_bus and _G.event_bus.publish then
                        local ex, ey, ez = self:GetPosition()
                        _G.event_bus.publish("miniboss_slash", {
                            entityId = self.entityId,
                            x = ex, y = ey, z = ez,
                            radius = m.slashRadius or 1.4,
                            dmg = m.dmg or 1,

                            kbStrength = m.kbStrength or 8.0,
                            kbUp = 0.0,
                        })
                    end
                end

                if m.dashT >= dashDur then
                    m.step = 2
                    m.recoverT = 0
                end

                return
            end

            -- Step 2: recovery
            if m.step == 2 then
                m.recoverT = (m.recoverT or 0) + dtSec
                if m.recoverT >= (m.postDelay or 0.55) then
                    self:_EndMove()
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
                print("[MinibossAI] SPAWNING DEATHLOTUS")
                self:SpawnForwardSpray3(fx, fz, m.range or 12.0, m.spread or 0.7, m.lotusYOffset or 0.0)
            end

            if m.t >= dur then
                self:_EndMove()
            end
            return
        end
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
            postDelay = 0.45
        })
    end,

    FateSealed = function(self)
        self._animator:SetTrigger("Melee")
        playRandomSFX(self._audio, self.enemyMeleeAttackSFX)
        self:_BeginMove("FateSealed", {
            chargeDur = 2.00,
            dashDur = 0.33,
            dashSpeed = 290.0,
            slashAt = 0.90,
            slashRadius = 1.4,
            dmg = 1,
            kbStrength = 8.0,
            postDelay = 2.60
        })
    end,

    DeathLotus = function(self)
        self._animator:SetTrigger("Ranged")
        self:_BeginMove("DeathLotus", {
            duration = 2.8,
            spinSpeed = math.pi * 1.8,  -- rad/s
            fireInterval = 0.10,
            range = 12.0,
            spread = 0.7,
            lotusYOffset = -3.0,
        })
    end,
}