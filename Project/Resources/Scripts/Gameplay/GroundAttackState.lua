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
        if ai.aggressive then
            ai.fsm:Change("Chase", ai.states.Chase)
        else
            ai.fsm:Change("Patrol", ai.states.Patrol)
        end
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

    if ai.IsMelee then
        ai._animator:SetBool("Melee", true)
    else
        ai._animator:SetBool("Ranged", true)
    end

    ai._skipFirstCooldown = not ai._hasAttackedBefore

    if ai.IsMelee and ai._skipFirstCooldown then
        ai.attackTimer = ai.FirstMeleeAttackHeadStart or 0.20
    else
        -- [FIX] Force it to be ready to swing immediately upon re-entering range!
        ai.attackTimer = 999
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
                if d2 > (diseng * diseng) then
                    if ai.aggressive then
                        ai.fsm:Change("Chase", ai.states.Chase)
                    else
                        ai.fsm:Change("Patrol", ai.states.Patrol)
                    end
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

    if ai.IsMelee then
        if (not ai._attackCommitted) and (not ai.meleeAnimTriggered) and d2 > (diseng * diseng) then
            ai._animator:SetBool("ReadyToAttack", false)
            ai._readyLatched = false
            if ai.aggressive then
                ai.fsm:Change("Chase", ai.states.Chase)
            else
                ai.fsm:Change("Patrol", ai.states.Patrol)
            end
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
        
        local impactWait = ai.MeleeAnimDelay or 0.5
        local cd = ai.MeleeAttackCooldown or (ai.config.AttackCooldown or 1.0)

        -- PHASE 1: IMMEDIATELY TRIGGER ANIMATION (Start the Wind-up)
        -- We swing IF the animation isn't playing AND (it's the first hit OR cooldown has finished)
        if not ai.meleeAnimTriggered and (ai._skipFirstCooldown or ai.attackTimer >= cd) then
            if not ai:IsAttackWindowValid(ai._currentAttackToken) then
                interruptOut(ai)
                return
            end

            -- Range check before committing to the swing
            local d2check1 = ai:GetPlayerDistanceSq()
            local _, meleeRcheck1, diseng1 = ai:GetRanges()
            
            if d2check1 > (meleeRcheck1 * meleeRcheck1) then
                if d2check1 > (diseng1 * diseng1) then
                    ai.fsm:Change("Patrol", ai.states.Patrol)
                else
                    ai.fsm:Change("Chase", ai.states.Chase)
                end
                return
            end

            -- START THE ATTACK: Reset the timer to 0 to track this specific swing!
            ai.attackTimer = 0
            ai._skipFirstCooldown = false
            ai.meleeAnimTriggered = true
            ai._damageDealt = false
            ai._attackCommitted = true

            ai._animator:SetBool("Ranged", false)
            ai._animator:SetBool("Melee", true)
        end

        -- PHASE 2: IMPACT -> DEAL DAMAGE
        -- Triggers exactly when the timer reaches the wind-up duration
        if ai.meleeAnimTriggered and not ai._damageDealt and ai.attackTimer >= impactWait then
            if not ai:IsAttackWindowValid(ai._currentAttackToken) then
                interruptOut(ai)
                return
            end

            ai._damageDealt = true
            
            local d2check2 = ai:GetPlayerDistanceSq()
            local _, meleeRcheck2, _ = ai:GetRanges()

            -- Publish attack SFX & Visuals
            ai:_publishSFX("meleeAttack")
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

            -- Only deal damage if the player didn't dodge out of range during the wind-up
            if d2check2 <= (meleeRcheck2 * meleeRcheck2) then
                ai:_publishSFX("meleeHit")
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
        end

        -- PHASE 3: REPEAT
        -- Once the total cooldown time has elapsed, reset flags so Phase 1 can trigger again
        if ai._damageDealt and ai.attackTimer >= cd then
            ai.meleeAnimTriggered = false
            ai._attackCommitted = false
            
            ai._animator:SetBool("Melee", false)
            ai._animator:SetBool("ReadyToAttack", false)

            -- Note: We DO NOT reset ai.attackTimer to 0 here. 
            -- We leave it >= cd so Phase 1 instantly sees it is ready to swing again!
        end
    -- Ranged
    else
        ai.attackTimer = (ai.attackTimer or 0) + dtSec

        local spawnFeatherDelay = ai.RangedAnimDelay or 0.5
        local cd = ai.AttackCooldown

        if not ai.rangedAnimTriggered and (ai._skipFirstCooldown or ai.attackTimer >= cd) then
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
                
                -- START THE ATTACK: Reset the timer to 0 to track this specific swing!
                ai.rangedAnimTriggered = true
                ai.attackTimer = 0
                ai._skipFirstCooldown = false
                ai._damageDealt = false
                ai._attackCommitted = true

            elseif d2 > (diseng * diseng) then
                if ai.aggressive then
                    ai.fsm:Change("Chase", ai.states.Chase)
                else
                    ai.fsm:Change("Patrol", ai.states.Patrol)
                end
                return
            else
                ai.fsm:Change("Chase", ai.states.Chase)
                return
            end
        end

        if ai.rangedAnimTriggered and not ai._damageDealt and ai.attackTimer >= spawnFeatherDelay then
            -- final guard before projectile actually spawns
            if not ai:IsAttackWindowValid(ai._currentAttackToken) then
                interruptOut(ai)
                return
            end

            ai._damageDealt = true

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

        -- PHASE 3: REPEAT
        -- Once the total cooldown time has elapsed, reset flags so Phase 1 can trigger again
        if ai._damageDealt and ai.attackTimer >= cd then
            ai.rangedAnimTriggered = false
            ai._attackCommitted = false
            
            ai._animator:SetBool("Ranged", false)
            ai._animator:SetBool("ReadyToAttack", false)

            -- Note: We DO NOT reset ai.attackTimer to 0 here. 
            -- We leave it >= cd so Phase 1 instantly sees it is ready to swing again!
        end
    end
end

function AttackState:Exit(ai)
    stopCC(ai)
    ai._currentAttackToken = nil
    ai._attackCancelled = false
    ai._attackCancelReason = nil

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