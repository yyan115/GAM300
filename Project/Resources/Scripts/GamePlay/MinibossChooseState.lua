<<<<<<< HEAD:Project/Resources/Scripts/Gameplay/MinibossChooseState.lua
-- Resources/Scripts/Gameplay/MinibossChooseState.lua
=======
-- Resources/Scripts/GamePlay/MinibossChooseState.lua
>>>>>>> main:Project/Resources/Scripts/GamePlay/MinibossChooseState.lua
local ChooseState = {}

function ChooseState:Enter(ai)
    ai.currentMove = nil
    ai.currentMoveDef = nil
end

function ChooseState:Update(ai, dt)
    local name, def, weight, total, roll, phase = ai:ChooseMove()

    if not name then
        -- Nothing available -> wait until something comes off cooldown
        local wait = ai:GetNextMoveReadyTime()
        -- also respect your baseline RecoverDuration (prevents instant chains)
        wait = math.max(wait, ai.RecoverDuration or 0.6)

        -- IMPORTANT: don't spam this log every frame
        ai._noMoveLogT = (ai._noMoveLogT or 0) + (dt or 0)
        if ai._noMoveLogT >= 0.5 then
            ai._noMoveLogT = 0
            print(string.format("[Miniboss][Pick] phase=%d NO AVAILABLE MOVES -> recover %.2fs", ai:GetPhase(), wait))
        end

        ai._recoverTimer = wait
        ai.fsm:Change("Recover", ai.states.Recover)
        return
    end

    ai._noMoveLogT = 0

    -- Save pick info for Execute + logging
    ai.currentMove = name
    ai.currentMoveDef = def

    ai._dbgPick = {
        phase = phase,
        weight = weight,
        total = total,
        roll = roll
    }

    ai.fsm:Change("Execute", ai.states.Execute)
end

return ChooseState
