-- Resources/Scripts/GamePlay/FlyingHookedState.lua
local FlyingHooked = {}

function FlyingHooked:Enter(ai)
    if ai._animator then
        --ai._animator:SetBool("Hooked", true)
    end

    ai._hookedTimer = 0
    ai.attackTimer = 0

    if ai.BeginSlamDown then
        ai:BeginSlamDown()
    else
        -- fallback (if slam isn't present for some reason)
        if ai.ConvertToGroundEnemy then
            ai:ConvertToGroundEnemy({ nextState = "Hooked" })
        end
    end
end

function FlyingHooked:Update(ai, dt)
    -- While slamming, do nothing here.
    -- EnemyAI.Update() will call UpdateSlamDown() and convert on landing.
    if ai.health <= 0 then ai.dead = true return end
end

function FlyingHooked:Exit(ai)
    if ai._animator then
        --ai._animator:SetBool("Hooked", false)
    end
end

return FlyingHooked