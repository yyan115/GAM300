-- Resources/Scripts/Gameplay/GroundIdleState.lua
local IdleState = {}

function IdleState:Enter(ai)
    ai:PlayClip(ai.clips.Idle, true)
end

function IdleState:Update(ai, dt)
    print("[FSM][Idle] Update tick")

    if ai:IsPlayerInRange(ai.config.DetectionRange) then
        print("[FSM][Idle] Player detected → Attack")
        ai.fsm:Change("Attack", ai.states.Attack)
        return
    end

    if ai.config.EnablePatrol then
        print("[FSM][Idle] EnablePatrol=true → Patrol")
        ai.fsm:Change("Patrol", ai.states.Patrol)
        return
    end
end

function IdleState:Exit(ai)
end

return IdleState
