require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")


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


return Component {
    mixins = { TransformMixin },
    
    fields = {
        Lifetime                = 5, --FAILSAFE,
        ForwardOffset           = 0.8,
        HeightOffset            = 1.0,
        TriggerNormalizedTime   = 0.45, -- tune this to match strike frame
        StartRotation           = -40,
        EndRotation             = -110,
        RotationSpeed           = 1000
    },

    Start = function(self)
        self.model = self:GetComponent("ModelRenderComponent")
        self.active         = false
        self.slashTriggered = false
        self.timer          = 0
        self.totalDegreesTurned = 0
        if self.model then self.model.isVisible = false end

        if _G.event_bus then
            self._slashSub = _G.event_bus.subscribe("onClawSlashTrigger", function(data)
                self:OnEventReceived(data)
            end)
        end
    end,

    OnEventReceived = function(self, data)
        if self.active or data.claimed then return end

        data.claimed        = true
        self.pendingData    = data
        self.active         = true
        self.slashTriggered = false
        self.timer          = 0

        self._enemyAnim = GetComponent(data.entityId, "AnimationComponent")
        self._enemyTransform = Engine.FindTransformByID(data.entityId)

        if self.model then self.model.isVisible = false end
    end,

    ActivateSlash = function(self)
    -- Query live position/rotation at the moment of impact
        local pos            = self._enemyTransform.localPosition
        local rot            = self._enemyTransform.localRotation

        local px, py, pz     = pos.x, pos.y, pos.z
        local q1 = { w = rot.w, x = rot.x, y = rot.y, z = rot.z }

        --flip roty 180
        local flipQuat = eulerToQuat(self.StartRotation,180,0)
        local finalQuat = multiplyQuat(q1, flipQuat)


        local fx = 2 * (q1.x * q1.z + q1.w * q1.y)
        local fy = 2 * (q1.y * q1.z - q1.w * q1.x)
        local fz = 1 - 2 * (q1.x * q1.x + q1.y * q1.y)

        self:SetPosition(
            px + (fx * self.ForwardOffset),
            py + (fy * self.ForwardOffset) + self.HeightOffset,
            pz + (fz * self.ForwardOffset)
        )
        self:SetRotation(finalQuat.w, finalQuat.x, finalQuat.y, finalQuat.z)

        self.timer = 0
        self.slashTriggered = true
        if self.model then self.model.isVisible = true end
    end,

    Update = function(self, dt)
        if not self.active or not self._enemyAnim then return end
        local dtSec = dt or 0

        if not self.slashTriggered then
            -- ... (Your existing Trigger logic)
            local anim = self._enemyAnim
            if anim:GetCurrentState() == "Melee Attack" and anim:GetNormalizedTime() >= self.TriggerNormalizedTime then
                self:ActivateSlash()
            end
        else
            -- HANDLE ROTATION
            if not self.rotationEnded then
                -- Calculate how much we want to turn this frame
                local amountToTurn = self.RotationSpeed * dtSec
                
                -- Check if this turn would put us over the limit
                if (self.totalDegreesTurned + amountToTurn) >= -self.EndRotation then
                    -- Cap it to exactly the remainder
                    amountToTurn = -self.EndRotation - self.totalDegreesTurned
                    self.rotationEnded = true
                end

                -- Apply the rotation
                local qW, qX, qY, qZ  = self:GetRotation()
                local currentRot = { w = qW, x = qX, y = qY, z = qZ }

                local stepQuat = eulerToQuat(-amountToTurn, 0, 0)
                local finalQuat = multiplyQuat(currentRot, stepQuat)
                
                self:SetRotation(finalQuat.w, finalQuat.x, finalQuat.y, finalQuat.z)
                
                self.totalDegreesTurned = self.totalDegreesTurned + amountToTurn
            else    --Rotation Ended
                self:Deactivate()   
            end

            -- 2. LIFETIME LOGIC, FAILSAFE, THIS SHOULDNT HAPPEN THOUGH (Rotation should end first)
            self.timer = self.timer + dtSec
            if self.timer >= self.Lifetime then
                self:Deactivate()
            end
        end
    end,

    Deactivate = function(self)
        self.active = false
        self.slashTriggered = false
        self.rotationEnded = false
        self.totalDegreesTurned = 0
        if self.model then self.model.isVisible = false end
    end,
}