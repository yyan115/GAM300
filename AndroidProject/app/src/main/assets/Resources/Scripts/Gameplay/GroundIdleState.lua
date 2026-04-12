-- Resources/Scripts/GamePlay/GroundIdleState.lua
local IdleState = {}

function IdleState:Enter(ai)
    ai._animator:SetBool("PatrolEnabled", false)
    ai._animator:SetBool("PlayerInAttackRange", false)
    ai._animator:SetBool("ReadyToAttack", false)
    ai._animator:SetBool("Melee", false)
    ai._animator:SetBool("Ranged", false)
end

function IdleState:Update(ai, dt)
    if ai.aggressive then
        ai.fsm:Change("Chase", ai.states.Chase)
        return
    end

    if ai:IsPlayerInRange(ai.config.AttackDisengageRange or ai.config.DetectionRange) and ai:HasLineOfSight() then
        ai.fsm:Change("Chase", ai.states.Chase)
        return
    end

    if ai.config.EnablePatrol then
        ai.fsm:Change("Patrol", ai.states.Patrol)
        return
    end
end

function IdleState:Exit(ai)
end

return IdleState
