require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

-- Animation States
local IDLE          = 0
local PIERCED       = 1
local TAKE_DAMAGE   = 2
local DEATH         = 3
local FALL          = 4
local ATTACK        = 5

local yPosFloor = 15.963        --HARDCODE FOR NOW

local playerNear = false
local currentState = IDLE

-- Helper Functions
local function IsPlayerInRange()
    if Input.GetKeyDown(Input.Key.U) then
        playerNear = true
    end
    return playerNear
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
            newState = FALL
        elseif Input.GetKeyDown(Input.Key.U) then
            TakeDamage(self)
            newState = TAKE_DAMAGE
        elseif Input.GetKeyDown(Input.Key.I) then
            newState = ATTACK
        else
            newState = IDLE
        end
        
        -- Apply state transition rules
        if currentState == FALL then
            newState = FALL  -- Death cannot be interrupted
        elseif currentState == TAKE_DAMAGE then
            if Animation.IsPlaying(self.animation) then
                newState = TAKE_DAMAGE  -- Keep playing damage animation
            else
                newState = ATTACK  -- After damage, go to attack
            end
        elseif currentState == ATTACK and newState == IDLE then
            newState = ATTACK  -- Attack cannot be interrupted by idle
        end


        if currentState == FALL then            
            self:Move(0,-0.05,0)
            local current_pos = self:GetPosition()
            -- if self.transform.y <= yPosFloor then
            --     newState = Death
            -- end
        end


        
        -- Update animation if state changed
        if newState ~= currentState then
            Animation.Pause(self.animation)
            local loop = (newState ~= FALL and newState ~= TAKE_DAMAGE and newState ~= Death)
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
