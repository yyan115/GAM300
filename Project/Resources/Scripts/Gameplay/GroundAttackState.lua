-- Resources/Scripts/GamePlay/GroundAttackState.lua
local AttackState = {}

local function stopCC(ai)
    if ai.MoveCC then
        ai:MoveCC(0, 0)
    end
end

function AttackState:Enter(ai)
    ai._animator:SetBool("PlayerInAttackRange", true)
    ai._animator:SetBool("PlayerInDetectionRange", false)
    ai._animator:SetBool("PatrolEnabled", false)
    ai._animator:SetBool("ReadyToAttack", false)

    local cd = ai.IsMelee
        and (ai.MeleeAttackCooldown or (ai.config.AttackCooldown or 1.0))
        or  (ai.config.AttackCooldown or 3.0)

    ai._skipFirstCooldown = true

    if ai.IsMelee and ai._skipFirstCooldown then
        ai.attackTimer = ai.FirstMeleeAttackHeadStart or 0.20
    else
        ai.attackTimer = 0
    end

    ai.meleeAnimTriggered = false
    ai.rangedAnimTriggered = false

    ai._readySettleT = 0
    ai._readyLatched = false
end

function AttackState:Update(ai, dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return end
    if dtSec > 0.05 then dtSec = 0.05 end

    ai:FacePlayer()
    stopCC(ai)

    local attackR, meleeR, diseng = ai:GetRanges()
    local d2 = ai:GetPlayerDistanceSq()

    -- If target moved away, leave attack and also clear ready
    if ai.IsMelee then
        if d2 > (meleeR * meleeR) then
            ai._animator:SetBool("ReadyToAttack", false)
            ai._readyLatched = false
            ai.fsm:Change("Chase", ai.states.Chase)
            return
        end
    else
        if d2 > (attackR * attackR) then
            ai._animator:SetBool("ReadyToAttack", false)
            ai._readyLatched = false
            ai.fsm:Change("Chase", ai.states.Chase)
            return
        end
    end

    local settleDelay = ai.ReadyToAttackDelay or 0.15
    ai._readySettleT = (ai._readySettleT or 0) + dtSec

    if (not ai._readyLatched) and ai._readySettleT >= settleDelay then
        ai._readyLatched = true
        ai._animator:SetBool("ReadyToAttack", true)
    end

    if ai.IsMelee then
        ai.attackTimer = (ai.attackTimer or 0) + dtSec
        local cd = ai.MeleeAttackCooldown or (ai.config.AttackCooldown or 1.0)

        if not ai.meleeAnimTriggered and (ai._skipFirstCooldown or ai.attackTimer >= ai.MeleeAnimDelay) then
            if d2 <= (meleeR * meleeR) then
                ai._animator:SetBool("Melee", true)
                ai.meleeAnimTriggered = true
                if _G.event_bus and _G.event_bus.publish then
                    local ex, _, ez = ai:GetPosition()
                    _G.event_bus.publish("melee_incoming", {
                        dmg           = (ai.MeleeDamage or 1),
                        src           = "GroundEnemy",
                        enemyEntityId = ai.entityId,
                        x             = ex or 0,   -- attacker XZ for dodge/knockback direction
                        z             = ez or 0,
                    })
                end

            -- TRIGGER CLAW VFX HERE
            if _G.event_bus then
                local x, y, z = ai:GetPosition()
                local qW, qX, qY, qZ = ai:GetRotation() -- Get the AI's current facing
                
                _G.event_bus.publish("onClawSlashTrigger", {
                    pos = {x = x, y = y, z = z},
                    rot = {w = qW, x = qX, y = qY, z = qZ},
                    entityId = ai.entityId,
                    variant = "NORMAL",
                    claimed = false
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
            ai.attackTimer = 0
            ai._hasAttackedBefore = true
            ai._skipFirstCooldown = false

            if ai.PlayAttackSFX then ai:PlayAttackSFX() end
            if ai.PlayHitSFX then ai:PlayHitSFX() end

            if _G.event_bus and _G.event_bus.publish then
                local ex, _, ez = ai:GetPosition()
                _G.event_bus.publish("meleeHitPlayerDmg", {
                    dmg           = (ai.MeleeDamage or 1),
                    src           = "GroundEnemy",
                    enemyEntityId = ai.entityId,
                    x             = ex or 0,   -- attacker XZ for knockback direction in PlayerHealth
                    z             = ez or 0,
                })
            end
        end
    else
        -- RANGED
        ai.attackTimer = (ai.attackTimer or 0) + dtSec

        if not ai.rangedAnimTriggered and ai.attackTimer >= ai.RangedAnimDelay then
            if d2 <= (attackR * attackR) then
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
            local ok = ai:SpawnKnife()
            if ok then
                ai.attackTimer = 0
                ai.rangedAnimTriggered = false
                ai._hasAttackedBefore = true
                ai._readySettleT = 0
                ai._readyLatched = false
                ai._animator:SetBool("ReadyToAttack", false)
                ai._animator:SetBool("Ranged", false)
            else
                ai.attackTimer = (ai.config.AttackCooldown or 3.0)
            end
        end
    end
end

function AttackState:Exit(ai)
    ai._animator:SetBool("PlayerInAttackRange", false)
    ai._animator:SetBool("Ranged", false)
    ai._animator:SetBool("Melee", false)
    ai._animator:SetBool("ReadyToAttack", false)

    ai.meleeAnimTriggered = false
    ai.rangedAnimTriggered = false
    ai._readySettleT = 0
    ai._readyLatched = false

    stopCC(ai)
end

return AttackState