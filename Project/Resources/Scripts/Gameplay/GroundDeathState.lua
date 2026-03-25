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
    ai._animator:SetBool("Dead", true)
    if ai.DisableCombat then ai:DisableCombat() end

    -- Remove from charVsChar so corpse can't be pushed — controller stays alive
    pcall(function() CharacterController.DisableCollision(ai.entityId) end)

    -- init always
    ai._deathTimer = 0
    ai._deathLifetime = tonumber(ai.deathLifetime) or 8.0

    for i = 1, ai.NumFeathersSpawnedPerHit do
        ai:SpawnFeather(i)
    end
end

function DeathState:Update(ai, dt)
    local dtSec = toDtSec(dt)
    if dtSec == 0 then return end

    -- HARD GUARDS: never let nil reach comparisons
    ai._deathTimer = (tonumber(ai._deathTimer) or 0) + dtSec
    ai._deathLifetime = tonumber(ai._deathLifetime) or (tonumber(ai.deathLifetime) or 8.0)

    if ai._deathTimer >= ai._deathLifetime then
        if not ai._despawned and not ai._softDespawned then
            print("[DeathState] Despawn trigger entity=", tostring(ai.entityId))

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

function DeathState:Exit(ai) 
    ai._animator:SetBool("Dead", false)
end

return DeathState