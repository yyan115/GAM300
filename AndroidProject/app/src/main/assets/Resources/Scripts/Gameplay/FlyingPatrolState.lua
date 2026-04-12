-- Resources/Scripts/GamePlay/FlyingPatrolState.lua
local FlyingPatrol = {}

function FlyingPatrol:Enter(ai)
    -- reuse same patrol points that ground uses
    ai._patrolWaitT  = ai.PatrolWait or 1.5
    ai._isPatrolWait = false

    -- decide a target if none
    if not ai._patrolTarget then
        ai._patrolWhich = ai._patrolWhich or 2
        ai._patrolTarget = (ai._patrolWhich == 1) and ai._patrolA or ai._patrolB
    end

    ai._animator:SetBool("Flying", true)
    ai._animator:SetBool("PlayerInDetectionRange", false)
    ai._animator:SetBool("PatrolEnabled", true)
end

function FlyingPatrol:Update(ai, dt)
    if ai.aggressive then
        ai.fsm:Change("Chase", ai.states.Chase)
        return
    end

    -- always keep hover stable in flying states
    ai:MaintainHover(dt)

    -- detect player -> Chase (require line of sight)
    local detR = ai.DetectionRange or 4.0
    if ai:IsPlayerInRange(detR) and ai:HasLineOfSight() then
        ai.fsm:Change("Chase", ai.states.Chase)
        return
    end

    -- if patrol disabled, fall back to idle
    if not ai.EnablePatrol then
        ai.fsm:Change("Idle", ai.states.Idle)
        return
    end

    -- waiting at end point
    if ai._isPatrolWait then
        ai._patrolWaitT = (ai._patrolWaitT or 0) - dt
        if ai._patrolWaitT <= 0 then
            ai._isPatrolWait = false
            ai._animator:SetBool("PatrolEnabled", true)
            ai._patrolWaitT = ai.PatrolWait or 1.5

            -- flip target
            ai._patrolWhich = (ai._patrolWhich == 1) and 2 or 1
            ai._patrolTarget = (ai._patrolWhich == 1) and ai._patrolA or ai._patrolB
        else
            -- stand still while waiting
            ai._animator:SetBool("PatrolEnabled", false)
            return
        end
    end

    local tgt = ai._patrolTarget
    if not tgt then return end

    -- move in XZ only (Y handled by hover)
    local ex, _, ez = ai:GetPosition()
    if ex == nil then return end

    local dx = (tgt.x or ex) - ex
    local dz = (tgt.z or ez) - ez
    local d2 = dx*dx + dz*dz

    local arriveR = 0.5
    if d2 <= (arriveR * arriveR) then
        ai._isPatrolWait = true
        ai._patrolWaitT  = ai.PatrolWait or 1.5
        return
    end

    local spd = ai.FlyingChaseSpeed or ai.PatrolSpeed or 0.6
    local d = math.sqrt(d2)
    local dirX, dirZ = dx / d, dz / d

    -- clamp step (prevents teleporting)
    local step = spd * dt
    local maxStep = 0.20
    if step > maxStep then step = maxStep end

    -- Wall collision: raycast before moving
    if Physics and Physics.Raycast then
        local _, ey, _ = ai:GetPosition()
        local wallMargin = 0.4
        local hitDist = Physics.Raycast(
            ex, ey, ez,
            dirX, 0, dirZ,
            step + wallMargin)
        if hitDist >= 0 and hitDist < (step + wallMargin) then
            step = math.max(0, hitDist - wallMargin)
        end
    end

    if step < 1e-4 then
        ai:FaceDirection(dirX, dirZ)
        return
    end

    local _, y, _ = ai:GetPosition()
    ai:SetPosition(ex + dirX * step, y, ez + dirZ * step)
    ai:FaceDirection(dirX, dirZ)
end

return FlyingPatrol