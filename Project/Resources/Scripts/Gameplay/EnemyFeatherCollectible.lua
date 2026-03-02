require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

local function eulerToQuat(pitch, yaw, roll)
    -- Inputs are in DEGREES. Your math.random(0, 360) is already in degrees.
    local p = math.rad(pitch or 0) * 0.5
    local y = math.rad(yaw or 0)   * 0.5
    local r = math.rad(roll or 0)  * 0.5

    local sinP, cosP = math.sin(p), math.cos(p)
    local sinY, cosY = math.sin(y), math.cos(y)
    local sinR, cosR = math.sin(r), math.cos(r)

    -- Standard ZYX conversion
    return {
        w = cosP * cosY * cosR + sinP * sinY * sinR,
        x = sinP * cosY * cosR - cosP * sinY * sinR,
        y = cosP * sinY * cosR + sinP * cosY * sinR,
        z = cosP * cosY * sinR - sinP * sinY * cosR
    }
end

return Component {
    mixins = { TransformMixin },

    fields = {
        InitialInactiveDuration = 0.5,
        CollectionSpeed = 10.0,
        CollectionRadius = 0.1,
    },

    Awake = function(self)
        
    end,

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        self._rb = self:GetComponent("RigidBodyComponent")
        self._collider = self:GetComponent("ColliderComponent")

        self._initialInactiveDuration = self.InitialInactiveDuration
        self._colliderEnabled = false

        self._parentFeatherEntity = Engine.GetParentEntity(self.entityId)
        self._parentTransform = GetComponent(self._parentFeatherEntity, "Transform")
        self._parentRb = GetComponent(self._parentFeatherEntity, "RigidBodyComponent")
    end,

    Update = function(self, dt)
        if self._colliderEnabled == false then
            self._initialInactiveDuration = self._initialInactiveDuration - dt
            if self._initialInactiveDuration <= 0 then
                self._colliderEnabled = true
                self._collider.enabled = true
            end
        end

        if self._isCollecting and self._playerTransform then
            -- Disable physics so we can move manually without fighting gravity
            if self._parentRb and self._parentRb.enabled then
                self._parentRb.enabled = false
            end

            -- Get Positions
            local myPos = self._parentTransform.localPosition -- or GetPosition()
            local targetPos = self._playerTransform.localPosition -- or GetPosition()

            -- Calculate Direction
            local dx = targetPos.x - myPos.x
            local dy = (targetPos.y + 1.0) - myPos.y -- Aim for chest/head, not feet
            local dz = targetPos.z - myPos.z

            local distSq = dx*dx + dy*dy + dz*dz
            
            -- Check if reached
            if distSq < (self.CollectionRadius * self.CollectionRadius) then
                -- self:OnCollected()
                return
            end

            -- Move towards player
            local dist = math.sqrt(distSq)
            local moveStep = self.CollectionSpeed * dt
            
            -- Prevent overshooting
            if moveStep > dist then moveStep = dist end

            -- Normalize and Scale
            local nx, ny, nz = dx/dist, dy/dist, dz/dist
            
            myPos.x = myPos.x + nx * moveStep
            myPos.y = myPos.y + ny * moveStep
            myPos.z = myPos.z + nz * moveStep

            self._parentTransform.localPosition = myPos
            self._parentTransform.isDirty = true
        end
    end,

    OnDisable = function(self)

    end,

    -- Walk up the hierarchy to find the root entity
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

    OnTriggerEnter = function(self, otherEntityId)
        -- Don't trigger if already collecting
        if self._isCollecting then return end

        local rootId = self:_toRoot(otherEntityId)
        local tagComp = GetComponent(rootId, "TagComponent")
        
        if tagComp and Tag.Compare(tagComp.tagIndex, "Player") then
            print("[EnemyFeatherCollectible] OnTriggerEnter with Player!")

            -- 1. Mark as collecting
            self._isCollecting = true
            
            -- 2. Cache the Player Transform for the Update loop
            self._playerTransform = GetComponent(rootId, "Transform")
            
            -- 3. Disable collision trigger so we don't trigger it again
            if self._collider then self._collider.enabled = false end
        end
    end,

    OnCollected = function(self)
        -- [Logic to give player resource goes here]
        print("[EnemyFeatherCollectible] Collected!")
        
        -- Destroy Self
        Engine.DestroyEntity(self.entityId)
        
        -- Optional: Decrement your global feather count to keep the limit logic working
        if _G.ActiveFeatherCount then 
            _G.ActiveFeatherCount = math.max(0, _G.ActiveFeatherCount - 1)
        end
    end,
}