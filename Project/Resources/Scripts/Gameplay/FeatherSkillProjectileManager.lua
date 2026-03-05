require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

local function multiplyQuat(q1, q2)
    return {
        w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z,
        x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
        y = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x,
        z = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w
    }
end

local function rotateVec(q, v)
    local qx, qy, qz, qw = q.x, q.y, q.z, q.w
    local vx, vy, vz = v.x, v.y, v.z
    local tx = 2 * (qy * vz - qz * vy)
    local ty = 2 * (qz * vx - qx * vz)
    local tz = 2 * (qx * vy - qy * vx)
    
    return {
        x = vx + qw * tx + (qy * tz - qz * ty),
        y = vy + qw * ty + (qz * tx - qx * tz),
        z = vz + qw * tz + (qx * ty - qy * tx)
    }
end

local function getWorldTransform(entityId)
    local wPos = { x=0, y=0, z=0 }
    local wRot = { w=1, x=0, y=0, z=0 }
    
    local currId = entityId
    while currId and currId >= 0 do
        local t = GetComponent(currId, "Transform")
        if t then
            if t.localScale then
                wPos.x = wPos.x * t.localScale.x
                wPos.y = wPos.y * t.localScale.y
                wPos.z = wPos.z * t.localScale.z
            end
            
            wPos = rotateVec(t.localRotation, wPos)
            wPos.x = wPos.x + t.localPosition.x
            wPos.y = wPos.y + t.localPosition.y
            wPos.z = wPos.z + t.localPosition.z
            
            wRot = multiplyQuat(t.localRotation, wRot)
        end
        
        if Engine and Engine.GetParentEntity then
            local pId = Engine.GetParentEntity(currId)
            if pId == currId then break end 
            currId = pId
        else
            break
        end
    end
    return wPos, wRot
end

return Component {
    mixins = { TransformMixin },

    fields = {
        ColliderDisabledDuration = 0.5,
        RigidbodyKinematicDuration = 1.0,
        PlayerEntityName = "Player",
        FeatherRemnantPrefabPath = "Resources/Prefabs/FeatherSkillProjectileRemnant.prefab",
        SpawnRemnantOffset = -1.0,

        ProjectileDamage = 5.0,
        ProjectileSpeed = 25.0,
    },

    Awake = function(self)

    end,

    Start = function(self)
        self._colliderDisabledDuration = self.ColliderDisabledDuration
        self._rigidbodyKinematicDuration = self.RigidbodyKinematicDuration
        
        self._hasCollided = false
        self._hitRootEntityId = nil
        self._raycastHitPos = nil
        
        local rb = self:GetComponent("RigidBodyComponent")
        if rb then
            rb.ccd = true
            rb.motion_dirty = true
        end
    end,

    Update = function(self, dt)
        -- ==========================================
        -- DEFERRED COLLISION HANDLING
        -- ==========================================
        if self._hasCollided then
            print("[FeatherSkillProjectileManager] Instantiating FeatherRemnant prefab")
            local featherRemnantPrefab = Prefab.InstantiatePrefab(self.FeatherRemnantPrefabPath)
            print("[FeatherSkillProjectileManager] Instantiated FeatherRemnant prefab")

            local parentId = Engine.GetParentEntity(self.entityId)
            local fwdX, fwdY, fwdZ = 0, 0, -1 
            
            if parentId and parentId >= 0 then
                local parentTr = GetComponent(parentId, "Transform")
                if parentTr then
                    local pw, px, py, pz = parentTr.localRotation.w, parentTr.localRotation.x, parentTr.localRotation.y, parentTr.localRotation.z
                    
                    fwdX = -(2 * (px * pz + pw * py))
                    fwdY = -(2 * (py * pz - pw * px))
                    fwdZ = -(1 - 2 * (px * px + py * py))
                end
            end

            local wPos, wRot = getWorldTransform(self.entityId)
            local impactPos = self._raycastHitPos or wPos 
            print(string.format("[FeatherSkillProjectile] Impact point: %f %f %f", impactPos.x, impactPos.y, impactPos.z))

            local featherRemnantPrefabTr = GetComponent(featherRemnantPrefab, "Transform")
            
            if featherRemnantPrefabTr then
                featherRemnantPrefabTr.localPosition.x = impactPos.x + (fwdX * self.SpawnRemnantOffset)
                featherRemnantPrefabTr.localPosition.y = impactPos.y + (fwdY * self.SpawnRemnantOffset)
                featherRemnantPrefabTr.localPosition.z = impactPos.z + (fwdZ * self.SpawnRemnantOffset)
                
                featherRemnantPrefabTr.localRotation.w = wRot.w
                featherRemnantPrefabTr.localRotation.x = wRot.x
                featherRemnantPrefabTr.localRotation.y = wRot.y
                featherRemnantPrefabTr.localRotation.z = wRot.z

                featherRemnantPrefabTr.isDirty = true
            end

            if self._hitRootEntityId then
                local rootEntityTag = Engine.GetEntityTag(self._hitRootEntityId)
                if rootEntityTag and Tag and Tag.CompareNames and Tag.CompareNames(rootEntityTag, "Enemy") then
                    if event_bus and event_bus.publish then 
                        event_bus.publish("deal_damage_to_entity", {
                            entityId = self._hitRootEntityId,
                            damage   = self.ProjectileDamage,
                            hitType   = "FEATHER",
                        }) 
                    end
                end
            end

            if Engine.DestroyEntity then
                Engine.DestroyEntity(self.entityId)
            end
            
            return
        end

        -- ==========================================
        -- PREDICTIVE RAYCASTING
        -- ==========================================
        -- Only raycast once the collider delay is over (avoids hitting the player instantly)
        if self._colliderDisabledDuration <= 0 and Physics and Physics.RaycastGetEntity then
            local parentId = Engine.GetParentEntity(self.entityId)
            local fwdX, fwdY, fwdZ = 0, 0, -1 
            
            if parentId and parentId >= 0 then
                local parentTr = GetComponent(parentId, "Transform")
                if parentTr then
                    local pw, px, py, pz = parentTr.localRotation.w, parentTr.localRotation.x, parentTr.localRotation.y, parentTr.localRotation.z
                    fwdX = (2 * (px * pz + pw * py))
                    fwdY = (2 * (py * pz - pw * px))
                    fwdZ = (1 - 2 * (px * px + py * py))
                end
            end

            local wPos, _ = getWorldTransform(self.entityId)
            local moveStep = self.ProjectileSpeed * dt

            -- [FIX 1] Pull the raycast origin 0.5 meters BEHIND the feather! 
            -- This guarantees the raycast will not start "inside" the wall if the framerate drops.
            --local startOffset = -0.5 
            local rayOriginX = wPos.x
            local rayOriginY = wPos.y
            local rayOriginZ = wPos.z

            -- [FIX 2] Make the raycast longer to cover the pull-back offset + movement
            local raycastDist = moveStep

            local res1, res2 = Physics.RaycastGetEntity(rayOriginX, rayOriginY, rayOriginZ, fwdX, fwdY, fwdZ, raycastDist)
            local hitDist, hitEntityId = -1.0, -1

            -- print(string.format("[FeatherSkillProjectile] Ray origin: %f %f %f", rayOriginX, rayOriginY, rayOriginZ))
            -- print(string.format("[FeatherSkillProjectile] Ray forward: %f %f %f", fwdX, fwdY, fwdZ))
            -- print(string.format("[FeatherSkillProjectile] Raycast dist: %f", raycastDist))
            
            if type(res1) == "table" or type(res1) == "userdata" then
                -- If it's a struct, it uses .distance. If it's a tuple, it uses [1]
                hitDist = res1.distance or res1[1] or -1.0
                hitEntityId = res1.entityId or res1[2] or -1
            else
                -- If it successfully unpacked into two separate variables
                hitDist = res1 or -1.0
                hitEntityId = res2 or -1
            end

            -- Print the safely extracted variables! (This prevents the string.format nil crash)
            --print(string.format("[FeatherSkillProjectile] Raycasthit dist: %f Raycasthit entity: %d", hitDist, hitEntityId))
            
            if hitDist >= 0.0 then
                local isValidHit = true
                local otherRootId = nil

                if hitEntityId >= 0 then
                    local otherEntityRb = GetComponent(hitEntityId, "RigidBodyComponent")
                    
                    if otherEntityRb and otherEntityRb.isTrigger then
                        isValidHit = false
                    else
                        otherRootId = self:_toRoot(hitEntityId)
                        if Engine.GetEntityName(otherRootId) == self.PlayerEntityName or otherRootId == parentId then
                            isValidHit = false
                        end
                    end
                end

                if isValidHit then
                    self._hasCollided = true
                    self._hitRootEntityId = otherRootId 
                    
                    -- Add the distance properly to calculate exact impact point from our new backward offset
                    self._raycastHitPos = {
                        x = rayOriginX + (fwdX * hitDist),
                        y = rayOriginY + (fwdY * hitDist),
                        z = rayOriginZ + (fwdZ * hitDist)
                    }

                    --print(string.format("[FeatherSkillProjectile] RaycasthitPos: %f %f %f", self._raycastHitPos.x, self._raycastHitPos.y, self._raycastHitPos.z))
                end
            end
        end

        -- ==========================================
        -- NORMAL UPDATE TIMERS
        -- ==========================================
        local collider = self:GetComponent("ColliderComponent")
        if collider then
            self._colliderDisabledDuration = self._colliderDisabledDuration - dt
            if self._colliderDisabledDuration <= 0 then
                collider.enabled = true
            end
        end

        local rb = self:GetComponent("RigidBodyComponent")
        if rb then
            self._rigidbodyKinematicDuration = self._rigidbodyKinematicDuration - dt
            if self._rigidbodyKinematicDuration <= 0 then
                rb.motionID = 2
                rb.motion_dirty = true
            end
        end
    end,

    OnDisable = function(self)

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

    OnTriggerEnter = function(self, otherEntityId)
        if self._hasCollided then return end

        local otherEntityRb = GetComponent(otherEntityId, "RigidBodyComponent")
        if otherEntityRb and otherEntityRb.isTrigger then return end

        local otherRootEntityId = self:_toRoot(otherEntityId)
        if Engine.GetEntityName(otherRootEntityId) == self.PlayerEntityName then return end

        local parentId = Engine.GetParentEntity(self.entityId)
        if otherRootEntityId == parentId then return end

        self._hasCollided = true
        self._hitRootEntityId = otherRootEntityId
    end,
}