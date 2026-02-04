-- Resources/Scripts/Gameplay/MinibossBattlecryState.lua
local BattlecryState = {}

function BattlecryState:Enter(ai)
    ai._introTimer = 0
    ai._inIntro = true
    ai:LockActions("INTRO", ai.IntroDuration or 5.0) -- blocks combat states, but we’ll manually exit

    -- Optional: face player + play battlecry anim/audio
    ai:FacePlayer()
    if ai.PlayClip and ai.ClipBattlecry and ai.ClipBattlecry >= 0 then
        ai:PlayClip(ai.ClipBattlecry, false)
    end

    print("[Miniboss] Intro START (battlecry)")
end

function BattlecryState:Update(ai, dt)
    ai._introTimer = (ai._introTimer or 0) + (dt or 0)

    -- keep facing player during intro (optional)
    ai:FacePlayer()

    if ai._introTimer >= (ai.IntroDuration or 5.0) then
        ai._inIntro = false
        ai:UnlockActions()
        ai._recoverTimer = math.max(ai.RecoverDuration or 0.6, 0.35)
        print("[Miniboss] Intro END -> Combat")
        ai.fsm:Change("Recover", ai.states.Recover) -- small pacing before first move
    end
end

function BattlecryState:Exit(ai)
    -- in case we ever leave early
    ai._inIntro = false
end

return BattlecryState