-- Resources/Scripts/GamePlay/GroundHurtState.lua
local HurtState = {}

function HurtState:Enter(ai)
    ai._animator:SetBool("Hurt", true)

    ai:FacePlayer()

    -- Lock out attacks briefly
    ai._hurtTimer = 0

    if ai.particles then
        ai.particles.isEmitting   = true
        ai.particles.emissionRate = 180
    end
end

function HurtState:Update(ai, dt)
    ai._hurtTimer = (ai._hurtTimer or 0) + dt

    -- Wait for hurt "stun" duration, then return to appropriate state
    if ai._hurtTimer >= ai.config.HurtDuration then
        ai._hurtTimer = 0

        if ai.health <= 0 then
            self.dead = true
            return
        end

        -- If player is still near, resume Attack. Otherwise go Idle.
        if ai:IsPlayerInRange(ai.config.DetectionRange) then
            if not ai.IsPassive then 
                ai.fsm:Change("Attack", ai.states.Attack)
            else
                ai.fsm:Change("Idle", ai.states.Idle)
            end
        else
            ai.fsm:Change("Idle", ai.states.Idle)
        end
    end
end

function HurtState:Exit(ai)
    ai._animator:SetBool("Hurt", false)
    if ai.particles then
        ai.particles.isEmitting   = false
        ai.particles.emissionRate = 0
    end
end

return HurtState
