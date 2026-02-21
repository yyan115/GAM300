require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

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
        MaxSpawnForce = 5.0,
        MinSpawnForce = 1.0,
        MaxSpawnTorque = 10.0,
        MinSpawnTorque = 2.0,
        TriggerDuration = 0.3,
    },

    Awake = function(self)
        self._transform = self:GetComponent("Transform")
        self._rb = self:GetComponent("RigidBodyComponent")
        self._triggerDuration = self.TriggerDuration
        self._isTriggerDisabled = false
    end,

    GetRandomForce = function(self, canBeNegative)
        -- Pick a random magnitude between MinSpawnForce and MaxSpawnForce
        local magnitude = self.MinSpawnForce + math.random() * (self.MaxSpawnForce - self.MinSpawnForce)

        -- Randomly flip the sign if canBeNegative is true
        if canBeNegative then
            if math.random() < 0.5 then
                magnitude = -magnitude
            end
        end

        return magnitude
    end,

    GetRandomTorque = function(self)
        -- Pick a random magnitude between MinSpawnTorque and MaxSpawnTorque
        local magnitude = self.MinSpawnTorque + math.random() * (self.MaxSpawnTorque - self.MinSpawnTorque)
        return magnitude
    end,

    Start = function(self)
        --math.randomseed(os.clock() * 10000 + self.entityId)
        local forceX = self:GetRandomForce(true)
        local forceY = self:GetRandomForce(false)
        local forceZ = self:GetRandomForce(true)
        self._rb:AddForce(forceX, forceY, forceZ)

        -- 1. RANDOMIZE ROTATION (Corrected)
        local rotX = math.random(0, 360) -- Degrees
        local rotY = math.random(0, 360)
        local rotZ = math.random(0, 360)

        -- Use the helper function
        local q = eulerToQuat(rotX, rotY, rotZ)

        -- Apply valid quaternion
        self:SetRotation(q.w, q.x, q.y, q.z)
        
        -- Flag physics to accept the new rotation (assuming you added the C++ fix)
        self._rb.isTeleporting = true 
        
        self._torqueX = self:GetRandomTorque()
        self._torqueY = self:GetRandomTorque()
        self._torqueZ = self:GetRandomTorque()
        self._rb:AddTorque(self._torqueX, self._torqueY, self._torqueZ)
        print(string.format("[EnemyHurtFeather] Applied torque %f %f %f", self._torqueX, self._torqueY, self._torqueZ))

        self._rb.linearVel.y = 0.000001
    end,

    Update = function(self, dt)
        --self._rb:AddTorque(self._torqueX, self._torqueY, self._torqueZ)
        if not self._isTriggerDisabled then
            self._triggerDuration = self._triggerDuration - dt
            if self._triggerDuration <= 0.0 then
                self._rb.isTrigger = false
                self._isTriggerDisabled = true
            end
        end
    end,

    OnDisable = function(self)

    end,
}