-- Resources/Scripts/Gameplay/FlyingDeathState.lua
-- Death state for flying enemies: falls to the ground first, then despawns.

local FlyingDeathState = {}

local FALL_ACCEL   = 15.0   -- gravity-like acceleration (units/sec²)
local MAX_FALL_VEL = 20.0   -- terminal velocity cap

local function toDtSec(dt)
    local dtSec = tonumber(dt) or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return 0 end
    if dtSec > 0.05 then dtSec = 0.05 end
    return dtSec
end

function FlyingDeathState:Enter(ai)
    ai._animator:SetBool("Dead", true)
    if ai.DisableCombat then ai:DisableCombat() end

    -- Disable char-vs-char collision so corpse doesn't block anything
    pcall(function() CharacterController.DisableCollision(ai.entityId) end)

    -- Make rigidbody kinematic so we control the fall ourselves
    if ai._rb then
        pcall(function()
            ai._rb.motionID = 1       -- Kinematic
            ai._rb.motion_dirty = true
            ai._rb.linearVel = { x = 0, y = 0, z = 0 }
        end)
    end

    ai._deathTimer    = 0
    ai._deathLifetime = tonumber(ai.deathLifetime) or 8.0
    ai._fallVel       = 0       -- current downward speed
    ai._grounded      = false   -- true once the enemy reaches ground

    -- Spawn feathers on death (same as GroundDeathState)
    for i = 1, ai.NumFeathersSpawnedPerHit do
        ai:SpawnFeather(i)
    end
end

function FlyingDeathState:Update(ai, dt)
    local dtSec = toDtSec(dt)
    if dtSec == 0 then return end

    -- Fall until grounded
    if not ai._grounded then
        ai._fallVel = math.min(ai._fallVel + FALL_ACCEL * dtSec, MAX_FALL_VEL)

        local x, y, z = ai:GetPosition()
        if x then
            local newY = y - ai._fallVel * dtSec

            -- Check ground height
            local groundY = y - 100  -- fallback: far below
            if Nav and Nav.GetGroundY then
                local g = Nav.GetGroundY(ai.entityId)
                if g then groundY = g end
            end

            if newY <= groundY then
                newY = groundY
                ai._grounded = true
            end

            ai:SetPosition(x, newY, z)
        end
    end

    -- Despawn timer (runs whether falling or grounded)
    ai._deathTimer    = (tonumber(ai._deathTimer) or 0) + dtSec
    ai._deathLifetime = tonumber(ai._deathLifetime) or (tonumber(ai.deathLifetime) or 8.0)

    if ai._deathTimer >= ai._deathLifetime then
        if not ai._despawned and not ai._softDespawned then
            if ai.Despawn then
                ai:Despawn()
            elseif ai.SoftDespawn then
                ai:SoftDespawn()
            elseif ai.OnDestroy then
                ai:OnDestroy()
            end
        end
    end
end

function FlyingDeathState:Exit(ai)
    ai._animator:SetBool("Dead", false)
end

return FlyingDeathState
