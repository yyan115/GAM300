-- Resources/Scripts/GamePlay/AttackHitbox.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    fields = {
        hitboxRange = 1.5,
        maxEnemies = 4,
        isActive = false,
    },

    Start = function(self)
        self.myEntityId = self.entityId
        self.hitEnemies = {}
        
        -- Subscribe to attack events from ComboManager
        if event_bus then
            self:Subscribe("attack_performed", function(data)
                self:OnAttackPerformed(data)
            end)
        else
            print("[AttackHitbox] ERROR: event_bus not available!")
        end
        
        print("[AttackHitbox] Initialized")
    end,

    Update = function(self)
        if not self.isActive or not self.myEntityId then 
            return 
        end
        
        self:CheckForHits()
    end,

    OnAttackPerformed = function(self, attackData)
        -- Store attack data AS-IS from ComboManager
        self.currentAttackData = attackData
        self.isActive = true
        self.hitEnemies = {}
        
        print("[AttackHitbox] Attack activated: " .. tostring(attackData.state))
        
        -- Disable after one frame
        self:After(0.1, function()
            self.isActive = false
        end)
    end,

    CheckForHits = function(self)
        local enemies = Engine.GetEntitiesByTag("Enemy", self.maxEnemies)
        if #enemies == 0 then return end
        
        -- Get player transform for position/direction
        local playerTransform = Engine.FindTransformByName("Player")
        if not playerTransform then return end
        
        -- Get position - SAFE handling like camera_follow
        local px, py, pz = 0, 0, 0
        local ok, a, b, c = pcall(function()
            if Engine and Engine.GetTransformWorldPosition then
                return Engine.GetTransformWorldPosition(playerTransform)
            end
            return nil
        end)
        
        if ok and a ~= nil then
            if type(a) == "table" then
                px = a[1] or a.x or 0
                py = a[2] or a.y or 0
                pz = a[3] or a.z or 0
            else
                px, py, pz = a, b, c
            end
        end
        
        -- Get rotation - SAFE handling like camera_follow
        local qw, qx, qy, qz = 1, 0, 0, 0
        ok, a, b, c = pcall(function()
            if Engine and Engine.GetTransformWorldRotation then
                return Engine.GetTransformWorldRotation(playerTransform)
            end
            return nil
        end)
        
        if ok and a ~= nil then
            if type(a) == "table" then
                qw = a[1] or a.w or 1
                qx = a[2] or a.x or 0
                qy = a[3] or a.y or 0
                qz = a[4] or a.z or 0
            else
                qw, qx, qy, qz = a, b, c, nil
                -- If 4th value is missing, need to handle it
                if not qz then
                    -- Assume a,b,c were x,y,z and w is missing
                    qw, qx, qy, qz = 1, a, b, c
                end
            end
        end
        
        -- Forward vector from quaternion (safe with defaults)
        local forward = {
            x = 2 * ((qx or 0) * (qz or 0) + (qw or 1) * (qy or 0)),
            y = 2 * ((qy or 0) * (qz or 0) - (qw or 1) * (qx or 0)),
            z = 1 - 2 * ((qx or 0) * (qx or 0) + (qy or 0) * (qy or 0))
        }
        
        for i, enemyId in ipairs(enemies) do
            if not self.hitEnemies[enemyId] then
                if Physics.CheckDistance(self.myEntityId, enemyId, self.hitboxRange) then
                    
                    -- Get enemy position - SAFE handling
                    local ex, ey, ez = 0, 0, 0
                    local enemyTransform = GetComponent(enemyId, "Transform")
                    if enemyTransform then
                        ok, a, b, c = pcall(function()
                            if Engine and Engine.GetTransformWorldPosition then
                                return Engine.GetTransformWorldPosition(enemyTransform)
                            end
                            return nil
                        end)
                        
                        if ok and a ~= nil then
                            if type(a) == "table" then
                                ex = a[1] or a.x or 0
                                ey = a[2] or a.y or 0
                                ez = a[3] or a.z or 0
                            else
                                ex, ey, ez = a, b, c
                            end
                        end
                    end
                    
                    -- Calculate direction (player -> enemy)
                    local dirX = ex - px
                    local dirY = ey - py
                    local dirZ = ez - pz
                    local length = math.sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ)
                    if length > 0.001 then
                        dirX, dirY, dirZ = dirX/length, dirY/length, dirZ/length
                    end
                    
                    -- Send damage event with ALL data from ComboManager
                    if event_bus then
                        event_bus.publish("deal_damage", {
                            targetEntityId = enemyId,
                            attackData = self.currentAttackData,  -- Pass entire attack data
                            direction = { x = dirX, y = dirY, z = dirZ },
                            attackerPosition = { x = px, y = py, z = pz }
                        })
                        
                        print("[AttackHitbox] Hit enemy " .. enemyId)
                    end
                    
                    self.hitEnemies[enemyId] = true
                end
            end
        end
    end,

    OnDisable = function(self)
        self.isActive = false
        self.hitEnemies = {}
    end,
}