local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

-- Helper function to create a rotation quaternion from Euler angles (degrees)
local function eulerToQuat(pitch, yaw, roll)
    -- Convert degrees to radians
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

--TEMP SCRIPT -> TODO: CHANGE TO CREATE AND DESTROY ENTITY 
return Component {
    mixins = { TransformMixin },
    
    fields = {
        StartRot = 40,        -- Starting angle offset (relative to player)
        EndRot = -100,          -- Ending angle offset (relative to player)
        Speed = 500,           -- Degrees per second sweep speed
    },
    
    Start = function(self)
        local playerEntityId = Engine.GetEntityByName("Player")
        if not playerEntityId then
            print("[SlashVFX] ERROR: Player entity not found!")
            return
        end

        -- Store transform for later use
        self._playerTransform = GetComponent(playerEntityId, "Transform")
        self._transform = self:GetComponent("Transform")
        self.model = self:GetComponent("ModelRenderComponent")
        
        self.active = false
        self.age = 0
        self._playerYawDegrees = 0  -- Store player's yaw in degrees when slash triggers
        
        if self.model then
            ModelRenderComponent.SetVisible(self.model, false)
        end
        -- Subscribe to attack events
        if event_bus then
            event_bus.subscribe("attack_performed", function(data)
                if data.state == "light_2" then
                    self:TriggerSlash(self.StartRot, self.EndRot, self.Speed)   -- StartRot, EndRot, Speed (deg/s)
                end
            end)
            print("[SlashVFX] Subscribed to attack events")
        end
    end,
    
Update = function(self, dt)
    if self.active then
        self.age = self.age + dt
        
        -- Calculate how far we've swept based on speed (degrees per second)
        local sweptAngle = self._currentSpeed * self.age
        
        -- Check if we've completed the sweep (use absolute values)
        if math.abs(sweptAngle) >= self._arcLength then
            self.active = false
            if self.model then
                ModelRenderComponent.SetVisible(self.model, false)
            end
            print("[SlashVFX] Slash complete - Final angle: " .. (self._currentStartRot + sweptAngle) .. "°")
            return  -- Exit early to prevent further updates
        end
        
        -- Clamp to the total arc length (preserve direction)
        if sweptAngle > 0 then
            sweptAngle = math.min(sweptAngle, self._arcLength)
        else
            sweptAngle = math.max(sweptAngle, -self._arcLength)
        end
        
        -- Current rotation angle = player's yaw + start offset + swept angle
        local currentAngleDegrees = self._playerYawDegrees + self._currentStartRot + sweptAngle
        
        -- Convert to quaternion using eulerToQuat (pitch=0, yaw=angle, roll=0)
        local quat = eulerToQuat(0, currentAngleDegrees, 0)
        
        -- Apply rotation
        self:SetRotation(quat.w, quat.x, quat.y, quat.z)
    end
end,
    
    TriggerSlash = function(self, startRot, endRot, speed)
        if not self._playerTransform then
            print("[SlashVFX] ERROR: Player transform not found!")
            return
        end
        
        -- Store the custom parameters
        self._currentStartRot = startRot or self.StartRot
        local targetEndRot = endRot or self.EndRot
        self._currentSpeed = speed or self.Speed  -- degrees per second
        
        -- Calculate total arc length (can be negative for reverse sweep)
        self._arcLength = math.abs(targetEndRot - self._currentStartRot)
        
        -- Adjust speed direction based on sweep direction
        if targetEndRot < self._currentStartRot then
            self._currentSpeed = -math.abs(self._currentSpeed)  -- Negative for reverse sweep
        else
            self._currentSpeed = math.abs(self._currentSpeed)   -- Positive for forward sweep
        end
        
        -- Get player position
        local playerX = self._playerTransform.localPosition.x
        local playerY = self._playerTransform.localPosition.y
        local playerZ = self._playerTransform.localPosition.z
        
        -- Get player rotation
        local playerRot = self._playerTransform.localRotation
        
        -- Extract player's Y rotation (yaw) from quaternion and convert to degrees
        local playerYawRadians = math.atan(2.0 * (playerRot.w * playerRot.y + playerRot.x * playerRot.z),
                                           1.0 - 2.0 * (playerRot.y * playerRot.y + playerRot.z * playerRot.z))
        
        -- Store player yaw in degrees for the Update function
        self._playerYawDegrees = math.deg(playerYawRadians)
        
        -- Calculate forward direction using player's yaw
        local forwardX = math.sin(playerYawRadians)
        local forwardZ = math.cos(playerYawRadians)
        
        -- Offset distance and height
        local offsetDistance = 1.0
        local offsetHeight = 0.5
        
        -- Position in front of player
        self._transform.localPosition.x = playerX + (forwardX * offsetDistance)
        self._transform.localPosition.y = playerY + offsetHeight
        self._transform.localPosition.z = playerZ + (forwardZ * offsetDistance)
        
        -- Activate the slash
        self.active = true
        self.age = 0
        
        if self.model then
            ModelRenderComponent.SetVisible(self.model, true)
        end
        
        local duration = self._arcLength / math.abs(self._currentSpeed)
        print("[SlashVFX] Slash triggered: " .. self._currentStartRot .. "° → " .. targetEndRot .. "° at " .. math.abs(self._currentSpeed) .. "°/s (duration: " .. string.format("%.2f", duration) .. "s)")
    end,
}