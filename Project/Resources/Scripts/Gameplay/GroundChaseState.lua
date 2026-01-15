-- Resources/Scripts/Gameplay/GroundChaseState.lua
local ChaseState = {}

function ChaseState:Enter(ai)
    ai:PlayClip(ai.clips.Walk or ai.clips.Idle, true)
end

function ChaseState:Update(ai, dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return end
    if dtSec > 0.05 then dtSec = 0.05 end

    local chaseSpd = ai.config.ChaseSpeed or 1.8
    local attackR, diseng = ai:GetRanges()
    local d2 = ai:GetPlayerDistanceSq()

    -- Too far -> Patrol
    if d2 > (diseng * diseng) then
        if ai.MoveCC then ai:MoveCC(0, 0) end
        ai.fsm:Change("Patrol", ai.states.Patrol)
        return
    end

    -- Close enough -> Attack
    if d2 <= (attackR * attackR) then
        if ai.MoveCC then ai:MoveCC(0, 0) end
        ai.fsm:Change("Attack", ai.states.Attack)
        return
    end

    -- Chase: always move here (no dead zone)
    ai:FacePlayer()

    local tr = ai._playerTr
    if not tr then return end
    local pp = Engine.GetTransformPosition(tr)
    local px, pz = pp[1], pp[3]

    local ex, ez = ai:GetEnemyPosXZ()
    local dx, dz = px - ex, pz - ez
    local d2b = dx*dx + dz*dz
    if d2b < 1e-6 then
        if ai.MoveCC then ai:MoveCC(0, 0) end
        return
    end

    local d = math.sqrt(d2b)
    local dirX, dirZ = dx / d, dz / d
    if ai.MoveCC then
        ai:MoveCC(dirX * chaseSpd, dirZ * chaseSpd)
    end
end

return ChaseState