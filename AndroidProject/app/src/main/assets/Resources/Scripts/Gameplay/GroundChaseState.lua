-- Resources/Scripts/GamePlay/GroundChaseState.lua
local ChaseState = {}

function ChaseState:Enter(ai)
    -- Optional: force first repath on enter
    --print("[GroundChaseState] ENTER")
    ai:ClearPath()
    ai:StopCC()
    ai._pathRepathT = 999

    ai._animator:SetBool("PlayerInDetectionRange", true)
    ai._animator:SetBool("PatrolEnabled", true)
    ai._footstepTimer = 0
    ai._chaseLosLostT = 0

    -- Play alert SFX once when first detecting player (entering chase)
    ai:_publishSFX("alert")
end

function ChaseState:Update(ai, dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return end
    if dtSec > 0.05 then dtSec = 0.05 end

    local chaseSpd = ai.config.ChaseSpeed or 1.8
    local attackR, meleeR, diseng = ai:GetRanges()
    local d2 = ai:GetPlayerDistanceSq()

    -- Too far -> disengage (clear aggressive so they don't chase forever)
    if d2 > (diseng * diseng) then
        ai.aggressive = false
        ai:StopCC()
        if ai.config.EnablePatrol then
            ai.fsm:Change("Patrol", ai.states.Patrol)
        else
            ai.fsm:Change("Idle", ai.states.Idle)
        end
        return
    end

    -- Lose chase if LOS blocked for sustained period (wall between enemy and player)
    local losGrace = tonumber(ai.LOSGracePeriod) or 3.0
    if not ai:HasLineOfSight() then
        ai._chaseLosLostT = (ai._chaseLosLostT or 0) + dtSec
        if ai._chaseLosLostT >= losGrace then
            ai.aggressive = false
            ai._chaseLosLostT = 0
            ai:StopCC()
            if ai.config.EnablePatrol then
                ai.fsm:Change("Patrol", ai.states.Patrol)
            else
                ai.fsm:Change("Idle", ai.states.Idle)
            end
            return
        end
    else
        ai._chaseLosLostT = 0
    end

    if ai.IsMelee then
        if d2 < (meleeR * meleeR) then
            ai:StopCC()
            if not ai.IsPassive then
                ai.fsm:Change("Attack", ai.states.Attack)
            else
                ai.fsm:Change("Idle", ai.states.Idle)
            end
            return
        end
    else
        if d2 <= (attackR * attackR) then
            ai:StopCC()
            if not ai.IsPassive then
                ai.fsm:Change("Attack", ai.states.Attack)
            else
                ai.fsm:Change("Idle", ai.states.Idle)
            end
            return
        end
    end

    local tr = ai._playerTr
    if not tr then return end
    local pp = Engine.GetTransformPosition(tr)
    local px, pz = pp[1], pp[3]

    -- Repath conditions:
    -- 1) timed interval
    -- 2) player moved enough
    -- 3) stuck while following path
    local needRepath = ai:ShouldRepathToXZ(px, pz, dtSec)
    if (ai._pathStuckT or 0) >= (ai.PathStuckTime or 0.75) then
        needRepath = true
    end

    if needRepath then
        ai._pathRepathT = 0
        local pathFound = ai:RequestPathToXZ(px, pz)
        
        if not pathFound then
            --print("[Chase] NO PATH to player - stopping movement")
            ai:StopCC()
            return
        end
    end

    -- Follow the path
    local arrived = ai:FollowPath(dtSec, chaseSpd)

    -- If we "arrived" but still not in attack range (can happen if path ends early), do a soft fallback
    if arrived then
        ai:StopCC()
        ai._footstepTimer = 0
        -- optional: face player for aiming
        ai:FacePlayer()
    else
        ai._footstepTimer = (ai._footstepTimer or 0) + dtSec
        if ai._footstepTimer >= (ai.ChaseFootstepInterval or 0.35) then
            ai._footstepTimer = 0
            ai:_publishSFX("footstep")
        end
    end
end

return ChaseState
