require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

--CONFIGURATIONS
local ENEMY_NAME = "GroundEnemy"  -- Change this to match your enemy's name
local FLOOR_NAME = "TopWall"      -- Change this to match your floor's name
local PLAYER_NAME = "Player"      -- Change this to match your player's name

-- Animation States
local IDLE          = 0
local ATTACK        = 1
local TAKE_DAMAGE   = 2
local DEATH         = 3

local playerNear = false
local currentState = IDLE


--HACK FUNCTION FOR NOW
local function SimulateAttackFromPlayer()
    local tr = Engine.FindTransformByName(PLAYER_NAME)
    local pos = Engine.GetTransformPosition(tr)  -- Get the table

    local player_x = pos[1]  -- First element
    local player_y = pos[2]  -- Second element
    local player_z = pos[3]  -- Third element

    -- Get enemy position
    local Ground_Enemytr = Engine.FindTransformByName(ENEMY_NAME)
    local enemyPos = Engine.GetTransformPosition(Ground_Enemytr)
    local enemy_x = enemyPos[1]
    local enemy_y = enemyPos[2]
    local enemy_z = enemyPos[3]

    -- Calculate distance
    local dx = player_x - enemy_x
    local dy = player_y - enemy_y
    local dz = player_z - enemy_z
    local distance = math.sqrt(dx*dx + dy*dy + dz*dz)
    
    -- Check if player is within range
    local getDamagedRange = 1.0
    return distance < getDamagedRange
end


-- Helper Functions
local function IsPlayerInRange()
    local tr = Engine.FindTransformByName(PLAYER_NAME)
    local pos = Engine.GetTransformPosition(tr)  -- Get the table

    local player_x = pos[1]  -- First element
    local player_y = pos[2]  -- Second element
    local player_z = pos[3]  -- Third element

    -- Get enemy position
    local Ground_Enemytr = Engine.FindTransformByName(ENEMY_NAME)
    local enemyPos = Engine.GetTransformPosition(Ground_Enemytr)
    local enemy_x = enemyPos[1]
    local enemy_y = enemyPos[2]
    local enemy_z = enemyPos[3]

    -- Calculate distance
    local dx = player_x - enemy_x
    local dy = player_y - enemy_y
    local dz = player_z - enemy_z
    local distance = math.sqrt(dx*dx + dy*dy + dz*dz)
    
    -- Check if player is within range
    local detectionRange = 8.0  -- Adjust this value as needed
    return distance < detectionRange
end

-- Play random SFX from array
local function playRandomSFX(audio, clips)
    local count = clips and #clips or 0
    if count > 0 and audio then
        audio:PlayOneShot(clips[math.random(1, count)])
    end
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
        Attack_Cooldown = 1.0,

        -- SFX clip arrays (populate in editor with audio GUIDs)
        attackSFXClips = {},
        hurtSFXClips = {},

        -- SFX settings
        sfxVolume        = 0.5,
        attackSoundInterval = 5.0,
    },
    
    Awake = function(self)
        print("[EnemyLogic] Awake")
        self.hasRotated = false
        currentState = IDLE
        
        -- Add timers for periodic SFX
        self._attackSoundTimer = 0.0
        self._attackSoundInterval = 3.3  -- Play attack sound every 2 seconds while attacking
    end,
    
    Start = function(self) 
        print("[EnemyLogic] Start - Caching components...")
        
        -- Cache components (same as PlayerMovement)
        self._animator = self:GetComponent("AnimationComponent")
        self._collider = self:GetComponent("ColliderComponent")
        self._audio = self:GetComponent("AudioComponent")
        
        print("[EnemyLogic] Animator:", self._animator)
        print("[EnemyLogic] Collider:", self._collider)
        print("[EnemyLogic] Audio:", self._audio)
        
        -- Initialize animator
        if self._animator then
            self._animator.enabled = true
            -- Use PlayClip directly like PlayerMovement (not Animation.PlayClip)
            self._animator:PlayClip(IDLE, true)
            print("[EnemyLogic] Started IDLE animation")
        else
            print("[EnemyLogic ERROR] No AnimationComponent found!")
        end
        
        -- Initialize audio
        if self._audio then
            self._audio.enabled = true
            self._audio:SetVolume(self.sfxVolume or 0.5)
        end
    end,
    
    -- Update = function(self, dt) 
    --     -- Safety check
    --     if not self._animator then
    --         return
    --     end
        
    --     local audio = self._audio
    --     local newState = currentState

    --     -- Determine new state 
    --     if self.Health <= 0 then
    --         newState = DEATH
    --     elseif Input.GetMouseButton(Input.MouseButton.Left) and SimulateAttackFromPlayer() then
    --         TakeDamage(self)
    --         newState = TAKE_DAMAGE
    --     elseif IsPlayerInRange() then
    --         newState = ATTACK
    --     else
    --         newState = IDLE
    --     end
        
    --     -- Apply state transition rules
    --     if currentState == DEATH then
    --         newState = DEATH  -- Death cannot be interrupted
    --     elseif currentState == TAKE_DAMAGE then
    --         -- Check if damage animation is still playing using isPlay property
    --         if self._animator.isPlay then
    --             newState = TAKE_DAMAGE  -- Keep playing damage animation
    --         else
    --             newState = ATTACK  -- After damage, go to attack
    --         end
    --     elseif currentState == ATTACK and newState == IDLE then
    --         newState = ATTACK  -- Attack cannot be interrupted by idle
    --     end
        
    --     --HANDLE ROTATION
    --     if (newState == ATTACK or newState == TAKE_DAMAGE) and not self.hasRotated then 
    --         RotateTowardsPlayer(self)
    --         self.hasRotated = true
    --     end
    --     -- Reset rotation flag when leaving ATTACK or TAKE_DAMAGE
    --     if newState ~= ATTACK and newState ~= TAKE_DAMAGE then
    --         self.hasRotated = false
    --     end

    --     --END OF ROTATION HANDLING

    --     -- Update animation if state changed
    --     if newState ~= currentState then
    --         local loop = (newState ~= DEATH and newState ~= TAKE_DAMAGE)
            
    --         -- Use component method directly (like PlayerMovement does with animator:PlayClip)
    --         self._animator:PlayClip(newState, loop)
            
    --         print("[EnemyLogic] State changed: " .. currentState .. " -> " .. newState)
    --         currentState = newState

    --         -- Play appropriate SFX when entering new state
    --         if currentState == ATTACK then
    --             playRandomSFX(audio, self.attackSFXClips)
    --             self._attackSoundTimer = 0.0  -- Reset timer on state entry
    --         elseif currentState == TAKE_DAMAGE then
    --             playRandomSFX(audio, self.hurtSFXClips)
    --         end
    --     end
        
    --     -- Play periodic attack sounds while in ATTACK state
    --     if currentState == ATTACK then
    --         self._attackSoundTimer = self._attackSoundTimer + dt
    --         if self._attackSoundTimer >= self._attackSoundInterval then
    --             playRandomSFX(audio, self.attackSFXClips)
    --             self._attackSoundTimer = 0.0
    --             print("[EnemyLogic] Playing periodic attack sound")
    --         end
    --     end
    -- end
}

--  Animation clip indices:
--  0 = IDLE
--  1 = ATTACK
--  2 = TAKEDAMAGE
--  3 = DEATH