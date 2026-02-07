-- Resources/Scripts/Gameplay/MinibossBattlecryState.lua
local BattlecryState = {}

function BattlecryState:Enter(ai)
    ai._introTimer = 0
    ai._inIntro = true
    ai:LockActions("INTRO", ai.IntroDuration or 5.0) -- blocks combat states

    -- face player + play battlecry anim/audio
    ai:FacePlayer()
    ai._animator:SetTrigger("Taunt")

    -- Play taunt SFX (force full volume: temporarily disable 3D rolloff
    -- so the dramatic battlecry is always clearly audible at aggro range)
    local count = ai.enemyTauntSFX and #ai.enemyTauntSFX or 0
    if count > 0 and ai._audio then
        local savedBlend = ai._audio.SpatialBlend
        ai._audio.SpatialBlend = 0          -- play as 2D (bypasses distance rolloff)
        ai._audio:PlayOneShot(ai.enemyTauntSFX[math.random(1, count)])
        ai._audio.SpatialBlend = savedBlend  -- restore 3D for combat SFX
    end

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