-- Resources/Scripts/GamePlay/GroundHookedState.lua
local HookedState = {}

local function toDtSec(dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return 0 end
    if dtSec > 0.05 then dtSec = 0.05 end
    return dtSec
end

local function reachedPlayer(ai)
    local tr = ai._playerTr
    if not tr then
        tr = Engine.FindTransformByName(ai.PlayerName or "Player")
        ai._playerTr = tr
    end
    if not tr then return false end

    local pp = Engine.GetTransformPosition(tr)
    if not pp then return false end
    local px, pz = pp[1], pp[3]

    local ex, ez = ai:GetEnemyPosXZ()
    if ex == nil or ez == nil then return false end

    local dx, dz = px - ex, pz - ez
    local d2 = dx*dx + dz*dz

    local stopR = tonumber(ai.HookStopDistance) or 1.2
    return d2 <= (stopR * stopR)
end

function HookedState:Enter(ai)
    if ai._animator then
        ai._animator:SetBool("Hooked", true)
    end

    ai._hookedTimer = 0
    ai.attackTimer = 0

    ai._hookReachedPlayer = false

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
        if ai.StopCC then ai:StopCC() end
        return
    end

    if not ai._hookReachedPlayer then
        if reachedPlayer(ai) then
            ai._hookReachedPlayer = true
            if ai.StopCC then ai:StopCC() end
        else
            -- PULL toward player every frame while hooked
            if ai.PullTowardPlayer then
                ai:PullTowardPlayer(dtSec)
            end
        end
    else
        -- keep still (prevents micro-movement / run-in-place)
        if ai.StopCC then ai:StopCC() end
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
    ai._hookReachedPlayer = nil
end

return HookedState