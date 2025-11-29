require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

-- Animation States
local ATTACK        = 5
return Component {
    mixins = { TransformMixin },
    
    fields = {
        Speed = 8.0,
        Lifetime = 1.8,
        Damage = 1,
        ThrowDelay = 1.5  -- Adjust to match animation timing
    },
    
    Start = function(self) 
        self.model = self:GetComponent("ModelRenderComponent")
        self.active = false
        self.directionX = 0
        self.directionY = 0
        self.directionZ = 0
        self.age = 0
        self.hasLaunched = false
        self.attackTimer = 0  -- Timer for attack delay
        
        -- Hide knife initially
        if self.model then
            ModelRenderComponent.SetVisible(self.model, false)
        end
    end,
    
    Update = function(self, dt)
        -- Check if enemy is in attack animation
        local clipIndex = Engine.GetClipIndexByName("FlyingEnemy")
        
        if clipIndex == ATTACK then
            if not self.hasLaunched then
                -- Count up the timer
                self.attackTimer = self.attackTimer + dt
                
                -- Launch after delay
                if self.attackTimer >= self.ThrowDelay then
                    self:Launch()
                    self.hasLaunched = true
                    self.attackTimer = 0
                end
            end
        else
            -- Reset when not attacking
            self.hasLaunched = false
            self.attackTimer = 0
        end
        
        -- Handle flying knife
        if self.active then
            -- Update age
            self.age = self.age + dt
            
            -- Despawn if too old
            if self.age > self.Lifetime then
                self:Reset()
                return
            end
            
            -- Calculate movement delta
            local dx = self.directionX * self.Speed * dt
            local dy = self.directionY * self.Speed * dt
            local dz = self.directionZ * self.Speed * dt
            
            -- Move the knife
            self:Move(dx, dy, dz)
        end
    end,
    
Launch = function(self)
    -- Local offset from enemy (relative to enemy's facing direction)
    local offsetX   = -0.5
    local offsetY   = 1
    local offsetZ   = 0        

    -- Get enemy position and rotation
    local enemyTr = Engine.FindTransformByName("FlyingEnemy")
    local enemyPos = Engine.GetTransformPosition(enemyTr)
    local enemyRot = Engine.GetTransformRotation(enemyTr)  -- Get enemy rotation
    
    -- Convert local offset to world space based on enemy rotation
    -- You'll need to rotate the offset vector by the enemy's rotation
    -- This is a simplified version - adjust based on your rotation format
    
    -- Get enemy's forward direction from rotation
    local enemy_x = enemyPos[1]
    local enemy_y = enemyPos[2]
    local enemy_z = enemyPos[3]
    
    -- Get player position (target)
    local playerTr = Engine.FindTransformByName("Player")
    local playerPos = Engine.GetTransformPosition(playerTr)
    local player_x = playerPos[1]
    local player_y = playerPos[2] + 0.5
    local player_z = playerPos[3]
    
    -- Calculate enemy's forward direction (same calculation as rotation)
    local dx_to_player = player_x - enemy_x
    local dz_to_player = player_z - enemy_z
    local angle = math.atan(dx_to_player, dz_to_player)
    
    -- Rotate the offset based on enemy's facing angle
    local cos_angle = math.cos(angle)
    local sin_angle = math.sin(angle)
    
    -- Apply rotation to offset (2D rotation in XZ plane)
    local rotated_offsetX = offsetX * cos_angle - offsetZ * sin_angle
    local rotated_offsetZ = offsetX * sin_angle + offsetZ * cos_angle
    
    -- Apply rotated offset to enemy position
    local spawn_x = enemy_x + rotated_offsetX
    local spawn_y = enemy_y + offsetY  -- Y doesn't rotate
    local spawn_z = enemy_z + rotated_offsetZ
    
    -- Calculate direction vector from spawn point to player
    local dx = player_x - spawn_x
    local dy = player_y - spawn_y
    local dz = player_z - spawn_z
    
    -- Normalize direction
    local distance = math.sqrt(dx*dx + dy*dy + dz*dz)
    if distance > 0 then
        self.directionX = dx / distance
        self.directionY = dy / distance
        self.directionZ = dz / distance
    end
    
    -- Set starting position
    self:SetPosition(spawn_x, spawn_y, spawn_z)

    print("initial position of knife is ", spawn_x, spawn_y, spawn_z)
    
    -- Activate knife
    self.active = true
    self.age = 0
    
    if self.model then
        ModelRenderComponent.SetVisible(self.model, true)
    end
    
    print("Knife launched!")
end,


    Reset = function(self)
        self.active = false
        self.age = 0
        self.directionX = 0
        self.directionY = 0
        self.directionZ = 0
        self.hasLaunched = false
        if self.model then
            ModelRenderComponent.SetVisible(self.model, false)
        end
        print("Knife reset")
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
