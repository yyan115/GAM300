-- Resources/Scripts/GamePlay/FlyingIdleState.lua
local FlyingIdle = {}

function FlyingIdle:Enter(ai)
    if ai._animator then
        ai._animator:SetBool("Flying", true)

        -- IMPORTANT: don't force this false
        ai._animator:SetBool("PatrolEnabled", ai.EnablePatrol and true or false)

        ai._animator:SetBool("Moving", false)
        ai._animator:SetBool("Attacking", false)
    end
end

function FlyingIdle:Update(ai, dt)
    if ai.health <= 0 then ai.dead = true return end

    if ai.MaintainHover then
        ai:MaintainHover(dt)
    end

    -- detect player -> Chase
    local detR = ai.DetectionRange or 4.0
    if ai:IsPlayerInRange(detR) then
        ai.fsm:Change("Chase", ai.states.Chase)
        return
    end

    -- idle -> patrol (so it doesn't hover forever)
    if ai.EnablePatrol and ai.states.Patrol then
        ai.fsm:Change("Patrol", ai.states.Patrol)
        return
    end
end

function FlyingIdle:Exit(ai) end
return FlyingIdle