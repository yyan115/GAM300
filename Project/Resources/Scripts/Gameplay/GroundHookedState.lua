-- Resources/Scripts/GamePlay/GroundHookedState.lua
local HookedState = {}

function HookedState:Enter(ai)
    if ai._animator then
        ai._animator:SetBool("Hooked", true)
    end

    ai._hookedTimer = 0
    ai.attackTimer = 0

    -- While hooked, don't follow nav / patrol leftovers
    if ai.ClearPath then ai:ClearPath() end
    if ai.StopCC then ai:StopCC() end
end

function HookedState:Update(ai, dt)
    ai._hookedTimer = (ai._hookedTimer or 0) + (dt or 0)

    -- Still allow death to override
    if ai.health <= 0 then
        ai.dead = true
        return
    end

    -- PULL toward player every frame while hooked
    if ai.PullTowardPlayer then
        ai:PullTowardPlayer(dt)
    end

    -- Exit hooked after duration
    if ai._hookedTimer >= (ai.config.HookedDuration or 0) then
        ai._hookedTimer = 0

        if ai:IsPlayerInRange(ai.config.DetectionRange) then
            if ai._animator then ai._animator:SetBool("PlayerInRange", true) end
            ai.fsm:Change("Attack", ai.states.Attack)
        else
            if ai._animator then ai._animator:SetBool("PlayerInRange", false) end
            ai.fsm:Change("Idle", ai.states.Idle)
        end
    end
end

function HookedState:Exit(ai)
    if ai._animator then
        ai._animator:SetBool("Hooked", false)
    end

    -- stop any tiny residual motion when hook ends
    if ai.StopCC then ai:StopCC() end
end

return HookedState
