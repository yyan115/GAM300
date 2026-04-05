local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

-- Helper: Create Quat from Euler
local function eulerToQuat(pitch, yaw, roll)
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

-- Helper: Multiply two Quaternions (Combines rotations)
local function multiplyQuats(q1, q2)
    return {
        w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z,
        x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
        y = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x,
        z = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w
    }
end

return Component {
    mixins = { TransformMixin },
    
    fields = {
        StartRot = 60,         -- Adjust Start Rotation
        EndRot = -200,         -- Adjust End Rotation
        RollOffset = 0,        -- Left/Right tilt
        PitchOffset = 0,       -- Front/Back tilt
        OffsetHeight = 0,      -- Adjust OffsetHeight for dagger starting height
        OffsetDistance = -0.2, -- Adjust how far VFX spawn based on player position
        SideOffset = 0,        -- Adjust VFX Spawn (+ve for Right, -ve for Left)
        Speed = 750,
        SpawnTime = 0.14,      -- Threshold for triggering (Normalized Time)
        AttackState = "NA3",
    },
    
    Start = function(self)
        local playerEntityId = Engine.GetEntityByName("Player")
        local daggerEntityId = Engine.GetEntityByName("LowPolyFeatherChain")

        if not playerEntityId or not daggerEntityId then
            --print("[SlashVFX] ERROR: Player or Dagger not found!")
            return
        end

        -- Component Lookups
        self._playerTransform = GetComponent(playerEntityId, "Transform")
        self._daggerTransform = GetComponent(daggerEntityId, "Transform")
        self._transform = self:GetComponent("Transform")
        self._playerAnimation = GetComponent(playerEntityId, "AnimationComponent")
        self.model = self:GetComponent("ModelRenderComponent")
        
        -- Runtime State
        self.active = false
        self._hasTriggered = false
        self._playerYawQuat = {w=1, x=0, y=0, z=0}

        -- Initial Visibility
        if self.model then 
            ModelRenderComponent.SetVisible(self.model, false) 
        end
    end,
    
    Update = function(self, dt)
        local currentState = self._playerAnimation:GetCurrentState()
        local cleanAttackState = self.AttackState:gsub('"', '')
        local isAttacking = currentState == cleanAttackState
        
        local normalizedTime = self._playerAnimation:GetNormalizedTime()

        if isAttacking then
            if normalizedTime >= self.SpawnTime and not self._hasTriggered then
                self:TriggerSlash(self.StartRot, self.EndRot, self.Speed)
                self._hasTriggered = true
            end

            if normalizedTime < self.SpawnTime * 0.5 then
                self._hasTriggered = false
            end
        else
            self._hasTriggered = false
        end
        
        -- VFX Sweep Logic
        if self.active then
            self.age = self.age + dt
            local sweptAngle = self._currentSpeed * self.age
            
            if math.abs(sweptAngle) >= self._arcLength then
                self.active = false
                if self.model then ModelRenderComponent.SetVisible(self.model, false) end
                return  
            end
            
            local localYaw = self._currentStartRot + sweptAngle

            local sweepQuat      = eulerToQuat(0, localYaw, 0)
            local tiltQuat       = eulerToQuat(self.PitchOffset, 0, self.RollOffset)
            local localSlashQuat = multiplyQuats(tiltQuat, sweepQuat)
            local finalQuat      = multiplyQuats(self._playerYawQuat, localSlashQuat)

            self:SetRotation(finalQuat.w, finalQuat.x, finalQuat.y, finalQuat.z)
        end
    end,

    TriggerSlash = function(self, startRot, endRot, speed)
        self._currentStartRot = startRot or self.StartRot
        local targetEndRot = endRot or self.EndRot
        self._currentSpeed = speed or self.Speed
        self._arcLength = math.abs(targetEndRot - self._currentStartRot)
        
        if targetEndRot < self._currentStartRot then
            self._currentSpeed = -math.abs(self._currentSpeed)
        else
            self._currentSpeed = math.abs(self._currentSpeed)
        end
        
        local playerRot = self._playerTransform.localRotation
        local q = playerRot

        -- Derive forward and right vectors directly from quaternion
        local forwardX = 2 * (q.x * q.z + q.w * q.y)
        local forwardZ = 1 - 2 * (q.x * q.x + q.y * q.y)

        local rightX = 1 - 2 * (q.y * q.y + q.z * q.z)
        local rightZ = 2 * (q.x * q.z - q.w * q.y)

        -- Spawn position using correct world-space vectors
        local forward = self.OffsetDistance
        local side = self.SideOffset or 0

        local combinedOffsetX = (forwardX * forward) - (rightX * side)
        local combinedOffsetZ = (forwardZ * forward) - (rightZ * side)

        -- Yaw for sweep rotation derived from actual forward vector
        local playerYawRad = math.atan(forwardX, forwardZ)
        self._playerYawQuat = eulerToQuat(0, math.deg(playerYawRad), 0)

        local daggerPos = self._daggerTransform.worldPosition
        local playerScale = self._playerTransform.localScale

        self._transform.localPosition.x = (daggerPos.x + combinedOffsetX) / playerScale.x
        self._transform.localPosition.y = (daggerPos.y + self.OffsetHeight) / playerScale.y
        self._transform.localPosition.z = (daggerPos.z + combinedOffsetZ) / playerScale.z
        
        self.active = true
        self.age = 0
        
        if self.model then
            ModelRenderComponent.SetVisible(self.model, true)
        end
    end,
}