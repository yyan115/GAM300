-- Resources/Scripts/GamePlay/GroundHookedState.lua
local HookedState = {}

local function toDtSec(dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return 0 end
    if dtSec > 0.05 then dtSec = 0.05 end
    return dtSec
end

function HookedState:Enter(ai)
    if ai._animator then
        ai._animator:SetBool("Hooked", true)
    end

    ai._hookedTimer = 0
    ai.attackTimer = 0
    ai._hookPullT = 0

    -- Delay only if this hook was entered due to flying->ground conversion
    if ai._justConvertedFromFlying then
        ai._hookedLandingTimer = tonumber(ai.HookedLandingDelay) or 0
        ai._justConvertedFromFlying = false
    else
        ai._hookedLandingTimer = 0
    end

    if ai.ClearPath then ai:ClearPath() end
    if ai.StopCC then ai:StopCC() end
end


function HookedState:Update(ai, dt)
    local dtSec = toDtSec(dt)
    if dtSec <= 0 then return end

    ai._hookedTimer = (ai._hookedTimer or 0) + dtSec

    -- Still allow death to override
    if ai.health <= 0 then
        ai.dead = true
        return
    end

    -- Wait a short moment before pull starts
    if ai._hookedLandingTimer and ai._hookedLandingTimer > 0 then
        ai._hookedLandingTimer = ai._hookedLandingTimer - dtSec
        if ai._hookedLandingTimer < 0 then ai._hookedLandingTimer = 0 end
        -- keep enemy still during this “impact / stagger” window
        if ai.StopCC then ai:StopCC() end
        return
    end

    -- PULL toward player every frame while hooked
    if ai.PullTowardPlayer then
        ai:PullTowardPlayer(dtSec)
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

    if ai.StopCC then ai:StopCC() end

    ai._hookedLandingTimer = nil
end

return HookedState