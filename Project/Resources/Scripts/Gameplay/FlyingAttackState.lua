-- Resources/Scripts/GamePlay/FlyingAttackState.lua
local FlyingAttack = {}

local function toDtSec(dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return 0 end
    if dtSec > 0.05 then dtSec = 0.05 end
    return dtSec
end

local function interruptOut(ai)
    ai._animator:SetBool("ReadyToAttack", false)
    ai._readyLatched = false
    ai._readySettleT = 0
    ai._didAttackThisCycle = false
    ai._attackWindupT = 0

    local attackR, _, diseng = ai:GetRanges()
    if not ai:IsPlayerInRange(diseng) then
        ai.fsm:Change("Chase", ai.states.Chase)
    elseif not ai:IsPlayerInRange(attackR) then
        ai.fsm:Change("Chase", ai.states.Chase)
    else
        ai.fsm:Change("Hurt", ai.states.Hurt)
    end
end

function FlyingAttack:Enter(ai)
    ai._currentAttackToken = ai:BeginAttackWindow()

    ai._attackT = 0
    ai._attackWindupT = 0
    ai._didAttackThisCycle = false

    ai._attackCooldownT = ai._attackCooldownT or 0

    -- READY GATE
    ai._readySettleT = 0
    ai._readyLatched = false

    ai._animator:SetBool("Flying", true)
    ai._animator:SetBool("PlayerInAttackRange", true)
    ai._animator:SetBool("PlayerInDetectionRange", false)
    ai._animator:SetBool("PatrolEnabled", false)
    ai._animator:SetBool("ReadyToAttack", false)
end

function FlyingAttack:Update(ai, dt)
    local dtSec = toDtSec(dt)
    if dtSec <= 0 then return end

    ai:MaintainHover(dtSec)
    ai:FacePlayer()

    local attackR, _, diseng = ai:GetRanges()

    -- if player left disengage range -> chase
    if not ai:IsPlayerInRange(diseng) then
        ai._animator:SetBool("ReadyToAttack", false)
        ai._animator:SetBool("PlayerInAttackRange", false)
        ai._readyLatched = false
        ai.fsm:Change("Chase", ai.states.Chase)
        return
    end

    -- if this attack window was interrupted (hurt / hook / knockup / token invalid), stop here
    if not ai:IsAttackWindowValid(ai._currentAttackToken) then
        interruptOut(ai)
        return
    end

    -- cooldown ticking
    ai._attackCooldownT = math.max(0, (ai._attackCooldownT or 0) - dtSec)

    -- If not in attack range yet, keep moving closer
    if not ai:IsPlayerInRange(attackR) then
        ai._animator:SetBool("ReadyToAttack", false)
        ai._animator:SetBool("PlayerInAttackRange", false)
        ai._readyLatched = false
        ai._readySettleT = 0

        ai:MoveTowardPlayerXZ_Flying(dtSec, ai.FlyingChaseSpeed or 1.2)
        return
    end

    -- In attack range -> latch ReadyToAttack after settle
    local settleDelay = ai.ReadyToAttackDelay or 0.15
    if not ai._readyLatched then
        ai._readySettleT = (ai._readySettleT or 0) + dtSec
        if ai._readySettleT >= settleDelay then
            ai._readyLatched = true
            ai._animator:SetBool("ReadyToAttack", true)
            ai._animator:SetBool("PlayerInAttackRange", true)
        end
    end

    -- ready to attack?
    if (ai._attackCooldownT or 0) > 0 then
        return
    end

    -- windup (sync with animation)
    local windup = tonumber(ai.RangedAnimDelay) or 0
    ai._attackWindupT = (ai._attackWindupT or 0) + dtSec

    if ai._attackWindupT < windup then
        return
    end

    -- interrupted during windup? do not fire
    if not ai:IsAttackWindowValid(ai._currentAttackToken) then
        interruptOut(ai)
        return
    end

    -- do attack once per cycle
    if ai._didAttackThisCycle then
        return
    end
    ai._didAttackThisCycle = true

    -- Ranged: throw knives
    ai._animator:SetBool("Ranged", true)

    -- final guard before firing
    if not ai:IsAttackWindowValid(ai._currentAttackToken) then
        interruptOut(ai)
        return
    end

    local ok = ai:SpawnKnife()

    -- restart cooldown only if the attack really fired
    if ok then
        ai._attackCooldownT = tonumber(ai.AttackCooldown) or 3.0
    else
        ai._attackCooldownT = 0
    end

    -- reset windup for next cycle
    ai._attackWindupT = 0
    ai._didAttackThisCycle = false
end

function FlyingAttack:Exit(ai)
    ai:CancelPendingAttack("EXIT_ATTACK")

    ai._animator:SetBool("PlayerInAttackRange", false)
    ai._animator:SetBool("Ranged", false)

    -- Clear ready gate on exit
    ai._animator:SetBool("ReadyToAttack", false)
    ai._readySettleT = 0
    ai._readyLatched = false
end

return FlyingAttack