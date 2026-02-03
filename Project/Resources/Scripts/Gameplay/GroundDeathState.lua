-- Resources/Scripts/Gameplay/GroundDeathState.lua
local DeathState = {}

local function toDtSec(dt)
    local dtSec = tonumber(dt) or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return 0 end
    if dtSec > 0.05 then dtSec = 0.05 end
    return dtSec
end

function DeathState:Enter(ai)
    ai.dead = true
    if ai.PlayClip and ai.clips and ai.clips.Death then
        ai:PlayClip(ai.clips.Death, false)
    end
    if ai.DisableCombat then ai:DisableCombat() end

    -- init always
    ai._deathTimer = 0
    ai._deathLifetime = tonumber(ai.deathLifetime) or 8.0
end

function DeathState:Update(ai, dt)
    local dtSec = toDtSec(dt)
    if dtSec == 0 then return end

    -- HARD GUARDS: never let nil reach comparisons
    ai._deathTimer = (tonumber(ai._deathTimer) or 0) + dtSec
    ai._deathLifetime = tonumber(ai._deathLifetime) or (tonumber(ai.deathLifetime) or 8.0)

    if ai._deathTimer >= ai._deathLifetime then
        if not ai._despawned then
            ai._despawned = true
            print("[DeathState] Despawn trigger entity=", tostring(ai.entityId))

            if ai.SoftDespawn then
                ai:SoftDespawn()
            elseif ai.OnDestroy then
                ai:OnDestroy() -- cleanup only
            end
        end
    end
end

function DeathState:Exit(ai) end
return DeathState