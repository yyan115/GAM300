-- Scripts/AI/states/GroundIdleState.lua
local IdleState = {}

function IdleState:Enter(ai)
    ai:PlayClip(ai.clips.Idle, true)
end

function IdleState:Update(ai, dt)
    -- Transition condition
    if ai:IsPlayerInRange(ai.config.DetectionRange) then
        ai.fsm:Change("Attack", ai.states.Attack)
        return
    end

    -- (Priority 3) if you later want patrol:
    -- if ai:ShouldPatrol() then ai.fsm:Change("Patrol", ai.states.Patrol) end
end

function IdleState:Exit(ai)
end

return IdleState
