-- Resources/Scripts/GamePlay/GroundHurtState.lua
local HurtState = {}

function HurtState:Enter(ai)
    print("[GroundHurtState] ENTER")

    -- Randomly choose one of the Hurt animations to play
    local random = math.random(1, 3)
    if random == 1 then
        ai._animator:SetBool("Hurt1", true)
    elseif random == 2 then
        ai._animator:SetBool("Hurt2", true)
    elseif random == 3 then
        ai._animator:SetBool("Hurt3", true)
    end

    ai:FacePlayer()

    -- Lock out attacks briefly
    ai._hurtTimer = 0

    if ai.particles then
        ai.particles.isEmitting   = true
        ai.particles.emissionRate = 100
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
    print("[GroundHurtState] EXIT")
    ai._animator:SetBool("Hurt1", false)
    ai._animator:SetBool("Hurt2", false)
    ai._animator:SetBool("Hurt3", false)
    if ai.particles then
        ai.particles.isEmitting   = false
        ai.particles.emissionRate = 0
    end
end

return HurtState
