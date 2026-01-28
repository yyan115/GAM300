-- Resources/Scripts/GamePlay/MinibossAI.lua
require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local StateMachine = require("GamePlay.StateMachine")
local ChooseState  = require("GamePlay.MinibossChooseState")
local ExecuteState = require("GamePlay.MinibossExecuteState")
local RecoverState = require("GamePlay.MinibossRecoverState")

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
    Move1 = { cooldown = 2.0, weights = { [1]=50, [2]=40, [3]=10, [4]=0 }, execute = function(ai) print("[Miniboss] Move1: Knife Volley") ai:KnifeVolley() end },
    Move2 = { cooldown = 2.5, weights = { [1]=50, [2]=30, [3]=30, [4]=0 }, execute = function(ai) print("[Miniboss] Move2: Knife Spin") ai:KnifeSpin() end },
    Move3 = { cooldown = 3.0, weights = { [1]=0,  [2]=30, [3]=30, [4]=10 }, execute = function(ai) print("[Miniboss] Move3: Dash Slash") ai:DashSlash() end },
    Move4 = { cooldown = 4.0, weights = { [1]=0,  [2]=0,  [3]=30, [4]=40 }, execute = function(ai) print("[Miniboss] Move4: Flight") ai:Flight() end },
    Move5 = { cooldown = 5.0, weights = { [1]=0,  [2]=0,  [3]=0,  [4]=50 }, execute = function(ai) print("[Miniboss] Move5: Frenzy Combo") ai:FrenzyCombo() end },
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

        -- Damage / Hook / Death (EnemyAI-style)
        HitIFrame      = 0.2,
        HookedDuration = 4.0,

        -- “Transformation” (phase transition) lock
        PhaseTransformDuration = 3.2,

        PlayerName = "Player",

        -- Gravity tuning (copied style from EnemyAI)
        Gravity      = -9.81,
        MaxFallSpeed = -25.0,

        -- Optional: small “stick to ground” behaviour
        GroundStickVel = -0.2,
    },

    Awake = function(self)
        self.health = self.MaxHealth
        self.dead = false

        self.fsm = StateMachine.new(self)
        self.states = {
            Choose  = ChooseState,
            Execute = ExecuteState,
            Recover = RecoverState,
        }

        self._moveCooldowns = {}
        self.currentMove = nil
        self.currentMoveDef = nil
        self._recoverTimer = 0
        self._hitLockTimer = 0

        -- action lock system (blocks Choose/Execute/etc)
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
        end

        -- === Hook event subscription (optional but matches your EnemyAI pattern) ===
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

        -- Seed prevY for grounded heuristic
        local _, y, _ = self:GetPosition()
        self._prevY = y

        self._phase = self:GetPhase()

        self.fsm:Change("Choose", self.states.Choose)
    end,

    Update = function(self, dt)
        --if Input.IsActionJustPressed("DebugHit") then self:ApplyHit(1) end
        local dtSec = toDtSec(dt)

        -- tick hit i-frames
        self._hitLockTimer = math.max(0, (self._hitLockTimer or 0) - dtSec)

        -- Tick move cooldowns
        for k, v in pairs(self._moveCooldowns) do
            self._moveCooldowns[k] = math.max(0, v - dtSec)
        end

        -- Apply gravity via CC (so miniboss falls like EnemyAI CC users)
        self:ApplyGravity(dtSec)

        -- === Action lock timer ===
        if self._lockAction and (self._lockReason ~= "DEAD") then
            self._lockTimer = math.max(0, (self._lockTimer or 0) - dtSec)

            if self._lockTimer <= 0 then
                local reason = self._lockReason
                self:UnlockActions()

                if reason == "PHASE_TRANSFORM" then
                    self:FinishPhaseTransform()
                    -- after transform, go Recover to pace
                    self.fsm:Change("Recover", self.states.Recover)
                elseif reason == "HOOKED" then
                    self._hooked = false
                    print("[Miniboss][Hooked] END")
                    self._recoverTimer = math.max(self.RecoverDuration or 0.6, 0.35)
                    self.fsm:Change("Recover", self.states.Recover)
                else
                    -- e.g. HIT_STUN ends -> just continue
                end
            end
        end

        -- === If dead or locked, do NOT run normal behaviour ===
        if self.dead or self:IsActionLocked() then
            -- still sync transform from CC
            if self._controller then
                local pos = CharacterController.GetPosition(self._controller)
                if pos then
                    self:SetPosition(pos.x, pos.y, pos.z)
                    if self._lastFacingRot then
                        local r = self._lastFacingRot
                        self:SetRotation(r.w, r.x, r.y, r.z)
                    end
                end
            end
            return
        end

        -- check for phase threshold crossing during normal time (not only on hit)
        self:CheckPhaseTransition()
        if self:IsActionLocked() then
            -- phase transform may have started
            return
        end

        -- Run behaviour
        self.fsm:Update(dtSec)

        if self._controller then
            local pos = CharacterController.GetPosition(self._controller)
            if pos then
                self:SetPosition(pos.x, pos.y, pos.z)

                -- restore yaw after CC sync
                if self._lastFacingRot then
                    local r = self._lastFacingRot
                    self:SetRotation(r.w, r.x, r.y, r.z)
                end
            end
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
        CharacterController.Move(self._controller, 0, self._vy, 0)

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

    -------------------------------------------------
    -- Facing (copied from EnemyAI)
    -------------------------------------------------
    ApplyRotation = function(self, w, x, y, z)
        self._lastFacingRot = { w = w, x = x, y = y, z = z }
        self:SetRotation(w, x, y, z)
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

        local q = { yawQuatFromDir(dx, dz) }
        if #q == 0 then return end

        self:ApplyRotation(q[1], q[2], q[3], q[4])
    end,

    GetEnemyPosXZ = function(self)
        if self._controller then
            local pos = CharacterController.GetPosition(self._controller)
            if pos then return pos.x, pos.z end
        end
        local x, _, z = self:GetPosition()
        return x, z
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
        -- TEMP: immediate completion, same as before
        return true
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
        if (self._hitLockTimer or 0) > 0 then return end

        self._hitLockTimer = self.HitIFrame or 0.2
        self.health = math.max(0, (self.health or 0) - (dmg or 1))

        print(string.format("[Miniboss][Hit] dmg=%s hp=%.1f/%.1f", tostring(dmg or 1), self.health, self.MaxHealth))

        if self.health <= 0 then
            self:Die()
            return
        end

        -- if a hit causes a phase threshold to be crossed, transform immediately
        self:CheckPhaseTransition()

        -- IMPORTANT: if we just started transforming, don't apply HIT_STUN (it would overwrite the lock)
        if self._transforming or (self._lockReason == "PHASE_TRANSFORM") then
            return
        end

        -- Optional: tiny hit-stun lock
        self:LockActions("HIT_STUN", 0.15)

        if self.ClipHurt and self.ClipHurt >= 0 and self.PlayClip then
            self:PlayClip(self.ClipHurt, false)
        end
    end,

    ApplyHook = function(self, duration)
        if self.dead then return end
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

        -- hard-lock forever
        self._lockAction = true
        self._lockReason = "DEAD"
        self._lockTimer = 999999

        -- stop movement (optional)
        if self._controller then
            pcall(function() CharacterController.Move(self._controller, 0, 0, 0) end)
        end
        self._vy = 0

        if self.ClipDeath and self.ClipDeath >= 0 and self.PlayClip then
            self:PlayClip(self.ClipDeath, false)
        end

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
            if self._hookSub then pcall(function() _G.event_bus.unsubscribe(self._hookSub) end) end
        end
        self._damageSub = nil
        self._hookSub = nil
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
            if self._hookSub then pcall(function() _G.event_bus.unsubscribe(self._hookSub) end) end
        end
        self._damageSub = nil
        self._hookSub = nil
    end,

    -------------------------------------------------
    -- Move implementations (stub)
    -- IMPORTANT: moves can call self:FacePlayer() if needed.
    -------------------------------------------------
    KnifeVolley   = function(self) end,
    KnifeSpin     = function(self) end,
    DashSlash     = function(self) end,
    Flight        = function(self) end,
    FrenzyCombo   = function(self) end,
}