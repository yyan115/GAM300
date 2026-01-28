-- Resources/Scripts/Gameplay/MinibossRecoverState.lua
local RecoverState = {}

function RecoverState:Enter(ai)
    ai._recoverT = ai._recoverTimer or ai.RecoverDuration or 0.6
end

function RecoverState:Update(ai, dt)
    if ai.FacePlayer then
        ai:FacePlayer()
    end

    ai._recoverTimer = ai._recoverTimer - dt
    if ai._recoverTimer <= 0 then
        ai.fsm:Change("Choose", ai.states.Choose)
    end
end

return RecoverState
