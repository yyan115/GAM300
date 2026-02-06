-- Resources/Scripts/GamePlay/GroundPatrolState.lua
local PatrolState = {}

local Physics = _G.Physics

local function atan2(y, x)
    local ok, v = pcall(math.atan, y, x)
    if ok and type(v) == "number" then return v end
    if x > 0 then return math.atan(y / x) end
    if x < 0 and y >= 0 then return math.atan(y / x) + math.pi end
    if x < 0 and y < 0 then return math.atan(y / x) - math.pi end
    if x == 0 and y > 0 then return math.pi / 2 end
    if x == 0 and y < 0 then return -math.pi / 2 end
    return 0
end

local function eulerToQuat(pitch, yaw, roll)
    local p = math.rad(pitch or 0) * 0.5
    local y = math.rad(yaw or 0)   * 0.5
    local r = math.rad(roll or 0)  * 0.5
    local sinP, cosP = math.sin(p), math.cos(p)
    local sinY, cosY = math.sin(y), math.cos(y)
    local sinR, cosR = math.sin(r), math.cos(r)
    return {
        w = cosP * cosY * cosR + sinP * sinY * sinR,
        x = sinP * cosY * cosR - cosP * sinY * sinR,
        y = cosP * sinY * cosR + sinP * cosY * sinR,
        z = cosP * cosY * sinR - sinP * sinY * cosR
    }
end

local function stop(ai)
    if ai.StopCC then ai:StopCC() end
end

local function switchTarget(ai)
    print("[GroundPatrolState] Before switch target, target = ", ai._patrolWhich)
    ai._patrolWhich = (ai._patrolWhich == 1) and 2 or 1
    ai._patrolTarget = (ai._patrolWhich == 1) and ai._patrolA or ai._patrolB
    ai._switchLockT = 0.45
    ai._stuckT = 0
    print("[GroundPatrolState] After switch target, target = ", ai._patrolWhich)
end

function PatrolState:Enter(ai)
    ai._animator:SetBool("PatrolEnabled", true)
    -- print(string.format("[Patrol][Enter] A=%s B=%s T=%s",
    -- tostring(ai._patrolA and (ai._patrolA.x .. "," .. ai._patrolA.z) or "nil"),
    -- tostring(ai._patrolB and (ai._patrolB.x .. "," .. ai._patrolB.z) or "nil"),
    -- tostring(ai._patrolTarget and (ai._patrolTarget.x .. "," .. ai._patrolTarget.z) or "nil")))

    ai._patrolWaitT  = 0
    ai._switchLockT  = 0
    ai._enteredT     = 0

    ai._stuckT       = 0
    ai._lastX        = nil
    ai._lastZ        = nil

    if not ai._patrolWhich then
        ai._patrolWhich = 2
    end
    ai._patrolTarget = (ai._patrolWhich == 1) and ai._patrolA or ai._patrolB

    local x, y, z = ai:GetPosition()

    -- Patrol points must be set by EnemyAI.Start (authoritative).
    -- If they're missing, bail out safely.
    if not ai._patrolA or not ai._patrolB then
        --print("[Patrol] Missing patrol points (_patrolA/_patrolB). Did Start run?")
        ai._patrolWaitT = 0.2
        stop(ai)
        return
    end

    if not ai._patrolTarget then
        ai._patrolTarget = ai._patrolB
    end

    --ai:PlayClip((ai.clips and (ai.clips.Walk or ai.clips.Idle)) or 0, true)

    ai._lastX, ai._lastZ = x, z
    stop(ai)

    -- reset any old path
    if ai.ClearPath then ai:ClearPath() end

    local t = ai._patrolTarget
    if not t then return end

    -- ensure path exists / repath periodically
    local needRepath = (not ai._path)
    -- if (ai._pathStuckT or 0) >= (ai.PathStuckTime or 0.75) then
    --     needRepath = true
    -- end
    if needRepath then
        print("[GroundPatrolState] NEED REPATH.")
        ai._pathRepathT = 0
        ai:RequestPathToXZ(t.x, t.z)
    end
end

function PatrolState:Update(ai, dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return end
    if dtSec > 0.05 then dtSec = 0.05 end

    ai._enteredT    = (ai._enteredT or 0) + dtSec
    ai._switchLockT = math.max(0, (ai._switchLockT or 0) - dtSec)

    -- -- waiting at endpoint
    -- if (ai._patrolWaitT or 0) > 0 then
    --     ai._patrolWaitT = ai._patrolWaitT - dtSec
    --     stop(ai)

    --     -- local r = ai._waitLockRot
    --     -- if r then ai:SetRotation(r.w, r.x, r.y, r.z) end
    --     -- if ai._patrolWaitT <= 0 then ai._waitLockRot = nil end
    --     -- return
    -- end

    local detect = (ai.config and ai.config.DetectionRange) or ai.DetectionRange or 4.0
    local d2 = ai:GetPlayerDistanceSq()

    if d2 <= (detect * detect) then
        stop(ai)
        ai.fsm:Change("Chase", ai.states.Chase)
        return
    end

    local speed = (ai.config and ai.config.PatrolSpeed) or ai.PatrolSpeed or 0.3
    if speed <= 0 then stop(ai); return end

    local t = ai._patrolTarget
    if not t then return end

    local pathEnded = ai:FollowPath(dtSec, speed)

    -- Only treat as arrived if we're actually close to the patrol target (world-space)
    local ex, ez = ai:GetEnemyPosXZ()
    local dx = ex - t.x
    local dz = ez - t.z
    local arrivedSq = dx*dx + dz*dz
    local arriveR = ai.PathArriveRadius or 0.5

    if ai._isPatrolWait == false and pathEnded then
        print("[GroundPatrolState] Arrived at patrol point")
        ai._waitLockRot = ai._lastFacingRot
        ai._patrolWaitT = (ai.config and ai.config.PatrolWait) or ai.PatrolWait or 1.5
        stop(ai)
        ai._isPatrolWait = true
        return
    end

    if ai._isPatrolWait and pathEnded then
        ai._patrolWaitT = ai._patrolWaitT - dtSec
        if ai._patrolWaitT <= 0 then
            print("[GroundPatrolState] Switching patrol targets")
            switchTarget(ai)
            ai._isPatrolWait = false

            t = ai._patrolTarget
            ai:RequestPathToXZ(t.x, t.z)
        end
    end

    -- -- If the path ended but we are NOT near target, it means the path ended early (bad goal snap)
    -- if pathEnded then
    --     ai:ClearPath()
    --     ai:RequestPathToXZ(t.x, t.z) -- retry, don't switch targets
    -- end

    local ex, ez = ai:GetEnemyPosXZ()
    local movedSq = 0
    if ai._lastX ~= nil and ai._lastZ ~= nil then
        local mx = ex - ai._lastX
        local mz = ez - ai._lastZ
        movedSq = mx*mx + mz*mz
    end
    ai._lastX, ai._lastZ = ex, ez

    local movedEpsSq = 1e-6
    if movedSq < movedEpsSq then
        ai._stuckT = (ai._stuckT or 0) + dtSec
    else
        ai._stuckT = 0
    end

    -- if (ai._stuckT or 0) >= 0.75 and (ai._switchLockT or 0) <= 0 then
    --     -- Treat blocked movement as arrival
    --     ai._waitLockRot = ai._lastFacingRot
    --     ai:ClearPath()
    --     switchTarget(ai)
    --     ai._patrolWaitT = (ai.config and ai.config.PatrolWait) or ai.PatrolWait or 1.5
    --     stop(ai)
    --     return
    -- end
end

return PatrolState