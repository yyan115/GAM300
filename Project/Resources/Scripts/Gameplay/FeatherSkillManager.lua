require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

local function lerp(a, b, t)
    return a + (b - a) * t
end

local function eulerToQuat(pitch, yaw, roll)
    local p = math.rad(pitch or 0) * 0.5
    local y = math.rad(yaw or 0)   * 0.5
    local r = math.rad(roll or 0)  * 0.5

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
        CastingRotationSpeed = 360.0,
        FeatherTargetRotationX = 70.0, 
        LaunchRotationSpeed = 1080.0, 
        ProjectileSpeed = 25.0,
        SkillSizeFactor = 2.0,
        MaxAliveDuration = 10.0,
        
        FocalDistance = 30.0,       
        AimVerticalOffset = 0.0,    
        
        NumFeathersToSpawn = 10,
        FeatherProjectilePrefabPath = "Resources/Prefabs/FeatherSkillProjectile.prefab",
        SpawnDuration = 1.0,
        
        CastDelay = 3.0,
        ConvergeDuration = 0.5,
        
        WindupDuration = 0.2,
        WindupPullbackDistance = 0.6,

        SkillLightEntityName = "SkillLight",
        SkillLightInitialDiffuseR = 255,
        SkillLightInitialDiffuseG = 170,
        SkillLightInitialDiffuseB = 170,

        SkillLightCastedDiffuseR = 255,
        SkillLightCastedDiffuseG = 0,
        SkillLightCastedDiffuseB = 0,

    },

    Awake = function(self)
        self._spawnTimer = 0.0
        self._castDelay = self.CastDelay
        self._windupTimer = self.WindupDuration
        self._aliveDuration = self.MaxAliveDuration

        self._state = 0
        self._feathersSpawned = 0
    end,

    Start = function(self)
        local transform = self:GetComponent("Transform")
        if not transform then return end
        
        self._lockedCamX     = _G.CAMERA_POS_X or 0.0
        self._lockedCamY     = _G.CAMERA_POS_Y or 0.0
        self._lockedCamZ     = _G.CAMERA_POS_Z or 0.0
        self._lockedCamFwdX  = _G.CAMERA_FWD_X or 0.0
        self._lockedCamFwdY  = _G.CAMERA_FWD_Y or 0.0
        self._lockedCamFwdZ  = _G.CAMERA_FWD_Z or -1.0

        -- [FIX] The engine recycles Entity IDs, but sometimes fails to clear stale parent/child 
        -- relationships. Before we calculate our world position, we must ensure we are 
        -- NOT parented to a dead enemy (which would teleport us to its last position).
        if Engine and Engine.SetParentEntity then
            Engine.SetParentEntity(self.entityId, -1)
        end

        -- Now that we are unparented from any stale hierarchy, re-parent to the player 
        -- so we spawn at the correct location (the player's hand).
        if playerEntity and Engine.SetParentEntity then
            Engine.SetParentEntity(self.entityId, playerEntity)
        end

        local wPos, wRot = getWorldTransform(self.entityId)

        -- Finally, unparent so we can move independently in world space.
        if Engine.SetParentEntity then
            Engine.SetParentEntity(self.entityId, -1)
        end
        
        transform.localPosition.x = wPos.x
        transform.localPosition.y = wPos.y
        transform.localPosition.z = wPos.z

        transform.localScale.x = self.SkillSizeFactor
        transform.localScale.y = self.SkillSizeFactor
        transform.localScale.z = self.SkillSizeFactor

        local targetX = self._lockedCamX + (self._lockedCamFwdX * self.FocalDistance)
        local targetY = self._lockedCamY + (self._lockedCamFwdY * self.FocalDistance) + self.AimVerticalOffset
        local targetZ = self._lockedCamZ + (self._lockedCamFwdZ * self.FocalDistance)

        local dirX = targetX - wPos.x
        local dirY = targetY - wPos.y
        local dirZ = targetZ - wPos.z

        local len = math.sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ)
        if len > 0.001 then
            dirX = dirX / len
            dirY = dirY / len
            dirZ = dirZ / len
        end

        local finalPitch = -math.deg(math.asin(dirY)) 
        local finalYaw = 0
        if dirX == 0 and dirZ == 0 then
            finalYaw = 0
        elseif dirZ > 0 then
            finalYaw = math.deg(math.atan(dirX / dirZ))
        elseif dirZ < 0 then
            finalYaw = math.deg(math.atan(dirX / dirZ)) + (dirX >= 0 and 180.0 or -180.0)
        else
            finalYaw = dirX > 0 and 90.0 or -90.0
        end

        local aimQuat = eulerToQuat(finalPitch, finalYaw, 0)

        transform.localRotation.w = aimQuat.w
        transform.localRotation.x = aimQuat.x
        transform.localRotation.y = aimQuat.y
        transform.localRotation.z = aimQuat.z
        transform.isDirty = true

        self._baseRotation = { w = aimQuat.w, x = aimQuat.x, y = aimQuat.y, z = aimQuat.z }
        self._currentSpinAngle = 0

        self._childFeathers = {}
        self._skillLightEntityId = nil
        
        if Engine and Engine.GetChildrenEntities then
            local childrenIds = Engine.GetChildrenEntities(self.entityId)
            
            for _, childId in ipairs(childrenIds) do
                local childName = Engine.GetEntityName(childId)
                
                if childName == self.SkillLightEntityName then
                    self._skillLightEntityId = childId
                    local skillPointLightComp = GetComponent(childId, "PointLightComponent")
                    if skillPointLightComp then
                        skillPointLightComp.diffuse.x = self.SkillLightInitialDiffuseR / 255.0
                        skillPointLightComp.diffuse.y = self.SkillLightInitialDiffuseG / 255.0
                        skillPointLightComp.diffuse.z = self.SkillLightInitialDiffuseB / 255.0
                    end
                else
                    -- [FIX] Do NOT destroy children! They might be valid game objects (like the 
                    -- chain endpoint) that were parented to a dead enemy whose ID was recycled.
                    -- Just unparent them so they don't interfere with us.
                    if Engine and Engine.SetParentEntity then
                        Engine.SetParentEntity(childId, -1)
                    end
                end
            end
        end
    end,

    SpawnFeather = function(self, index)
        local angleDeg = (360.0 / self.NumFeathersToSpawn) * index
        local angleRad = math.rad(angleDeg)
        
        local posX = math.cos(angleRad) * 0.015
        local posY = math.sin(angleRad) * 0.02
        local posZ = 0.0

        local newFeatherId = Prefab.InstantiatePrefab(self.FeatherProjectilePrefabPath)
        
        -- [NEW] Cache the pure prefab scale BEFORE the engine tries to modify it during parenting
        local childTransform = GetComponent(newFeatherId, "Transform")
        local origScaleX, origScaleY, origScaleZ = 1.0, 1.0, 1.0
        if childTransform then
            origScaleX = childTransform.localScale.x
            origScaleY = childTransform.localScale.y
            origScaleZ = childTransform.localScale.z
        end

        if Engine.SetParentEntity then
            Engine.SetParentEntity(newFeatherId, self.entityId)
        end

        if childTransform then
            childTransform.localPosition.x = posX
            childTransform.localPosition.y = posY
            childTransform.localPosition.z = posZ

            local zQuat = eulerToQuat(0, 0, angleDeg)
            childTransform.localRotation.w = zQuat.w
            childTransform.localRotation.x = zQuat.x
            childTransform.localRotation.y = zQuat.y
            childTransform.localRotation.z = zQuat.z

            -- [FIXED] Do not multiply by SkillSizeFactor! Restore original local scale 
            -- so it cleanly inherits the parent's huge scale uniformly.
            childTransform.localScale.x = origScaleX
            childTransform.localScale.y = origScaleY
            childTransform.localScale.z = origScaleZ
            
            childTransform.isDirty = true

            table.insert(self._childFeathers, {
                entityId = newFeatherId,
                baseRot = { w = zQuat.w, x = zQuat.x, y = zQuat.y, z = zQuat.z }
            })

            print(string.format("[FeatherSkillManager] Spawned feather projectile entity %d at %f %f %f", newFeatherId, childTransform.localPosition.x, childTransform.localPosition.y, childTransform.localPosition.z))
        end

        if index == 0 then
            local lightComp = GetComponent(newFeatherId, "PointLightComponent")
            if lightComp then
                lightComp.enabled = true
            end
        end
    end,

    Update = function(self, dt)
        self._aliveDuration = self._aliveDuration - dt
        if self._aliveDuration <= 0 then
            if Engine and Engine.DestroyEntity then
                Engine.DestroyEntity(self.entityId)
            end
            return
        end

        local transform = self:GetComponent("Transform")
        if not transform then return end
        
        local currentRotationSpeed = 0

        local bw, bx, by, bz = self._baseRotation.w, self._baseRotation.x, self._baseRotation.y, self._baseRotation.z
        local fwdX = 2 * (bx * bz + bw * by)
        local fwdY = 2 * (by * bz - bw * bx)
        local fwdZ = 1 - 2 * (bx * bx + by * by)

        if self._state == 0 then
            currentRotationSpeed = self.CastingRotationSpeed
            self._spawnTimer = self._spawnTimer + dt
            
            local expectedSpawns = math.floor((self._spawnTimer / self.SpawnDuration) * self.NumFeathersToSpawn)
            if expectedSpawns > self.NumFeathersToSpawn then 
                expectedSpawns = self.NumFeathersToSpawn 
            end

            while self._feathersSpawned < expectedSpawns do
                self:SpawnFeather(self._feathersSpawned)
                self._feathersSpawned = self._feathersSpawned + 1
            end

            if self._spawnTimer >= self.SpawnDuration then
                while self._feathersSpawned < self.NumFeathersToSpawn do
                    self:SpawnFeather(self._feathersSpawned)
                    self._feathersSpawned = self._feathersSpawned + 1
                end
                self._state = 1
            end

        elseif self._state == 1 then
            self._castDelay = self._castDelay - dt
            currentRotationSpeed = self.CastingRotationSpeed

            local elapsedCastTime = self.CastDelay - self._castDelay
            local t = elapsedCastTime / self.ConvergeDuration
            if t < 0 then t = 0 end
            if t > 1 then t = 1 end

            local currentXRotation = lerp(0, self.FeatherTargetRotationX, t)
            local pitchQuat = eulerToQuat(currentXRotation, 0, 0)

            for _, child in ipairs(self._childFeathers) do
                local newRot = multiplyQuat(child.baseRot, pitchQuat)
                local childTransform = GetComponent(child.entityId, "Transform")
                if childTransform then 
                    childTransform.localRotation.w = newRot.w
                    childTransform.localRotation.x = newRot.x
                    childTransform.localRotation.y = newRot.y
                    childTransform.localRotation.z = newRot.z
                    childTransform.isDirty = true
                end
            end

            if self._castDelay <= 0.0 then
                self._currentSpinAngle = 0
                local pos = transform.localPosition
                self._windupStartPos = { x = pos.x, y = pos.y, z = pos.z }

                self._state = 2
            end

        elseif self._state == 2 then
            self._windupTimer = self._windupTimer - dt
            currentRotationSpeed = self.LaunchRotationSpeed

            local t = 1.0 - (self._windupTimer / self.WindupDuration)
            if t > 1 then t = 1 end
            if t < 0 then t = 0 end
            
            local easeT = t * t 
            
            local pos = transform.localPosition
            pos.x = self._windupStartPos.x - (fwdX * self.WindupPullbackDistance * easeT)
            pos.y = self._windupStartPos.y - (fwdY * self.WindupPullbackDistance * easeT)
            pos.z = self._windupStartPos.z - (fwdZ * self.WindupPullbackDistance * easeT)
            transform.localPosition = pos

            if self._windupTimer <= 0.0 then
                self._state = 3

                if event_bus and event_bus.publish then
                    event_bus.publish("feather_skill_release", {})
                end
            end

        elseif self._state == 3 then
            local skillPointLightComp = nil
            if self._skillLightEntityId then
                skillPointLightComp = GetComponent(self._skillLightEntityId, "PointLightComponent")
            end
            
            if skillPointLightComp then
                skillPointLightComp.diffuse.x = self.SkillLightCastedDiffuseR / 255.0
                skillPointLightComp.diffuse.y = self.SkillLightCastedDiffuseG / 255.0
                skillPointLightComp.diffuse.z = self.SkillLightCastedDiffuseB / 255.0
            end

            currentRotationSpeed = self.LaunchRotationSpeed

            local moveStep = self.ProjectileSpeed * dt
            local pos = transform.localPosition

            pos.x = pos.x + (fwdX * moveStep)
            pos.y = pos.y + (fwdY * moveStep)
            pos.z = pos.z + (fwdZ * moveStep)

            transform.localPosition = pos
        end

        if self._state ~= 0 then
            self._currentSpinAngle = (self._currentSpinAngle + currentRotationSpeed * dt) % 360
            local spinQuat = eulerToQuat(0, 0, self._currentSpinAngle)
            local parentNewRot = multiplyQuat(self._baseRotation, spinQuat)
            
            transform.localRotation.w = parentNewRot.w
            transform.localRotation.x = parentNewRot.x
            transform.localRotation.y = parentNewRot.y
            transform.localRotation.z = parentNewRot.z
        end
        
        transform.isDirty = true

        if self._state > 0 and self._feathersSpawned >= self.NumFeathersToSpawn then
            local anyFeathersAlive = false
            
            for _, child in ipairs(self._childFeathers) do
                if GetComponent(child.entityId, "Transform") then
                    anyFeathersAlive = true
                    break
                end
            end

            if not anyFeathersAlive then
                if Engine and Engine.DestroyEntity then
                    print("[FeatherSkillManager] All feathers destroyed. Cleaning up parent entity: " .. tostring(self.entityId))
                    Engine.DestroyEntity(self.entityId)
                end
            end
        end
    end,

    OnDisable = function(self)

    end,
}