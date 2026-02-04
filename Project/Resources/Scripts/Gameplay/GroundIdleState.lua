-- Resources/Scripts/GamePlay/GroundIdleState.lua
local IdleState = {}

function IdleState:Enter(ai)
    print("[GroundIdleState] ENTER")
    ai._animator:SetBool("PatrolEnabled", false)
end

function IdleState:Update(ai, dt)
    if ai:IsPlayerInRange(ai.config.AttackDisengageRange or ai.config.DetectionRange) then
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
