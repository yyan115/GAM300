-- Resources/Scripts/GamePlay/GroundHookedState.lua
local HookedState = {}

function HookedState:Enter(ai)
    ai._animator:SetBool("Hooked", true)
    ai._hookedTimer = 0

    -- Optional: stop attacks immediately
    ai.attackTimer = 0
end

function HookedState:Update(ai, dt)
    ai._hookedTimer = (ai._hookedTimer or 0) + dt

    -- Still allow death to override
    if ai.health <= 0 then
        self.dead = true
        return
    end

    if ai._hookedTimer >= ai.config.HookedDuration then
        ai._hookedTimer = 0
        
        if ai:IsPlayerInRange(ai.config.DetectionRange) then
            self._animator:SetBool("PlayerInRange", true)
            ai.fsm:Change("Attack", ai.states.Attack)
        else
            self._animator:SetBool("PlayerInRange", false)
            ai.fsm:Change("Idle", ai.states.Idle)
        end
    end
end

function HookedState:Exit(ai) 
    ai._animator:SetBool("Hooked", false)
end

return HookedState
