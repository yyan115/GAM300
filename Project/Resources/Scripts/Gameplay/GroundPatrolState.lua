-- Resources/Scripts/Gameplay/GroundPatrolState.lua
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
    ai._patrolTarget = (ai._patrolTarget == ai._patrolA) and ai._patrolB or ai._patrolA
    ai._switchLockT = 0.45 -- cannot switch again during this window
    ai._stuckT = 0
end

local function clamp01(x)
    if x < 0 then return 0 end
    if x > 1 then return 1 end
    return x
end

function PatrolState:Enter(ai)
    ai._patrolWaitT  = 0
    ai._switchLockT  = 0
    ai._enteredT     = 0

    ai._stuckT       = 0
    ai._lastX        = nil
    ai._lastZ        = nil

    local x, y, z = ai:GetPosition()

    if not ai._patrolA or not ai._patrolB then
        local dist = ai.config.PatrolDistance or 4.0
        ai._patrolA = { x = x - dist, y = y, z = z }
        ai._patrolB = { x = x + dist, y = y, z = z }
    end

    if not ai._patrolTarget then
        ai._patrolTarget = ai._patrolB
    end

    ai:PlayClip(ai.clips.Walk or ai.clips.Idle, true)

    ai._lastX, ai._lastZ = x, z
    stop(ai)
end

function PatrolState:Update(ai, dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return end
    if dtSec > 0.05 then dtSec = 0.05 end

    ai._enteredT    = (ai._enteredT or 0) + dtSec
    ai._switchLockT = math.max(0, (ai._switchLockT or 0) - dtSec)

    if (ai._patrolWaitT or 0) > 0 then
        ai._patrolWaitT = ai._patrolWaitT - dtSec
        stop(ai)

        -- Force rotation to remain stable during wait
        local r = ai._waitLockRot
        if r then
            ai:SetRotation(r.w, r.x, r.y, r.z)
        end

        -- When wait ends, clear lock so patrol can rotate normally again
        if ai._patrolWaitT <= 0 then
            ai._waitLockRot = nil
        end

        return
    end

    local attackR, diseng = ai:GetRanges()
    local d2 = ai:GetPlayerDistanceSq()

    -- if within detection window -> Chase
    if d2 <= (diseng * diseng) and d2 > (attackR * attackR) then
        stop(ai)
        ai.fsm:Change("Chase", ai.states.Chase)
        return
    end

    local speed = ai.config.PatrolSpeed or 1.0
    if speed <= 0 then
        stop(ai)
        return
    end

    local x, y, z = ai:GetPosition()
    local t = ai._patrolTarget
    if not t then return end

    local dx = t.x - x
    local dz = t.z - z
    local distSq = dx*dx + dz*dz

    -- arrival
    local arriveRadius = 0.25
    if distSq <= (arriveRadius * arriveRadius) and (ai._switchLockT or 0) <= 0 then
        -- Lock current facing so waiting never changes orientation
        ai._waitLockRot = ai._lastFacingRot  -- from EnemyAI:ApplyRotation()

        switchTarget(ai)
        ai._patrolWaitT = ai.config.PatrolWait or 0.5
        stop(ai)
        return
    end

    local dist = math.sqrt(distSq)
    if dist < 1e-6 then
        stop(ai)
        return
    end

    local dirX = dx / dist
    local dirZ = dz / dist

    local finalSpeedScale = 1.0

    if (ai._enteredT or 0) > 0.20 then
        -- === WALL PROBE + OFFSET (2-ray) ===
        if Physics then
            local wallRayUp  = ai.WallRayUp or 0.8
            local wallSkin   = ai.WallSkin  or 0.18
            local wallOff    = ai.WallOffset or 0.08
            local minStep    = ai.MinStep or 0.01

            local intendedStep = speed * dtSec
            local probeDist = intendedStep + wallSkin

            -- Two rays: low + high
            local rayY_low  = y + (wallRayUp * 0.35)   -- near knees
            local rayY_high = y + wallRayUp            -- chest

            local hitLow  = Physics.Raycast(x, rayY_low,  z, dirX, 0, dirZ, probeDist)
            local hitHigh = Physics.Raycast(x, rayY_high, z, dirX, 0, dirZ, probeDist)

            local lowOK  = (hitLow  and hitLow  >= 0 and hitLow  <= probeDist)
            local highOK = (hitHigh and hitHigh >= 0 and hitHigh <= probeDist)

            -- Treat as WALL only if BOTH rays hit.
            if lowOK and highOK then
                local hit = math.min(hitLow, hitHigh)
                local allowedStep = hit - wallOff

                if allowedStep <= minStep then
                    stop(ai)
                    ai._stuckT = (ai._stuckT or 0) + dtSec
                    return
                end

                finalSpeedScale = clamp01(allowedStep / (intendedStep > 1e-6 and intendedStep or 1e-6))
            end
        end
    end

    -- Move with reduced speed if near wall
    if ai.MoveCC then
        ai:MoveCC(dirX * speed * finalSpeedScale, dirZ * speed * finalSpeedScale)
    end

    if distSq > 1e-4 and ai.FaceDirection then
        ai:FaceDirection(dirX, dirZ)
    end

    -- === STUCK DETECTION (CC-based, stable) ===
    -- Arm after a short time so it doesn't flip on spawn jitter.
    if (ai._enteredT or 0) < 0.50 then
        ai._lastX, ai._lastZ = x, z
        ai._stuckT = 0
        return
    end

    -- Only consider stuck if we're not near the endpoint.
    if distSq <= (arriveRadius * arriveRadius * 4.0) then
        ai._lastX, ai._lastZ = x, z
        ai._stuckT = 0
        return
    end

    local movedSq = 0
    if ai._lastX ~= nil and ai._lastZ ~= nil then
        local mx = x - ai._lastX
        local mz = z - ai._lastZ
        movedSq = mx*mx + mz*mz
    end
    ai._lastX, ai._lastZ = x, z

    -- If controller isn't letting us move, flip after a while.
    local movedEpsSq = 1e-6
    if movedSq < movedEpsSq then
        ai._stuckT = (ai._stuckT or 0) + dtSec
    else
        ai._stuckT = 0
    end

    if (ai._stuckT or 0) >= 0.75 and (ai._switchLockT or 0) <= 0 then
        switchTarget(ai)
        ai._patrolWaitT = 0.12
        stop(ai)
        return
    end
end

return PatrolState