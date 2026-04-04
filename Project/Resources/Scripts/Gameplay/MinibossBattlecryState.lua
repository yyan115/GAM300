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
    local _bossAudio = GetComponent(ai.entityId, "AudioComponent")
    if _bossAudio then _bossAudio.SpatialBlend = 0 end  -- play as 2D (bypasses distance rolloff)
    ai:_publishSFX("taunt")

    --print("[Miniboss] Intro START (battlecry)")
end

function BattlecryState:Update(ai, dt)
    ai._introTimer = (ai._introTimer or 0) + (dt or 0)

    -- keep facing player during intro
    ai:FacePlayer()

    if ai._introTimer >= (ai.IntroDuration or 5.0) then
        ai._inIntro = false
        ai:UnlockActions()

        -- NEW SYSTEM: just use a timer, no FSM combat states
        ai._postIntroRecoverT = math.max(ai.RecoverDuration or 0.6, 0.35)

        --print("[Miniboss] Intro END -> Combat (new system)")

        local _bossAudio = GetComponent(ai.entityId, "AudioComponent")
        if _bossAudio then _bossAudio.SpatialBlend = 1 end
    end
end

function BattlecryState:Exit(ai)
    -- in case we ever leave early
    ai._inIntro = false
end

return BattlecryState