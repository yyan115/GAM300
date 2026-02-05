-- Resources/Scripts/GamePlay/GroundAttackState.lua
local AttackState = {}

local function stopCC(ai)
    -- so calling Move(0,0) once per frame is okay.
    if ai.MoveCC then
        ai:MoveCC(0, 0)
    end
end

function AttackState:Enter(ai)
    ai._animator:SetBool("PlayerInRange", true)
    if not ai.IsMelee then
        ai._animator:SetBool("Ranged", true)
    -- else
    --     ai._animator:SetBool("Melee", true)
    end
    ai.attackTimer = 0
    ai.meleeAnimTriggered = false
end

function AttackState:Update(ai, dt)
    local dtSec = dt or 0
    if dtSec > 1.0 then dtSec = dtSec * 0.001 end
    if dtSec <= 0 then return end
    if dtSec > 0.05 then dtSec = 0.05 end

    ai:FacePlayer()

    local attackR, meleeR, diseng = ai:GetRanges()
    local d2 = ai:GetPlayerDistanceSq()

    if d2 > (diseng * diseng) then
        if ai.MoveCC then ai:MoveCC(0, 0) end
        ai.fsm:Change("Patrol", ai.states.Patrol)
        return
    end

    if ai.IsMelee then
        if d2 > (meleeR * meleeR) then
            if ai.MoveCC then ai:MoveCC(0, 0) end
            ai.fsm:Change("Chase", ai.states.Chase)
            return
        end
    else
        if d2 >= (attackR * attackR) then
            if ai.MoveCC then ai:MoveCC(0, 0) end
            ai.fsm:Change("Chase", ai.states.Chase)
            return
        end
    end

    if ai.MoveCC then ai:MoveCC(0, 0) end

    if ai.IsMelee then
        ai.attackTimer = (ai.attackTimer or 0) + dtSec
        local cd = ai.MeleeAttackCooldown or (ai.config.AttackCooldown or 1.0)
        if not ai.meleeAnimTriggered and ai.attackTimer >= ai.MeleeAnimDelay then
            ai._animator:SetBool("Melee", true)
            MeleeAnimTriggered = true
        end

        if ai.attackTimer >= cd then
            ai.attackTimer = 0

            print(string.format("Melee Attack!"))
            -- Play melee attack SFX
            if ai.PlayAttackSFX then ai:PlayAttackSFX() end
            -- Play melee hit SFX when attack lands on player
            if ai.PlayHitSFX then ai:PlayHitSFX() end
            --ai._animator:SetBool("Melee", true)
            -- MELEE HIT: emit event (keeps consistent with your event-bus approach)
            if _G.event_bus and _G.event_bus.publish then
                _G.event_bus.publish("meleeHitPlayerDmg", {
                    dmg = (ai.MeleeDamage or 1),
                    src = "GroundEnemy",
                    enemyEntityId = ai.entityId,
                })
            end
        end
    else
        -- RANGED
        ai.attackTimer = (ai.attackTimer or 0) + dtSec
        if ai.attackTimer >= (ai.config.AttackCooldown or 3.0) then
            local ok = ai:SpawnKnife()
            if ok then
                ai.attackTimer = 0
            else
                ai.attackTimer = (ai.config.AttackCooldown or 3.0)
            end
        end
    end
end

function AttackState:Exit(ai)
    ai._animator:SetBool("PlayerInRange", false)
    ai._animator:SetBool("Ranged", false)
    ai._animator:SetBool("Melee", false)
    ai.meleeAnimTriggered = false
    stopCC(ai)
end

return AttackState