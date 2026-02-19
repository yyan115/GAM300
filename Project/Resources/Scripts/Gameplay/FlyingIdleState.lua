-- Resources/Scripts/GamePlay/FlyingIdleState.lua
local FlyingIdle = {}

function FlyingIdle:Enter(ai)
    if ai._animator then
        ai._animator:SetBool("Flying", true)
        ai._animator:SetBool("PatrolEnabled", false)
    end
end

function FlyingIdle:Update(ai, dt)
    if ai.health <= 0 then ai.dead = true return end

    if ai.MaintainHover then
        ai:MaintainHover(dt)
    end

    if ai:IsPlayerInRange(ai.config.AttackDisengageRange or ai.config.DetectionRange) then
        ai.fsm:Change("Chase", ai.states.Chase)
        return
    end
end

function FlyingIdle:Exit(ai) end

return FlyingIdle