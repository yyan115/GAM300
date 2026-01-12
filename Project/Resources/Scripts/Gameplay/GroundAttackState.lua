-- Gameplay/EnemyStates/GroundAttackState.lua
local AttackState = {}

function AttackState:Enter(ai)
    ai:PlayClip(ai.clips.Attack, true)
    ai.attackTimer = 0
end

function AttackState:Update(ai, dt)
    ai:FacePlayer()

    if not ai:IsPlayerInRange(ai.config.DetectionRange) then
        ai.fsm:Change("Idle", ai.states.Idle)
        return
    end

    ai.attackTimer = ai.attackTimer + dt
    if ai.attackTimer >= ai.config.AttackCooldown then
        ai.attackTimer = 0
        ai:SpawnKnife()
    end
end

return AttackState
