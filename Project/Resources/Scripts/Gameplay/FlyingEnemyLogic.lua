require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

--CONFIGURATIONS
local ENEMY_NAME = "FlyingEnemy"  -- Change this to match your enemy's name
local FLOOR_NAME = "TempGround"      -- Change this to match your floor's name
local PLAYER_NAME = "Player"      -- Change this to match your player's name

local DETECTION_RANGE = 7.0       -- How close player needs to be to trigger attack
local FALL_SPEED = 0.05           -- How fast the enemy falls
local FLOOR_OFFSET = 1.0          -- Offset above floor to trigger death


-- Animation States
local IDLE          = 0
local PIERCED       = 1
local TAKE_DAMAGE   = 2
local DEATH         = 3     --fall to ground
local FALL          = 4     --free fall
local ATTACK        = 5

local playerNear = false
local currentState = IDLE
local enemyDead = false

local function IsPlayerInRange()

    local tr = Engine.FindTransformByName(PLAYER_NAME)
    local pos = Engine.GetTransformPosition(tr)  -- Get the table

    local player_x = pos[1]  -- First element
    local player_y = pos[2]  -- Second element
    local player_z = pos[3]  -- Third element

    -- Get enemy position
    local Flying_Enemytr = Engine.FindTransformByName(ENEMY_NAME)
    local enemyPos = Engine.GetTransformPosition(Flying_Enemytr)
    local enemy_x = enemyPos[1]
    local enemy_y = enemyPos[2]
    local enemy_z = enemyPos[3]

    -- Calculate distance
    local dx = player_x - enemy_x
    local dy = player_y - enemy_y
    local dz = player_z - enemy_z
    local distance = math.sqrt(dx*dx + dy*dy + dz*dz)
    
    -- Check if player is within range
    return distance < DETECTION_RANGE
end

-- Helper function to create a rotation quaternion from Euler angles (degrees)
local function eulerToQuat(pitch, yaw, roll)
    -- Convert degrees to radians
    local p = math.rad(pitch or 0) * 0.5
    local y = math.rad(yaw or 0) * 0.5
    local r = math.rad(roll or 0) * 0.5
    
    local sinP, cosP = math.sin(p), math.cos(p)
    local sinY, cosY = math.sin(y), math.cos(y)
    local sinR, cosR = math.sin(r), math.cos(r)
    
    return {
        w = cosP * cosY * cosR + sinP * sinY * sinR,
        x = sinP * cosY * cosR - cosP * sinY * sinR,
        y = cosP * sinY * cosR + sinP * cosY * sinR,
        z = cosP * cosY * sinR - sinP * sinY * cosR
    }
end

local function RotateTowardsPlayer(self)
    -- Get player position
    local playerTr = Engine.FindTransformByName(PLAYER_NAME)
    local playerPos = Engine.GetTransformPosition(playerTr)
    local player_x = playerPos[1]
    local player_z = playerPos[3]
    
    -- Get enemy position
    local enemyTr = Engine.FindTransformByName(ENEMY_NAME)
    local enemyPos = Engine.GetTransformPosition(enemyTr)
    local enemy_x = enemyPos[1]
    local enemy_z = enemyPos[3]
    
    -- Calculate direction to player (ignore Y for flat rotation)
    local dx = player_x - enemy_x
    local dz = player_z - enemy_z
    
    -- Calculate angle in degrees (using math.atan instead of math.atan2)
    local angle = math.deg(math.atan(dx, dz))
    
    -- Create rotation quaternion (rotate around Y axis)
    local quat = eulerToQuat(0, angle, 0)
    
    -- Apply rotation
    self:SetRotation(quat.w, quat.x, quat.y, quat.z)
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
        self._animation = self:GetComponent("AnimationComponent") 
        self.collider = self:GetComponent("ColliderComponent")
        self._animation:PlayClip(IDLE, true, self.entityId)
        currentState = IDLE
    end,
    
    Update = function(self, dt) 
        local newState = currentState

        -- Determine new state 
        if self.Health <= 0 then
            newState = FALL
        elseif Input.GetKeyDown(Input.Key.U) then
            TakeDamage(self)
            newState = TAKE_DAMAGE
        elseif IsPlayerInRange() then
            newState = ATTACK
        else
            newState = IDLE
        end
        
-- Apply state transition rules
        if currentState == FALL and newState ~= DEATH then
            newState = FALL  -- Stay in FALL unless transitioning to DEATH
        elseif currentState == DEATH then
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
        
        --HANDLE ROTATION
        if newState == ATTACK or newState == TAKE_DAMAGE and not self.hasRotated then 
            RotateTowardsPlayer(self)
            self.hasRotated = true
        end
        -- Reset rotation flag when leaving ATTACK or TAKE_DAMAGE
        if newState ~= ATTACK and newState ~= TAKE_DAMAGE then
            self.hasRotated = false
        end

        --END OF ROTATION HANDLING



        if currentState == FALL and enemyDead == false then            
            self:Move(0,-FALL_SPEED,0)
            local current_pos = self:GetPosition()
            local Flying_Enemytr = Engine.FindTransformByName(ENEMY_NAME)
            local enemyPos = Engine.GetTransformPosition(Flying_Enemytr)
            local enemy_y = enemyPos[2] --Self YPos

            local FloorTransform = Engine.FindTransformByName(FLOOR_NAME)
            local Floor = Engine.GetTransformPosition(FloorTransform)
            local floorY = Floor[2]

            if enemy_y <= floorY + FLOOR_OFFSET then
                 newState = DEATH
                 enemyDead = true
            end
        end


        
        -- Update animation if state changed
        if newState ~= currentState then
            self._animation:Pause()
            local loop = (newState ~= FALL and newState ~= TAKE_DAMAGE and newState ~= DEATH)
            self._animation:PlayClip(newState, loop, self.entityId)
            currentState = newState
        end


    end
}

--  1 = ATTACK
--  2 = TAKEDAMAGE
--  3 = ENEMYDEATH
