-- Resources/Scripts/GamePlay/FlyingChaseState.lua
local FlyingChase = {}

function FlyingChase:Enter(ai)
    ai._alerted = ai._alerted or false
    if not ai._alerted then
        ai._alerted = true
        if ai.PlayAlertSFX then ai:PlayAlertSFX() end
    end

    ai._animator:SetBool("Flying", true)
    ai._animator:SetBool("PatrolEnabled", true)
    ai._animator:SetBool("PlayerInDetectionRange", true)
end

function FlyingChase:Update(ai, dt)
    ai:MaintainHover(dt)

    local detR = ai.DetectionRange or 4.0
    if not ai:IsPlayerInRange(detR) then
        -- go back to patrol if enabled, else idle
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