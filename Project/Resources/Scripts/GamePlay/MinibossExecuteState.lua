<<<<<<< HEAD:Project/Resources/Scripts/Gameplay/MinibossExecuteState.lua
-- Resources/Scripts/Gameplay/MinibossExecuteState.lua
=======
-- Resources/Scripts/GamePlay/MinibossExecuteState.lua
>>>>>>> main:Project/Resources/Scripts/GamePlay/MinibossExecuteState.lua
local ExecuteState = {}

function ExecuteState:Enter(ai)
    if ai.FacePlayer then
        ai:FacePlayer()
    end

    local name = ai.currentMove
    local def  = ai.currentMoveDef
    if not name or not def then
        ai.fsm:Change("Recover", ai.states.Recover)
        return
    end

    -- Debug print using the stored pick info
    local d = ai._dbgPick or {}
    print(string.format(
        "[Miniboss][Pick] phase=%d move=%s weight=%d total=%d roll=%.2f",
        d.phase or ai:GetPhase(),
        tostring(name),
        d.weight or -1,
        d.total or -1,
        d.roll or -1
    ))

    -- Face player before executing (if you want)
    if ai.FacePlayer then ai:FacePlayer() end

    -- Execute move
    def.execute(ai)

    -- Start cooldown
    ai:StartMoveCooldown(name, def.cooldown or 1.0)

    -- Go to recover after executing
    ai._recoverTimer = ai.RecoverDuration or 0.6
    ai.fsm:Change("Recover", ai.states.Recover)
end

function ExecuteState:Update(ai, dt)
    if ai.FacePlayer then
        ai:FacePlayer()
    end
end

function ExecuteState:Exit(ai)
    ai.currentMove = nil
    ai.currentMoveDef = nil
end

return ExecuteState