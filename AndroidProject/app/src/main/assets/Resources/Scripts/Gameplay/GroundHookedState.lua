-- Resources/Scripts/Gameplay/GroundHookedState.lua
local HookedState = {}

function HookedState:Enter(ai)
    ai._hookedTimer = 0

    -- If you have a specific clip, use it. Otherwise reuse hurt for now.
    local clip = ai.clips.Hooked or ai.clips.Hurt
    ai:PlayClip(clip, false)

    -- Optional: stop attacks immediately
    ai.attackTimer = 0
end

function HookedState:Update(ai, dt)
    ai._hookedTimer = (ai._hookedTimer or 0) + dt

    -- Still allow death to override
    if ai.health <= 0 then
        ai.fsm:Change("Death", ai.states.Death)
        return
    end

    if ai._hookedTimer >= ai.config.HookedDuration then
        ai._hookedTimer = 0

        if ai:IsPlayerInRange(ai.config.DetectionRange) then
            ai.fsm:Change("Attack", ai.states.Attack)
        else
            ai.fsm:Change("Idle", ai.states.Idle)
        end
    end
end

return HookedState
