-- Resources/Scripts/GamePlay/GroundChaseState.lua
local ChaseState = {}

function ChaseState:Enter(ai)
    -- Optional: force first repath on enter
    print("[GroundChaseState] ENTER")
    ai._pathRepathT = (ai.PathRepathInterval or 0.45)
end

function ChaseState:Update(ai, dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return end
    if dtSec > 0.05 then dtSec = 0.05 end

    local chaseSpd = ai.config.ChaseSpeed or 1.8
    local attackR, meleeR, diseng = ai:GetRanges()
    local d2 = ai:GetPlayerDistanceSq()

    -- Too far -> Patrol
    if d2 > (diseng * diseng) then
        ai:StopCC()
        ai.fsm:Change("Patrol", ai.states.Patrol)
        return
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
            print("[Chase] NO PATH to player - stopping movement")
            ai:StopCC()
            return
        end
    end

    -- Follow the path
    local arrived = ai:FollowPath(dtSec, chaseSpd)

    -- If we "arrived" but still not in attack range (can happen if path ends early), do a soft fallback
    if arrived then
        ai:StopCC()
        -- optional: face player for aiming
        ai:FacePlayer()
    end
end

return ChaseState
