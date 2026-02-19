-- Resources/Scripts/GamePlay/FlyingChaseState.lua
local FlyingChase = {}

function FlyingChase:Enter(ai)
    if ai._animator then
        ai._animator:SetBool("Flying", true)
        ai._animator:SetBool("PatrolEnabled", false)
    end
end

function FlyingChase:Update(ai, dt)
    if ai.health <= 0 then ai.dead = true return end

    -- keep hover height
    if ai.MaintainHover then
        ai:MaintainHover(dt)
    end

    local attackR = ai.FlyingAttackRange or (ai.config.AttackRange or 3.0)
    local diseng  = ai.config.AttackDisengageRange or (ai.config.DetectionRange or 4.0)
    local d2 = ai:GetPlayerDistanceSq()

    if d2 > (diseng * diseng) then
        ai.fsm:Change("Idle", ai.states.Idle)
        return
    end

    if d2 <= (attackR * attackR) then
        -- reuse existing attack for now (it already faces player + spawns knives)
        if not ai.IsPassive then
            ai.fsm:Change("Attack", ai.states.Attack)
        else
            ai.fsm:Change("Idle", ai.states.Idle)
        end
        return
    end

    -- move in XZ toward player
    if ai.MoveTowardPlayerXZ_Flying then
        ai:MoveTowardPlayerXZ_Flying(dt, ai.FlyingChaseSpeed or 1.2)
    end
end

function FlyingChase:Exit(ai) end

return FlyingChase