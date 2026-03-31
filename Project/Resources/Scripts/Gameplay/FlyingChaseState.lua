-- Resources/Scripts/GamePlay/FlyingChaseState.lua
local FlyingChase = {}

function FlyingChase:Enter(ai)
    ai._animator:SetBool("Flying", true)
    ai._animator:SetBool("PatrolEnabled", false)
    ai._animator:SetBool("PlayerInDetectionRange", true)
    ai._animator:SetBool("PlayerInAttackRange", false)
    ai._animator:SetBool("ReadyToAttack", false)
    ai:_publishSFX("alert")
end

function FlyingChase:Update(ai, dt)
    ai:MaintainHover(dt)

    local detR = ai.DetectionRange or 4.0
    if not ai:IsPlayerInRange(detR) and not ai.aggressive then
        if ai.EnablePatrol then
            ai.fsm:Change("Patrol", ai.states.Patrol)
        else
            ai.fsm:Change("Idle", ai.states.Idle)
        end
        return
    end

    local attackR = ai.AttackRange or 3.0
    if ai:IsPlayerInRange(attackR) then
        ai.fsm:Change("Attack", ai.states.Attack)
        return
    end

    -- chase
    local spd = ai.FlyingChaseSpeed or 1.2
    ai:MoveTowardPlayerXZ_Flying(dt, spd)
end

return FlyingChase