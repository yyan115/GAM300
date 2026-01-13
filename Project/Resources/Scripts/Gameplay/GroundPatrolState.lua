-- Resources/Scripts/Gameplay/GroundPatrolState.lua
local PatrolState = {}

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

local function faceDir(ai, dirX, dirZ)
    local yaw = math.deg(atan2(dirX, dirZ))
    local q = eulerToQuat(0, yaw, 0)
    ai:SetRotation(q.w, q.x, q.y, q.z)
end

-- === impulse command buffer (Lua-writable or not) ===
local function tryAddImpulseXZ(ai, ix, iz)
    local rb = ai._rb
    if not rb then return false end
    local ok = pcall(function()
        rb.impulseApplied = { x = ix or 0, y = 0, z = iz or 0 }
    end)
    return ok
end

local function stopPushing(ai)
    local rb = ai._rb
    if not rb then return end
    pcall(function()
        rb.impulseApplied = { x = 0, y = 0, z = 0 }
    end)
    -- fallback stop (if your engine uses velocity commands)
    if ai.SetMoveVelocityXZ then
        ai:SetMoveVelocityXZ(0, 0)
    end
end

function PatrolState:Enter(ai)
    print("[FSM] Enter Patrol")

    ai._patrolWaitT     = 0
    ai._patrolSwitchCd  = 0
    ai._minMoveTime     = 0
    ai._enteredT        = 0

    ai._stuckT = 0
    ai._lastDistSq = nil

    -- for “did we actually move?” check
    local x, y, z = ai:GetPosition()
    ai._lastX, ai._lastZ = x, z

    if not ai._patrolA or not ai._patrolB then
        local dist = ai.config.PatrolDistance or 4.0
        ai._patrolA = { x = x - dist, y = y, z = z }
        ai._patrolB = { x = x + dist, y = y, z = z }
    end

    if not ai._patrolTarget then
        ai._patrolTarget = ai._patrolB
    end

    ai:PlayClip(ai.clips.Walk or ai.clips.Idle, true)

    -- IMPORTANT: print ok in a logger-friendly way
    local ok = tryAddImpulseXZ(ai, 0, 0)
    print("[Patrol] impulseApplied writable? ", tostring(ok))
end

function PatrolState:Update(ai, dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return end
    if dtSec > 0.05 then dtSec = 0.05 end

    ai._enteredT = (ai._enteredT or 0) + dtSec

    ai._patrolSwitchCd = math.max(0, (ai._patrolSwitchCd or 0) - dtSec)
    ai._minMoveTime    = math.max(0, (ai._minMoveTime or 0) - dtSec)

    -- endpoint wait
    if (ai._patrolWaitT or 0) > 0 then
        ai._patrolWaitT = ai._patrolWaitT - dtSec
        stopPushing(ai)
        return
    end

    -- attack transition
    if ai:IsPlayerInRange(ai.config.DetectionRange) then
        stopPushing(ai)
        ai.fsm:Change("Attack", ai.states.Attack)
        return
    end

    local speed = ai.config.PatrolSpeed or 1.0
    if speed <= 0 then
        stopPushing(ai)
        return
    end

    local x, y, z = ai:GetPosition()
    local t = ai._patrolTarget
    if not t then return end

    local dx = t.x - x
    local dz = t.z - z
    local distSq = dx*dx + dz*dz

    -- Arrive
    local arriveRadius = 0.25
    local arriveSq = arriveRadius * arriveRadius
    if distSq <= arriveSq and ai._patrolSwitchCd <= 0 and ai._minMoveTime <= 0 then
        ai._patrolTarget = (ai._patrolTarget == ai._patrolA) and ai._patrolB or ai._patrolA
        ai._patrolWaitT = ai.config.PatrolWait or 0.5
        ai._patrolSwitchCd = 0.35
        ai._minMoveTime = 0.20
        stopPushing(ai)
        return
    end

    local dist = math.sqrt(distSq)
    if dist < 1e-6 then
        stopPushing(ai)
        return
    end

    local dirX = dx / dist
    local dirZ = dz / dist

    -- ============================
    -- MOVE: IMPULSE (NO dt factor!)
    -- ============================
    -- Tune these:
    -- - impulseStrength: how hard it pushes each tick
    -- - maxImpulse: clamp to avoid crazy acceleration
    local impulseStrength = 2.5     -- try 1.0 .. 6.0
    local maxImpulse      = 8.0

    local ix = dirX * impulseStrength * speed
    local iz = dirZ * impulseStrength * speed

    -- clamp
    if ix >  maxImpulse then ix =  maxImpulse end
    if ix < -maxImpulse then ix = -maxImpulse end
    if iz >  maxImpulse then iz =  maxImpulse end
    if iz < -maxImpulse then iz = -maxImpulse end

    local ok = tryAddImpulseXZ(ai, ix, iz)
    if not ok and ai.SetMoveVelocityXZ then
        -- fallback (if impulseApplied isn't writable)
        ai:SetMoveVelocityXZ(dirX * speed, dirZ * speed)
    end

    -- ============================
    -- STUCK DETECTION (less eager)
    -- ============================
    -- Don’t allow “stuck flipping” in the first ~0.5s of entering patrol
    local stuckArmingTime = 0.50
    if (ai._enteredT or 0) < stuckArmingTime then
        ai._lastDistSq = distSq
        ai._lastX, ai._lastZ = x, z
    else
        local progressEps = 0.002      -- bigger tolerance (physics jitter)
        local stuckTimeToFlip = 0.60   -- longer so we don’t ping-pong

        local makingProgress = true
        if ai._lastDistSq ~= nil then
            makingProgress = distSq < (ai._lastDistSq - progressEps)
        end

        -- also check actual translation
        local movedSq = 0
        if ai._lastX ~= nil and ai._lastZ ~= nil then
            movedSq = (x - ai._lastX)*(x - ai._lastX) + (z - ai._lastZ)*(z - ai._lastZ)
        end

        if (not makingProgress) or (movedSq < 1e-7) then
            ai._stuckT = (ai._stuckT or 0) + dtSec
        else
            ai._stuckT = 0
        end

        ai._lastDistSq = distSq
        ai._lastX, ai._lastZ = x, z

        if (ai._stuckT or 0) >= stuckTimeToFlip and ai._patrolSwitchCd <= 0 and ai._minMoveTime <= 0 then
            ai._stuckT = 0
            ai._patrolTarget = (ai._patrolTarget == ai._patrolA) and ai._patrolB or ai._patrolA
            ai._patrolWaitT = 0.15
            ai._patrolSwitchCd = 0.35
            ai._minMoveTime = 0.20
            stopPushing(ai)
            return
        end
    end

    -- Face movement
    if distSq > 1e-4 then
        if ai.FaceDirection then
            ai:FaceDirection(dirX, dirZ)
        else
            faceDir(ai, dirX, dirZ)
        end
    end
end

return PatrolState
