-- Resources/Scripts/GamePlay/FlyingAttackState.lua
local FlyingAttack = {}

function FlyingAttack:Enter(ai)
    ai._attackT = 0
    ai._attackWindupT = 0
    ai._didAttackThisCycle = false

    -- if you want to reuse the same cooldown variable:
    ai._attackCooldownT = ai._attackCooldownT or 0

    ai._animator:SetBool("Flying", true)
    ai._animator:SetBool("PlayerInAttackRange", true)
    ai._animator:SetBool("PlayerInDetectionRange", false)
    ai._animator:SetBool("PatrolEnabled", false)
end

function FlyingAttack:Update(ai, dt)
    ai:MaintainHover(dt)
    ai:FacePlayer()

    local attackR, meleeR, diseng = ai:GetRanges()

    -- if player left disengage range -> chase
    if not ai:IsPlayerInRange(diseng) then
        ai.fsm:Change("Chase", ai.states.Chase)
        return
    end

    -- cooldown ticking
    ai._attackCooldownT = math.max(0, (ai._attackCooldownT or 0) - dt)

    -- if not in attack range yet, inch closer (keeps it from hovering uselessly)
    if not ai:IsPlayerInRange(attackR) then
        ai:MoveTowardPlayerXZ_Flying(dt, ai.FlyingChaseSpeed or 1.2)
        return
    end

    -- ready to attack?
    if (ai._attackCooldownT or 0) > 0 then
        return
    end

    -- windup (sync with animation)
    local windup = tonumber(ai.RangedAnimDelay) or 0
    ai._attackWindupT = (ai._attackWindupT or 0) + dt

    if ai._attackWindupT < windup then
        return
    end

    -- do attack once per cycle
    if ai._didAttackThisCycle then
        return
    end
    ai._didAttackThisCycle = true

    -- Ranged: throw knives
    ai._animator:SetBool("Ranged", true)
    local ok = ai:SpawnKnife()
    if ok and ai.PlayAttackSFX then
        -- SpawnKnife already plays ranged SFX in your EnemyAI,
        -- but leaving this harmless if you change later.
        -- ai:PlayAttackSFX()
    end

    -- restart cooldown
    ai._attackCooldownT = tonumber(ai.AttackCooldown) or 3.0

    -- reset windup for next cycle
    ai._attackWindupT = 0
    ai._didAttackThisCycle = false
end

function FlyingAttack:Exit(ai)
    ai._animator:SetBool("PlayerInAttackRange", false)
    ai._animator:SetBool("Ranged", false)
end

return FlyingAttack