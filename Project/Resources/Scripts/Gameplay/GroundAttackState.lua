-- Resources/Scripts/GamePlay/GroundAttackState.lua
local AttackState = {}

local function stopCC(ai)
    if ai.MoveCC then
        ai:MoveCC(0, 0)
    end
end

local function interruptOut(ai)
    stopCC(ai)

    ai._animator:SetBool("ReadyToAttack", false)
    ai._animator:SetBool("Melee", false)
    ai._animator:SetBool("Ranged", false)

    ai.meleeAnimTriggered  = false
    ai.rangedAnimTriggered = false
    ai._attackCommitted    = false
    ai._attackCommitTimer  = 0
    ai._attackRecovering   = false
    ai._attackRecoveryT    = 0
    ai._readyLatched       = false
    ai._readySettleT       = 0

    local attackR, meleeR, diseng = ai:GetRanges()
    local d2 = ai:GetPlayerDistanceSq()

    if d2 > (diseng * diseng) then
        ai.fsm:Change("Patrol", ai.states.Patrol)
        return
    end

    if ai.IsMelee then
        if d2 > (meleeR * meleeR) then
            ai.fsm:Change("Chase", ai.states.Chase)
            return
        end
    else
        if d2 > (attackR * attackR) then
            ai.fsm:Change("Chase", ai.states.Chase)
            return
        end
    end

    ai.fsm:Change("Hurt", ai.states.Hurt)
end

function AttackState:Enter(ai)
    ai._currentAttackToken = ai:BeginAttackWindow()

    ai._animator:SetBool("PlayerInAttackRange", true)
    ai._animator:SetBool("PlayerInDetectionRange", false)
    ai._animator:SetBool("PatrolEnabled", false)
    ai._animator:SetBool("ReadyToAttack", false)

    -- Clear both attack mode bools first so we do not carry stale mode
    ai._animator:SetBool("Melee", false)
    ai._animator:SetBool("Ranged", false)

    ai._skipFirstCooldown = not ai._hasAttackedBefore

    if ai.IsMelee and ai._skipFirstCooldown then
        ai.attackTimer = ai.FirstMeleeAttackHeadStart or 0.20
    else
        ai.attackTimer = 0
    end

    ai.meleeAnimTriggered  = false
    ai.rangedAnimTriggered = false

    ai._readySettleT = 0
    ai._readyLatched = false

    ai._attackCommitted   = false
    ai._attackCommitTimer = 0

    ai._attackRecovering  = false
    ai._attackRecoveryT   = 0
end

function AttackState:Update(ai, dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return end
    if dtSec > 0.05 then dtSec = 0.05 end

    ai:FacePlayer()
    stopCC(ai)

    -- interrupted by hurt / hook / knockup / invalid token
    if not ai:IsAttackWindowValid(ai._currentAttackToken) then
        interruptOut(ai)
        return
    end

    -- Recovery phase
    if ai._attackRecovering then
        ai._attackRecoveryT = ai._attackRecoveryT + dtSec
        local recoveryDur = ai.MeleeRecoveryDuration
            or ai.MeleeAttackCooldown
            or (ai.config.AttackCooldown or 1.0)

        if ai._attackRecoveryT >= recoveryDur then
            ai._attackRecovering = false
            ai._attackRecoveryT  = 0

            local attackR, meleeR, diseng = ai:GetRanges()
            local d2 = ai:GetPlayerDistanceSq()

            if ai.IsMelee then
                if d2 > (meleeR * meleeR) then
                    ai.fsm:Change("Chase", ai.states.Chase)
                    return
                end
            else
                if d2 > (attackR * attackR) then
                    ai.fsm:Change("Chase", ai.states.Chase)
                    return
                end
            end
        end

        return
    end

    if ai._attackCommitted then
        ai._attackCommitTimer = ai._attackCommitTimer + dtSec
    end

    local attackR, meleeR, diseng = ai:GetRanges()
    local d2 = ai:GetPlayerDistanceSq()

    -- Only allow state exit if not committed AND anim hasn't triggered yet
    if ai.IsMelee then
        if (not ai._attackCommitted) and (not ai.meleeAnimTriggered) and d2 > (meleeR * meleeR) then
            ai._animator:SetBool("ReadyToAttack", false)
            ai._readyLatched = false
            ai.fsm:Change("Chase", ai.states.Chase)
            return
        end
    else
        if (not ai._attackCommitted) and (not ai.rangedAnimTriggered) and d2 > (attackR * attackR) then
            ai._animator:SetBool("ReadyToAttack", false)
            ai._readyLatched = false
            ai.fsm:Change("Chase", ai.states.Chase)
            return
        end
    end

    -- Ready-to-attack settle
    local settleDelay = ai.ReadyToAttackDelay or 0.15
    ai._readySettleT = (ai._readySettleT or 0) + dtSec

    if (not ai._readyLatched) and ai._readySettleT >= settleDelay then
        ai._readyLatched = true
        ai._animator:SetBool("ReadyToAttack", true)
    end

    -- Melee
    if ai.IsMelee then
        ai.attackTimer = (ai.attackTimer or 0) + dtSec
        local cd = ai.MeleeAttackCooldown or (ai.config.AttackCooldown or 1.0)

        if not ai.meleeAnimTriggered and (ai._skipFirstCooldown or ai.attackTimer >= ai.MeleeAnimDelay) then
            -- interrupted during windup? do not proceed
            if not ai:IsAttackWindowValid(ai._currentAttackToken) then
                interruptOut(ai)
                return
            end

            if not ai._attackCommitted then
                ai._attackCommitted   = true
                ai._attackCommitTimer = 0
            end

            if d2 <= (meleeR * meleeR) then
                ai._animator:SetBool("Ranged", false)
                ai._animator:SetBool("Melee", true)
                ai.meleeAnimTriggered = true

                if _G.event_bus and _G.event_bus.publish then
                    local ex, _, ez = ai:GetPosition()
                    _G.event_bus.publish("melee_incoming", {
                        dmg           = (ai.MeleeDamage or 1),
                        src           = "GroundEnemy",
                        enemyEntityId = ai.entityId,
                        x             = ex or 0,
                        z             = ez or 0,
                    })
                end

                if _G.event_bus then
                    local x, y, z = ai:GetPosition()
                    local qW, qX, qY, qZ = ai:GetRotation()
                    _G.event_bus.publish("onClawSlashTrigger", {
                        pos      = { x = x,  y = y,  z = z  },
                        rot      = { w = qW, x = qX, y = qY, z = qZ },
                        entityId = ai.entityId,
                        variant  = "NORMAL",
                        claimed  = false,
                    })
                end
            elseif d2 > (diseng * diseng) then
                ai.fsm:Change("Patrol", ai.states.Patrol)
                return
            else
                ai.fsm:Change("Chase", ai.states.Chase)
                return
            end
        end

        if ai.attackTimer >= cd then
            -- final guard before actual damage lands
            if not ai:IsAttackWindowValid(ai._currentAttackToken) then
                interruptOut(ai)
                return
            end

            ai.attackTimer = 0
            ai._hasAttackedBefore = true
            ai._skipFirstCooldown = false

            local d2check = ai:GetPlayerDistanceSq()
            local _, meleeRcheck, _ = ai:GetRanges()
            if d2check <= (meleeRcheck * meleeRcheck) then
                ai:_publishSFX(ai.IsMelee and "meleeAttack" or "rangedAttack")
                ai:_publishSFX(ai.IsMelee and "meleeHit"    or "rangedHit")

                if _G.event_bus and _G.event_bus.publish then
                    local ex, _, ez = ai:GetPosition()
                    _G.event_bus.publish("meleeHitPlayerDmg", {
                        dmg           = (ai.MeleeDamage or 1),
                        src           = "GroundEnemy",
                        enemyEntityId = ai.entityId,
                        x             = ex or 0,
                        z             = ez or 0,
                    })
                end
            end

            ai.meleeAnimTriggered = false
            ai._attackCommitted   = false
            ai._attackCommitTimer = 0
            ai._animator:SetBool("Melee", false)
            ai._animator:SetBool("ReadyToAttack", false)
            ai._readyLatched      = false
            ai._readySettleT      = 0

            ai._attackRecovering  = true
            ai._attackRecoveryT   = 0
        end

    -- Ranged
    else
        ai.attackTimer = (ai.attackTimer or 0) + dtSec

        if not ai.rangedAnimTriggered and ai.attackTimer >= ai.RangedAnimDelay then
            -- interrupted during windup? do not proceed
            if not ai:IsAttackWindowValid(ai._currentAttackToken) then
                interruptOut(ai)
                return
            end

            if not ai._attackCommitted then
                ai._attackCommitted   = true
                ai._attackCommitTimer = 0
            end

            if d2 <= (attackR * attackR) then
                -- IMPORTANT: keep PlayerInAttackRange true for ranged too
                ai._animator:SetBool("Melee", false)
                ai._animator:SetBool("Ranged", true)
                ai.rangedAnimTriggered = true

            elseif d2 > (diseng * diseng) then
                ai.fsm:Change("Patrol", ai.states.Patrol)
                return
            else
                ai.fsm:Change("Chase", ai.states.Chase)
                return
            end
        end

        if ai.attackTimer >= (ai.config.AttackCooldown or 3.0) then
            -- final guard before projectile actually spawns
            if not ai:IsAttackWindowValid(ai._currentAttackToken) then
                interruptOut(ai)
                return
            end

            local ok = ai:SpawnKnife()
            if ok then
                ai.attackTimer         = 0
                ai.rangedAnimTriggered = false
                ai._hasAttackedBefore  = true
                ai._readySettleT       = 0
                ai._readyLatched       = false
                ai._animator:SetBool("ReadyToAttack", false)
                ai._animator:SetBool("Ranged", false)
                ai._attackCommitted    = false
                ai._attackCommitTimer  = 0
            else
                ai.attackTimer = (ai.config.AttackCooldown or 3.0)
            end
        end
    end
end

function AttackState:Exit(ai)
    stopCC(ai)
    ai:CancelPendingAttack("EXIT_ATTACK")

    ai._animator:SetBool("PlayerInAttackRange", false)
    ai._animator:SetBool("ReadyToAttack", false)

    -- clear both attack modes on exit
    ai._animator:SetBool("Melee", false)
    ai._animator:SetBool("Ranged", false)

    ai.meleeAnimTriggered  = false
    ai.rangedAnimTriggered = false

    ai._attackCommitted   = false
    ai._attackCommitTimer = 0
    ai._attackRecovering  = false
    ai._attackRecoveryT   = 0
end

return AttackState