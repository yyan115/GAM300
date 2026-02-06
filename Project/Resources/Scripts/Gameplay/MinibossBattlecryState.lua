-- Resources/Scripts/Gameplay/MinibossBattlecryState.lua
local BattlecryState = {}

function BattlecryState:Enter(ai)
    ai._introTimer = 0
    ai._inIntro = true
    ai:LockActions("INTRO", ai.IntroDuration or 5.0) -- blocks combat states

    -- face player + play battlecry anim/audio
    ai:FacePlayer()
    ai._animator:SetTrigger("Taunt")

    print("[Miniboss] Intro START (battlecry)")
end

function BattlecryState:Update(ai, dt)
    ai._introTimer = (ai._introTimer or 0) + (dt or 0)

    -- keep facing player during intro
    ai:FacePlayer()

    if ai._introTimer >= (ai.IntroDuration or 5.0) then
        ai._inIntro = false
        ai:UnlockActions()
        ai._recoverTimer = math.max(ai.RecoverDuration or 0.6, 0.35)
        print("[Miniboss] Intro END -> Combat")
        ai.fsm:Change("Recover", ai.states.Recover) -- small recovery before first move
    end
end

function BattlecryState:Exit(ai)
    -- in case we ever leave early
    ai._inIntro = false
end

return BattlecryState