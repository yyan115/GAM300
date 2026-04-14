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
    if ai.aggressive then
        ai.fsm:Change("Chase", ai.states.Chase)
        return
    end

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

    -- Drift back toward spawn if far from home (no patrol to carry us back)
    if ai._spawnX and ai._spawnZ and ai.MoveTowardPlayerXZ_Flying then
        local ex, _, ez = ai:GetPosition()
        if ex then
            local dx = ex - ai._spawnX
            local dz = ez - ai._spawnZ
            if (dx*dx + dz*dz) > 1.0 then
                -- Reuse flying move but aim at spawn instead of player
                local d = math.sqrt(dx*dx + dz*dz)
                local dirX, dirZ = -dx / d, -dz / d
                local spd = ai.FlyingChaseSpeed or 0.8
                local step = spd * dt
                if step > 0.25 then step = 0.25 end
                local _, y, _ = ai:GetPosition()
                ai:SetPosition(ex + dirX * step, y, ez + dirZ * step)
                ai:FaceDirection(dirX, dirZ)
            end
        end
    end
end

function FlyingIdle:Exit(ai) end
return FlyingIdle