require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

-- Animation States
local IDLE          = 0
local ATTACK        = 1
local TAKE_DAMAGE   = 2
local DEATH         = 3

local playerNear = false
local currentState = IDLE

-- Helper Functions
local function IsPlayerInRange()

    local tr = Engine.FindTransformByName("Player")
    local pos = Engine.GetTransformPosition(tr)  -- Get the table

    local player_x = pos[1]  -- First element
    local player_y = pos[2]  -- Second element
    local player_z = pos[3]  -- Third element

    print("Player pos is ", player_x, player_y, player_z)

    -- Get enemy position
    local Ground_Enemytr = Engine.FindTransformByName("GroundEnemy")
    local enemyPos = Engine.GetTransformPosition(Ground_Enemytr)
    local enemy_x = enemyPos[1]
    local enemy_y = enemyPos[2]
    local enemy_z = enemyPos[3]
    
    print("enemyPos is ", enemy_x, enemy_y, enemy_z)


    -- Calculate distance
    local dx = player_x - enemy_x
    local dy = player_y - enemy_y
    local dz = player_z - enemy_z
    local distance = math.sqrt(dx*dx + dy*dy + dz*dz)
    
    -- Check if player is within range
    local detectionRange = 4.0  -- Adjust this value as needed

    print("distance is ", distance)
    return distance < detectionRange


    -- if Input.GetKeyDown(Input.Key.U) then
    --     playerNear = true
    -- end
    -- return playerNear
end

local function TakeDamage(self)
    self.Health = self.Health - 1
end

return Component {
    mixins = { TransformMixin },
    
    fields = {
        Health          = 5,
        Damage          = 1,
        Attack_Speed    = 1.0,
        Attack_Cooldown = 1.0
    },
    
    Start = function(self) 
        self.animation = self:GetComponent("AnimationComponent") 
        self.collider = self:GetComponent("ColliderComponent")
        Animation.PlayClip(self.animation, IDLE, true)
        currentState = IDLE
    end,
    
    Update = function(self, dt) 
        local newState = currentState

        -- Determine new state 
        if self.Health <= 0 then
            newState = DEATH
        elseif Input.GetKeyDown(Input.Key.U) then
            TakeDamage(self)
            newState = TAKE_DAMAGE
        elseif IsPlayerInRange() then
            newState = ATTACK
        else
            newState = IDLE
        end
        
        -- Apply state transition rules
        if currentState == DEATH then
            newState = DEATH  -- Death cannot be interrupted
        elseif currentState == TAKE_DAMAGE then
            if Animation.IsPlaying(self.animation) then
                newState = TAKE_DAMAGE  -- Keep playing damage animation
            else
                newState = ATTACK  -- After damage, go to attack
            end
        elseif currentState == ATTACK and newState == IDLE then
            newState = ATTACK  -- Attack cannot be interrupted by idle
        end
        
        -- Update animation if state changed
        if newState ~= currentState then
            Animation.Pause(self.animation)
            local loop = (newState ~= DEATH and newState ~= TAKE_DAMAGE)
            Animation.PlayClip(self.animation, newState, loop)
            currentState = newState
        end
    end
}

--  1 = ATTACK
--  2 = TAKEDAMAGE
--  3 = ENEMYDEATH

-- Animation.PlayOnce(self.animation, AttackAnimation)      -- plays a clip once
-- Animation.Pause(self.animation)                           -- pauses playback
-- Animation.Stop(self.animation)                            -- stops and resets playback
-- Animation.SetSpeed(self.animation, 1.5)                  -- sets playback speed
-- Animation.SetLooping(self.animation, true)               -- sets looping
-- local playing = Animation.IsPlaying(self.animation)      -- returns true/false
