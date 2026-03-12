require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    fields = {
        InitialInactiveDuration = 0.5,
        AttractionStrength = 60.0, 
        Drag = 5.0,                
        MaxSpeed = 40.0,           
        PickupDetectionRadius = 1.0,
        CollectionRadius = 0.05,    
        TargetScale = 0.1,
        LiftHeight = 0.3,          
        PlayerEntityName = "Player",
    },

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        self._rb = self:GetComponent("RigidBodyComponent")
        self._collider = self:GetComponent("ColliderComponent")

        self._initialInactiveDuration = self.InitialInactiveDuration
        self._colliderEnabled = false

        self._parentFeatherEntity = Engine.GetParentEntity(self.entityId)
        self._playerEntityId = Engine.GetEntityByName(self.PlayerEntityName)
        
        self._onTriggerStayed = false
        self._isCollecting = false
        self._velocity = { x=0, y=0, z=0 }
    end,

    Update = function(self, dt)
        if self._collected then return end
        if not self._parentFeatherEntity or self._parentFeatherEntity < 0 then return end

        -- [FIXED] Stop setting it to true every frame once it's enabled
        if not self._colliderEnabled then
            self._initialInactiveDuration = self._initialInactiveDuration - dt
            if self._initialInactiveDuration <= 0 then
                if self._collider then
                    self._collider.enabled = true
                end
                self._colliderEnabled = true
            end
        end

        if self._colliderEnabled and not self._isCollecting then
            local playerId = self._playerEntityId
            if not playerId or playerId < 0 then
                playerId = Engine.GetEntityByName(self.PlayerEntityName)
                self._playerEntityId = playerId
            end

            if playerId and playerId >= 0 then
                local parentTransform = GetComponent(self._parentFeatherEntity, "Transform")
                local playerTransform = GetComponent(playerId, "Transform")

                if parentTransform and playerTransform then
                    local myPos = parentTransform.localPosition
                    local playerPos = playerTransform.localPosition
                    local dx = playerPos.x - myPos.x
                    local dy = (playerPos.y + self.LiftHeight) - myPos.y
                    local dz = playerPos.z - myPos.z
                    local radius = self.PickupDetectionRadius or 1.0

                    if (dx * dx + dy * dy + dz * dz) <= (radius * radius) then
                        self:TryCollect(playerId)
                        if self._isCollecting or self._collected then
                            return
                        end
                    end
                end
            end
        end

        if self._isCollecting and self._playerTransform then
            -- Fetch Transform FRESH every frame!
            local parentTransform = GetComponent(self._parentFeatherEntity, "Transform")
            if not parentTransform then return end -- Safety check

            -- [Physics Override]
            local freshParentRb = GetComponent(self._parentFeatherEntity, "RigidBodyComponent")
            if freshParentRb then
                freshParentRb.enabled = false
                freshParentRb.isTeleporting = true
            end

            -- Target Calculation
            local targetPos = self._playerTransform.localPosition
            local targetX = targetPos.x
            local targetY = targetPos.y + self.LiftHeight
            local targetZ = targetPos.z

            -- [ZOMBIE HANDLING] If already collected (waiting for Destroy), snap and hide
            if self._collected then
                parentTransform.localPosition.x = targetX
                parentTransform.localPosition.y = targetY
                parentTransform.localPosition.z = targetZ
                
                parentTransform.localScale.x = 0
                parentTransform.localScale.y = 0
                parentTransform.localScale.z = 0
                
                parentTransform.isDirty = true
                return 
            end

            -- Standard Movement Logic
            local myPos = parentTransform.localPosition
            local dx = targetX - myPos.x
            local dy = targetY - myPos.y
            local dz = targetZ - myPos.z
            local distSq = dx*dx + dy*dy + dz*dz
            local dist = math.sqrt(distSq)

            -- Drag
            local dragFactor = 1.0 - (self.Drag * dt)
            if dragFactor < 0 then dragFactor = 0 end
            self._velocity.x = self._velocity.x * dragFactor
            self._velocity.y = self._velocity.y * dragFactor
            self._velocity.z = self._velocity.z * dragFactor

            -- Attraction
            if dist > 0.001 then
                local nx, ny, nz = dx/dist, dy/dist, dz/dist
                self._velocity.x = self._velocity.x + (nx * self.AttractionStrength * dt)
                self._velocity.y = self._velocity.y + (ny * self.AttractionStrength * dt)
                self._velocity.z = self._velocity.z + (nz * self.AttractionStrength * dt)
            end

            -- Clamp Speed
            local vSq = self._velocity.x^2 + self._velocity.y^2 + self._velocity.z^2
            local currentSpeed = math.sqrt(vSq)
            if currentSpeed > self.MaxSpeed then
                local scale = self.MaxSpeed / currentSpeed
                self._velocity.x = self._velocity.x * scale
                self._velocity.y = self._velocity.y * scale
                self._velocity.z = self._velocity.z * scale
                currentSpeed = self.MaxSpeed
            end

            -- [Tunneling & Collection Check]
            local moveDist = currentSpeed * dt
            if (dist < self.CollectionRadius) or (dist <= moveDist) then
                print(string.format("[EnemyFeatherCollectible] Player collected feather - Entity %d", self.entityId))
                self:OnCollected()
                return
            else
                -- Apply normal movement
                myPos.x = myPos.x + self._velocity.x * dt
                myPos.y = myPos.y + self._velocity.y * dt
                myPos.z = myPos.z + self._velocity.z * dt
            end

            -- [Scale Logic]
            if self._startDistance and self._startDistance > 0 then
                local t = 1.0 - (dist / self._startDistance)
                if t < 0 then t = 0 end
                if t > 1 then t = 1 end

                local startS = self._startScale
                local targetS = self.TargetScale

                if startS then
                    local currentScale = parentTransform.localScale
                    currentScale.x = startS.x + (targetS - startS.x) * t
                    currentScale.y = startS.y + (targetS - startS.y) * t
                    currentScale.z = startS.z + (targetS - startS.z) * t
                    parentTransform.localScale = currentScale
                end
            end

            parentTransform.localPosition.x = myPos.x
            parentTransform.localPosition.y = myPos.y
            parentTransform.localPosition.z = myPos.z
            parentTransform.isDirty = true
        end
    end,

    _toRoot = function(self, entityId)
        local targetId = entityId
        if Engine and Engine.GetParentEntity then
            while true do
                local parentId = Engine.GetParentEntity(targetId)
                if not parentId or parentId < 0 then break end
                targetId = parentId
            end
        end
        return targetId
    end,

    -- [NEW] Reusable collection logic
    TryCollect = function(self, otherEntityId)
        if self._collected then return end
        if not otherEntityId or otherEntityId < 0 then return end
        if self._onTriggerStayed then return end
        if self._isCollecting then return end
        local name = Engine.GetEntityName(otherEntityId)
        print(string.format("Collided with entity entity %s", name))
        
        local rootId = self:_toRoot(otherEntityId)
        local tagComp = GetComponent(rootId, "TagComponent")
        local rootname = Engine.GetEntityName(rootId)
        print(string.format("Checking tag component of entity %s", rootname))
        
        if tagComp and Tag.Compare(tagComp.tagIndex, "Player") then
            print("Tag component == Player")
            -- [FIXED] Fetch the parent transform FRESH! Do not use the stale cached one from Start.
            local parentTransform = GetComponent(self._parentFeatherEntity, "Transform")
            if not parentTransform then return end

            self._isCollecting = true
            self._playerTransform = GetComponent(rootId, "Transform")
            
            local myPos = parentTransform.localPosition
            local targetPos = self._playerTransform.localPosition
            local dx = targetPos.x - myPos.x
            local dy = (targetPos.y + self.LiftHeight) - myPos.y
            local dz = targetPos.z - myPos.z
            self._startDistance = math.sqrt(dx*dx + dy*dy + dz*dz)

            -- Initial "Pop" Velocity upwards
            self._velocity = { x=0, y=5.0, z=0 } 

            local s = parentTransform.localScale
            self._startScale = { x = s.x, y = s.y, z = s.z }

            self._onTriggerStayed = true

            print(string.format("[EnemyFeatherCollectible] Player collecting feather - Entity %d", self.entityId))
        end
    end,

    -- [FIXED] Route BOTH physics events into the collection logic!
    OnTriggerEnter = function(self, otherEntityId)
        print("OnTriggerEnter")
        self:TryCollect(otherEntityId)
    end,

    OnTriggerStay = function(self, otherEntityId)
        print("OnTriggerStay")
        self:TryCollect(otherEntityId)
    end,

    OnCollected = function(self)
        if self._collected then return end
        self._collected = true
        self._isCollecting = false

        if self._collider then
            self._collider.enabled = false
        end

        if self._rb then
            self._rb.enabled = false
        end

        Engine.DestroyEntity(self._parentFeatherEntity)

        if _G.ActiveFeatherCount then 
            _G.ActiveFeatherCount = math.max(0, _G.ActiveFeatherCount - 1)
        end

        if event_bus and event_bus.publish then
            event_bus.publish("featherCollected", true)
        end
    end,
}
