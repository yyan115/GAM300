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
        StartRot = 60,     --Adjust StartEndRot for Left-Right Orientation
        EndRot = -200,    
        RollOffset = 0,    --Adjust RollOffset for Top - Down Orientation 
        OffsetHeight = 0, --Adjust OffsetHeight for dagger starting height
        OffsetDistance = -0.2, --Adjust where VFX spawn based on player position
        Speed = 750,
        Delay = 0.14,
        AttackState = "NA3",
    },
    
    Start = function(self)
        local playerEntityId = Engine.GetEntityByName("Player")
        local daggerEntityId = Engine.GetEntityByName("LowPolyFeatherChain")

        if not playerEntityId or not daggerEntityId then
            print("[SlashVFX] ERROR: Player or Dagger not found!")
            return
        end

        self._playerTransform = GetComponent(playerEntityId, "Transform")
        self._daggerTransform = GetComponent(daggerEntityId, "Transform")
        self._transform = self:GetComponent("Transform")
        self._playerAnimation = GetComponent(playerEntityId, "AnimationComponent")
        self.model = self:GetComponent("ModelRenderComponent")
        
        self.active = false
        self.delaying = false
        self._wasAttacking = false
        self._playerYawQuat = {w=1, x=0, y=0, z=0} -- Store as Quat to avoid Euler flipping

        if self.model then ModelRenderComponent.SetVisible(self.model, false) end
    end,
    
    Update = function(self, dt)
        local currentState = self._playerAnimation:GetCurrentState()
        local cleanAttackState = self.AttackState:gsub('"', '')
        local isAttacking = currentState == cleanAttackState
        if isAttacking and not self._wasAttacking then
            self:TriggerSlash(self.StartRot, self.EndRot, self.Speed, self.Delay)
        end
        
        self._wasAttacking = isAttacking
        if not isAttacking then return end

        if self.delaying then
            self.delayTimer = self.delayTimer + dt
            if self.delayTimer >= self._currentDelay then
                self.delaying = false
                self.active = true
                self.age = 0
                if self.model then ModelRenderComponent.SetVisible(self.model, true) end                
            end
            return
        end
        
        if self.active then
            self.age = self.age + dt
            local sweptAngle = self._currentSpeed * self.age
            
            if math.abs(sweptAngle) >= self._arcLength then
                self.active = false
                if self.model then ModelRenderComponent.SetVisible(self.model, false) end
                return  
            end
            
            -- 1. Calculate the LOCAL rotation of the slash (relative to a 0-facing player)
            local localYaw = self._currentStartRot + sweptAngle
            local localSlashQuat = eulerToQuat(0, localYaw, self.RollOffset)

            -- 2. Combine the stored Player Rotation with the Local Slash Rotation
            -- This keeps the "up" vector of the slash relative to the player
            local finalQuat = multiplyQuats(self._playerYawQuat, localSlashQuat)

            self:SetRotation(finalQuat.w, finalQuat.x, finalQuat.y, finalQuat.z)
        end
    end,

    TriggerSlash = function(self, startRot, endRot, speed, delay)
        self._currentStartRot = startRot or self.StartRot
        local targetEndRot = endRot or self.EndRot
        self._currentSpeed = speed or self.Speed
        self._currentDelay = delay or self.Delay or 0.0
        self._arcLength = math.abs(targetEndRot - self._currentStartRot)
        
        if targetEndRot < self._currentStartRot then
            self._currentSpeed = -math.abs(self._currentSpeed)
        else
            self._currentSpeed = math.abs(self._currentSpeed)
        end
        
        -- Capture the Player's orientation as a base
        local playerRot = self._playerTransform.localRotation
        
        -- Store the player's current orientation as the "Anchor"
        -- We extract just the Y-axis to keep the slash level with the ground
        local playerYawRad = math.atan(2.0 * (playerRot.w * playerRot.y + playerRot.x * playerRot.z),
                               1.0 - 2.0 * (playerRot.y * playerRot.y + playerRot.z * playerRot.z))        
        -- This is our stable reference point
        self._playerYawQuat = eulerToQuat(0, math.deg(playerYawRad), 0)
        
        -- Position logic
        local daggerPos = self._daggerTransform.worldPosition
        self._transform.localPosition.x = daggerPos.x + (math.sin(playerYawRad) * self.OffsetDistance)           
        self._transform.localPosition.y = daggerPos.y + self.OffsetHeight
        self._transform.localPosition.z = daggerPos.z + (math.cos(playerYawRad) * self.OffsetDistance)
        
        self.delaying = self._currentDelay > 0
        self.delayTimer = 0
        self.active = not self.delaying
        self.age = 0
        
        if self.active and self.model then
            ModelRenderComponent.SetVisible(self.model, true)
        end
    end,
}