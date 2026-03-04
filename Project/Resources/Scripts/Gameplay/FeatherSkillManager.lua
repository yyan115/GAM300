require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

-- Helper function for linear interpolation
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
        
        self._state = 0 
        self._startSkill = false
        self._feathersSpawned = 0
        
        -- [NEW] Track Camera Yaw so we know where to shoot!
        self._cameraYaw = 180.0
        if event_bus and event_bus.subscribe then
            self._cameraYawSub = event_bus.subscribe("camera_yaw", function(yaw)
                if yaw then self._cameraYaw = yaw end
            end)
        end
    end,

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        
        local r = self._transform.localRotation
        self._baseRotation = { w = r.w, x = r.x, y = r.y, z = r.z }
        self._currentSpinAngle = 0

        self._childFeathers = {}
        
        if Engine and Engine.GetChildrenEntities then
            local childrenIds = Engine.GetChildrenEntities(self.entityId)
            
            for _, childId in ipairs(childrenIds) do
                local childTransform = GetComponent(childId, "Transform")
                if childTransform then
                    local cr = childTransform.localRotation
                    table.insert(self._childFeathers, {
                        transform = childTransform,
                        baseRot = { w = cr.w, x = cr.x, y = cr.y, z = cr.z }
                    })
                end

                local childName = Engine.GetEntityName(childId)
                if childName == self.SkillLightEntityName then
                    local skillPointLightComp = GetComponent(childId, "PointLightComponent")
                    if skillPointLightComp then
                        self._skillPointLightComp = skillPointLightComp
                        self._skillPointLightComp.diffuse.x = self.SkillLightInitialDiffuseR / 255.0
                        self._skillPointLightComp.diffuse.y = self.SkillLightInitialDiffuseG / 255.0
                        self._skillPointLightComp.diffuse.z = self.SkillLightInitialDiffuseB / 255.0
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
        
        if Engine.SetParentEntity then
            Engine.SetParentEntity(newFeatherId, self.entityId)
        end

        local childTransform = GetComponent(newFeatherId, "Transform")
        if childTransform then
            childTransform.localPosition.x = posX
            childTransform.localPosition.y = posY
            childTransform.localPosition.z = posZ

            local zQuat = eulerToQuat(0, 0, angleDeg)
            childTransform.localRotation.w = zQuat.w
            childTransform.localRotation.x = zQuat.x
            childTransform.localRotation.y = zQuat.y
            childTransform.localRotation.z = zQuat.z
            childTransform.isDirty = true

            table.insert(self._childFeathers, {
                entityId = newFeatherId,
                transform = childTransform,
                baseRot = { w = zQuat.w, x = zQuat.x, y = zQuat.y, z = zQuat.z }
            })
        end
    end,

    Update = function(self, dt)
        -- if Input and Input.IsActionPressed and Input.IsActionJustPressed("FeatherSkill") then
        --     self._startSkill = true
        --     if self._skillPointLightComp then
        --         self._skillPointLightComp.enabled = true
        --     end
        -- end

        -- if not self._startSkill then 
        --     if self._skillPointLightComp then
        --         self._skillPointLightComp.enabled = false
        --     end
        --     return 
        -- end

        local currentRotationSpeed = 0

        local bw, bx, by, bz = self._baseRotation.w, self._baseRotation.x, self._baseRotation.y, self._baseRotation.z
        local fwdX = 2 * (bx * bz + bw * by)
        local fwdY = 2 * (by * bz - bw * bx)
        local fwdZ = 1 - 2 * (bx * bx + by * by)

        -- ==========================================
        -- PHASE 0: SPAWNING
        -- ==========================================
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

        -- ==========================================
        -- PHASE 1: CASTING DELAY (Converge & Wait)
        -- ==========================================
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
                child.transform.localRotation.w = newRot.w
                child.transform.localRotation.x = newRot.x
                child.transform.localRotation.y = newRot.y
                child.transform.localRotation.z = newRot.z
                child.transform.isDirty = true
            end

            -- UNPARENTING & FLATTENING LOGIC
            if self._castDelay <= 0.0 then
                local wPos, wRot = getWorldTransform(self.entityId)

                if Engine.SetParentEntity then
                    Engine.SetParentEntity(self.entityId, -1)
                end
                
                self._transform.localPosition.x = wPos.x
                self._transform.localPosition.y = wPos.y
                self._transform.localPosition.z = wPos.z

                -- [THE FIX] Level out the projectile so it shoots perfectly straight
                -- (Pitch = 0, Roll = 0, Yaw = Camera Yaw)
                local launchYaw = self._cameraYaw + 180.0
                local flatQuat = eulerToQuat(0, launchYaw, 0)

                self._transform.localRotation.w = flatQuat.w
                self._transform.localRotation.x = flatQuat.x
                self._transform.localRotation.y = flatQuat.y
                self._transform.localRotation.z = flatQuat.z

                -- Lock in the correct flat forward direction for flight!
                self._baseRotation = { w = flatQuat.w, x = flatQuat.x, y = flatQuat.y, z = flatQuat.z }
                
                self._currentSpinAngle = 0
                self._windupStartPos = { x = wPos.x, y = wPos.y, z = wPos.z }

                self._state = 2
                self._transform.isDirty = true
            end

        -- ==========================================
        -- PHASE 2: WIND-UP (The Jerk Backward)
        -- ==========================================
        elseif self._state == 2 then
            self._windupTimer = self._windupTimer - dt
            currentRotationSpeed = self.LaunchRotationSpeed

            local t = 1.0 - (self._windupTimer / self.WindupDuration)
            if t > 1 then t = 1 end
            if t < 0 then t = 0 end
            
            local easeT = t * t 

            local pos = self._transform.localPosition
            pos.x = self._windupStartPos.x - (fwdX * self.WindupPullbackDistance * easeT)
            pos.y = self._windupStartPos.y - (fwdY * self.WindupPullbackDistance * easeT)
            pos.z = self._windupStartPos.z - (fwdZ * self.WindupPullbackDistance * easeT)
            self._transform.localPosition = pos

            if self._windupTimer <= 0.0 then
                self._state = 3
            end

        -- ==========================================
        -- PHASE 3: LAUNCHED (Fly Forward)
        -- ==========================================
        elseif self._state == 3 then
            if self._skillPointLightComp then
                self._skillPointLightComp.diffuse.x = self.SkillLightCastedDiffuseR / 255.0
                self._skillPointLightComp.diffuse.y = self.SkillLightCastedDiffuseG / 255.0
                self._skillPointLightComp.diffuse.z = self.SkillLightCastedDiffuseB / 255.0
            end

            currentRotationSpeed = self.LaunchRotationSpeed

            local moveStep = self.ProjectileSpeed * dt
            local pos = self._transform.localPosition

            pos.x = pos.x + (fwdX * moveStep)
            pos.y = pos.y + (fwdY * moveStep)
            pos.z = pos.z + (fwdZ * moveStep)

            self._transform.localPosition = pos
        end

        -- ==========================================
        -- PARENT ROTATION LOGIC (Runs in all phases)
        -- ==========================================
        if self._state ~= 0 then
            self._currentSpinAngle = (self._currentSpinAngle + currentRotationSpeed * dt) % 360
            local spinQuat = eulerToQuat(0, 0, self._currentSpinAngle)
            local parentNewRot = multiplyQuat(self._baseRotation, spinQuat)
            
            self._transform.localRotation.w = parentNewRot.w
            self._transform.localRotation.x = parentNewRot.x
            self._transform.localRotation.y = parentNewRot.y
            self._transform.localRotation.z = parentNewRot.z
        end
        
        self._transform.isDirty = true
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe then
            if self._cameraYawSub then
                event_bus.unsubscribe(self._cameraYawSub)
                self._cameraYawSub = nil
            end
        end
    end,
}