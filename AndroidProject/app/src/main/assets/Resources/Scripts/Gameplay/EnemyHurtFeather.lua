require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local AudioHelper = require("extension.audio_helper")
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
        MaxSpawnTorque = 10.0,
        MinSpawnTorque = 2.0,
        TriggerDuration = 0.3,
        SpawnForce = 0.2,
        SpawnForceY = 0.1,
        AliveDuration = 10.0,
        featherDropSFX = {},
    },

    Awake = function(self)
        self._triggerDuration = self.TriggerDuration
        self._isTriggerDisabled = false
        self._aliveDuration = self.AliveDuration
        self._dropSFXPlayed = false
    end,

    -- GetRandomForce = function(self, canBeNegative)
    --     -- Pick a random magnitude between MinSpawnForce and MaxSpawnForce
    --     local magnitude = self.MinSpawnForce + math.random() * (self.MaxSpawnForce - self.MinSpawnForce)

    --     -- Randomly flip the sign if canBeNegative is true
    --     if canBeNegative then
    --         if math.random() < 0.5 then
    --             magnitude = -magnitude
    --         end
    --     end

    --     return magnitude
    -- end,

    GetRandomTorque = function(self)
        -- Pick a random magnitude between MinSpawnTorque and MaxSpawnTorque
        local magnitude = self.MinSpawnTorque + math.random() * (self.MaxSpawnTorque - self.MinSpawnTorque)
        return magnitude
    end,

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        self._rb = self:GetComponent("RigidBodyComponent")
        self._audio = self:GetComponent("AudioComponent")
    end,

    Update = function(self, dt)
        --self._rb:AddTorque(self._torqueX, self._torqueY, self._torqueZ)
        if not self._isTriggerDisabled then
            --print(string.format("self._triggerDuration: %f", self._triggerDuration))
            self._triggerDuration = self._triggerDuration - dt
            if self._triggerDuration <= 0.0 then
                self._rb.isTrigger = false
                self._isTriggerDisabled = true
                --print(string.format("[EnemyHurtFeather] Rb isTrigger set to false - Entity %d", self.entityId))
            end
        end

        self._aliveDuration = self._aliveDuration - dt
        if self._aliveDuration <= 0.0 then
            Engine.DestroyEntity(self.entityId)
            return
        end

        if not self._appliedInitialForce and _G.PendingFeatherData and _G.PendingFeatherData[self.entityId] then
            local data = _G.PendingFeatherData[self.entityId]
            local dirX, dirY, dirZ = data.x, data.y + 0.5, data.z
            
            --print("Feather received Hit Direction via Registry!") -- This should print now

            -- Cleanup to save memory
            _G.PendingFeatherData[self.entityId] = nil

            -- 1. Random Scale (Visual Variety)
            local s = 0.5 + math.random() * 0.7 -- 0.5 to 1.2
            self:SetScale(s, s, s)

            -- [FIX 2] MATH SAFETY CHECK
            local lenSq = dirX*dirX + dirY*dirY + dirZ*dirZ
            if lenSq > 0.0001 and lenSq < 1e10 then
                local len = math.sqrt(lenSq)
                dirX, dirY, dirZ = dirX/len, dirY/len, dirZ/len
            else
                -- Fallback if vector is zero/invalid
                dirX, dirY, dirZ = 0, 1, 0 
            end

            -- Add slight random variance (The "Cone" effect)
            -- Spread
            local spread = 0.4
            dirX = dirX + (math.random() - 0.5) * 0.6
            dirY = dirY + (math.random() - 0.5) * spread
            dirZ = dirZ + (math.random() - 0.5) * 0.6

            -- Apply the Force (increase force depending on scale)
            local forceMult = 2.0 * s;
            self._rb:AddImpulse(dirX * self.SpawnForce * forceMult, dirY * self.SpawnForceY * forceMult, dirZ * self.SpawnForce * forceMult)

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
            
            self._torqueX = self:GetRandomTorque() * forceMult
            self._torqueY = self:GetRandomTorque() * forceMult
            self._torqueZ = self:GetRandomTorque() * forceMult
            self._rb:AddTorque(self._torqueX, self._torqueY, self._torqueZ)
            --print(string.format("[EnemyHurtFeather] Applied torque %f %f %f", self._torqueX, self._torqueY, self._torqueZ))

            self._rb.linearVel.y = 0.000001

            self._appliedInitialForce = true
        end
    end,

    OnCollisionEnter = function(self, otherEntityId)
        if self._dropSFXPlayed then return end
        local otherEntityLayer = Engine.GetEntityLayer(otherEntityId)
        if otherEntityLayer == "Ground" then
            self._dropSFXPlayed = true
            local now = os.clock()
            local last = _G._featherDropSFXTime or 0
            self._audio = self:GetComponent("AudioComponent")
            if self._audio and self.featherDropSFX[1] and (now - last) > 0.3 then
                _G._featherDropSFXTime = now
                AudioHelper.PlayRandomSFX(self._audio, self.featherDropSFX)
            end
        end
    end,

    OnDisable = function(self)

    end,
}