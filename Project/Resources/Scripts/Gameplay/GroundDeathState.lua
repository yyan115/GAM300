-- Resources/Scripts/Gameplay/GroundDeathState.lua
local DeathState = {}

function DeathState:Enter(ai)
    ai.dead = true
    ai:PlayClip(ai.clips.Death, false)

    -- common shutdown behavior
    ai:DisableCombat()
end

function DeathState:Update(ai, dt)
    -- Optional: despawn after animation/time
    -- if ai.fsm.timeInState > 2.0 then ai:Despawn() end
end

function DeathState:Exit(ai)
end

return DeathState
