-- Resources/Scripts/GamePlay/FlyingHookedState.lua
local FlyingHooked = {}

function FlyingHooked:Enter(ai)
    if ai._animator then
        ai._animator:SetBool("Hooked", true)
    end

    ai._hookedTimer = 0
    ai.attackTimer = 0

    -- Convert immediately: flying enemy becomes ground enemy
    if ai.ConvertToGroundEnemy then
        ai:ConvertToGroundEnemy({ nextState = "Hooked" })
    end
end

function FlyingHooked:Update(ai, dt)
    -- usually won't run (we convert on enter)
    if ai.health <= 0 then ai.dead = true return end
end

function FlyingHooked:Exit(ai)
    if ai._animator then
        ai._animator:SetBool("Hooked", false)
    end
end

return FlyingHooked