-- Resources/Scripts/GamePlay/FlyingChaseState.lua
local FlyingChase = {}

function FlyingChase:Enter(ai)
    ai._animator:SetBool("Flying", true)
    ai._animator:SetBool("PatrolEnabled", false)
    ai._animator:SetBool("PlayerInDetectionRange", true)
    ai._animator:SetBool("PlayerInAttackRange", false)
    ai._animator:SetBool("ReadyToAttack", false)
    ai._chaseLosLostT = 0
    ai:_publishSFX("alert")
end

function FlyingChase:Update(ai, dt)
    ai:MaintainHover(dt)

    local detR = ai.DetectionRange or 4.0
    if not ai:IsPlayerInRange(detR) then
        -- Clear aggressive so flying enemies actually disengage.
        ai.aggressive = false
        if ai.EnablePatrol then
            ai.fsm:Change("Patrol", ai.states.Patrol)
        else
            ai.fsm:Change("Idle", ai.states.Idle)
        end
        return
    end

    -- Lose chase if LOS blocked for sustained period (wall between enemy and player)
    local losGrace = tonumber(ai.LOSGracePeriod) or 2.0
    if not ai:HasLineOfSight() then
        ai._chaseLosLostT = (ai._chaseLosLostT or 0) + dt
        if ai._chaseLosLostT >= losGrace then
            ai.aggressive = false
            ai._chaseLosLostT = 0
            if ai.EnablePatrol then
                ai.fsm:Change("Patrol", ai.states.Patrol)
            else
                ai.fsm:Change("Idle", ai.states.Idle)
            end
            return
        end
    else
        ai._chaseLosLostT = 0
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