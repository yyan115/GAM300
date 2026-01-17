-- Resources/Scripts/Gameplay/GroundAttackState.lua
local AttackState = {}

local function stopCC(ai)
    -- so calling Move(0,0) once per frame is okay.
    if ai.MoveCC then
        ai:MoveCC(0, 0)
    end
end

function AttackState:Enter(ai)
    ai:PlayClip(ai.clips.Attack, true)
    ai.attackTimer = 0
end

function AttackState:Update(ai, dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return end
    if dtSec > 0.05 then dtSec = 0.05 end

    -- Always face player while attacking
    ai:FacePlayer()

    local attackR, diseng = ai:GetRanges()
    local d2 = ai:GetPlayerDistanceSq()

    -- Too far -> Patrol
    if d2 > (diseng * diseng) then
        stopCC(ai)
        ai.fsm:Change("Patrol", ai.states.Patrol)
        return
    end

    -- Not in attack range -> Chase
    if d2 >= (attackR * attackR) then
        stopCC(ai)
        ai.fsm:Change("Chase", ai.states.Chase)
        return
    end

    -- In attack range -> STOP + SHOOT
    stopCC(ai)

    ai.attackTimer = (ai.attackTimer or 0) + dtSec
    if ai.attackTimer >= (ai.config.AttackCooldown or 1.0) then
        ai.attackTimer = 0
        ai:SpawnKnife()
    end
end

function AttackState:Exit(ai)
    stopCC(ai)
end

return AttackState