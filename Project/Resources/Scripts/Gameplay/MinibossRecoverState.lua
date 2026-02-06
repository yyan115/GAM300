-- Resources/Scripts/Gameplay/MinibossRecoverState.lua
local RecoverState = {}

function RecoverState:Enter(ai)
    -- normalize to one timer variable
    ai._recoverTimer = ai._recoverTimer or ai.RecoverDuration or 0.6
end

function RecoverState:Update(ai, dt)
    dt = dt or 0

    ai:FacePlayer()

    ai._recoverTimer = (ai._recoverTimer or 0) - dt
    if ai._recoverTimer <= 0 then
        ai.fsm:Change("Choose", ai.states.Choose)
    end
end

return RecoverState