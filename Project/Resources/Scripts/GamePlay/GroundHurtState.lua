-- Resources/Scripts/GamePlay/GroundHurtState.lua
local HurtState = {}

function HurtState:Enter(ai)
    -- Play hurt clip once (non-loop)
    ai:PlayClip(ai.clips.Hurt, false)

    ai:FacePlayer()

    -- Lock out attacks briefly
    ai._hurtTimer = 0
end

function HurtState:Update(ai, dt)
    ai._hurtTimer = (ai._hurtTimer or 0) + dt

    -- Wait for hurt "stun" duration, then return to appropriate state
    if ai._hurtTimer >= ai.config.HurtDuration then
        ai._hurtTimer = 0

        if ai.health <= 0 then
            ai.fsm:Change("Death", ai.states.Death)
            return
        end

        -- If player is still near, resume Attack. Otherwise go Idle.
        if ai:IsPlayerInRange(ai.config.DetectionRange) then
            ai.fsm:Change("Attack", ai.states.Attack)
        else
            ai.fsm:Change("Idle", ai.states.Idle)
        end
    end
end

function HurtState:Exit(ai)
    -- nothing
end

return HurtState
