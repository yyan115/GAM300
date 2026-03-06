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
        StartRot = 60,     --Adjust Start Rotation
        EndRot = -200,     --Adjust End Rotation
        RollOffset = 0,    --Adjust RollOffset for Top - Down Orientation 
        OffsetHeight = 0, --Adjust OffsetHeight for dagger starting height
        OffsetDistance = -0.2, --Adjust how far VFX spawn based on player position
        SideOffset = 0,        --Adjust VFX Spawn (+ve for Right, -ve for Left)
        Speed = 750,
        SpawnTime = 0.14,  -- Threshold for triggering (Normalized Time)
        AttackState = "NA3",
    },
    
    Start = function(self)
        local playerEntityId = Engine.GetEntityByName("Player")
        local daggerEntityId = Engine.GetEntityByName("LowPolyFeatherChain")

        if not playerEntityId or not daggerEntityId then
            print("[SlashVFX] ERROR: Player or Dagger not found!")
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
        self._hasTriggered = false -- The "Gate": Ensures one slash per animation cycle
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
        
        -- Get the value directly from your C++ function
        local normalizedTime = self._playerAnimation:GetNormalizedTime()

        if isAttacking then
            -- 1. Trigger Logic
            -- Check against SpawnTime (was Delay)
            if normalizedTime >= self.SpawnTime and not self._hasTriggered then
                self:TriggerSlash(self.StartRot, self.EndRot, self.Speed)
                self._hasTriggered = true
            end

            -- 2. Loop Reset
            -- If the animation loops back to the start, allow it to trigger again
            if normalizedTime < self.SpawnTime * 0.5 then
                self._hasTriggered = false
            end
        else
            -- Reset when the state ends/changes
            self._hasTriggered = false
        end
        
        -- VFX Sweep Logic
        if self.active then
            self.age = self.age + dt
            local sweptAngle = self._currentSpeed * self.age
            
            -- End the effect once the arc is finished
            if math.abs(sweptAngle) >= self._arcLength then
                self.active = false
                if self.model then ModelRenderComponent.SetVisible(self.model, false) end
                return  
            end
            
            -- Calculate rotation relative to player anchor
            local localYaw = self._currentStartRot + sweptAngle
            local localSlashQuat = eulerToQuat(0, localYaw, self.RollOffset)
            local finalQuat = multiplyQuats(self._playerYawQuat, localSlashQuat)

            self:SetRotation(finalQuat.w, finalQuat.x, finalQuat.y, finalQuat.z)
        end
    end,

TriggerSlash = function(self, startRot, endRot, speed)
        self._currentStartRot = startRot or self.StartRot
        local targetEndRot = endRot or self.EndRot
        self._currentSpeed = speed or self.Speed
        self._arcLength = math.abs(targetEndRot - self._currentStartRot)
        
        -- Set direction based on start/end rotation values
        if targetEndRot < self._currentStartRot then
            self._currentSpeed = -math.abs(self._currentSpeed)
        else
            self._currentSpeed = math.abs(self._currentSpeed)
        end
        
        -- Capture orientation (Anchor) to keep the slash stable while moving
        local playerRot = self._playerTransform.localRotation
        local playerYawRad = math.atan(2.0 * (playerRot.w * playerRot.y + playerRot.x * playerRot.z),
                                       1.0 - 2.0 * (playerRot.y * playerRot.y + playerRot.z * playerRot.z))        
        self._playerYawQuat = eulerToQuat(0, math.deg(playerYawRad), 0)
        
        local daggerPos = self._daggerTransform.worldPosition
        local forward = self.OffsetDistance
        local side = self.SideOffset or 0

        local combinedOffsetX = (math.sin(playerYawRad) * forward) - (math.cos(playerYawRad) * side)
        local combinedOffsetZ = (math.cos(playerYawRad) * forward) + (math.sin(playerYawRad) * side)

        self._transform.localPosition.x = daggerPos.x + combinedOffsetX
        self._transform.localPosition.y = daggerPos.y + self.OffsetHeight
        self._transform.localPosition.z = daggerPos.z + combinedOffsetZ
        
        -- Start sweep state
        self.active = true
        self.age = 0
        
        if self.model then
            ModelRenderComponent.SetVisible(self.model, true)
        end
    end,
}