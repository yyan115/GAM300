require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

-- Math helper to rotate a 3D vector by a Quaternion
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

return Component {
    mixins = { TransformMixin },

    fields = {
        AliveDuration = 3.0,
        
        -- Impulse variables
        SpawnForce = 15.0,  -- The outward thrust (X and Z)
        SpawnForceY = 15.0, -- The upward thrust (Y). Increase this for a higher arc!
        ConeSpread = 0.4,   -- The width/angle of the cone. Higher = wider spread.
    },

    Awake = function(self)

    end,

    Start = function(self)
        self._aliveDuration = self.AliveDuration
    end,

    Update = function(self, dt)
        self._aliveDuration = self._aliveDuration - dt
        if self._aliveDuration <= 0 then
            if Engine and Engine.DestroyEntity then
                Engine.DestroyEntity(self.entityId)
            end
            return
        end

        if not self._appliedInitialForce then
            local transform = self:GetComponent("Transform")
            local rb = self:GetComponent("RigidBodyComponent")
            
            if transform and rb then
                -- 1. Create a random vector pointing mostly forward (-Z) with some X/Y scatter
                local spread = self.ConeSpread
                local localDir = {
                    x = (math.random() * 2.0 - 1.0) * spread,
                    y = (math.random() * 2.0 - 1.0) * spread,
                    z = -1.0 
                }
                
                -- 2. Normalize it so the force isn't stronger when firing diagonally
                local len = math.sqrt(localDir.x^2 + localDir.y^2 + localDir.z^2)
                localDir.x = localDir.x / len
                localDir.y = localDir.y / len
                localDir.z = localDir.z / len
                
                -- 3. Rotate this local vector by the remnant's actual world rotation
                local worldDir = rotateVec(transform.localRotation, localDir)
                
                -- 4. Apply a randomized force variation (e.g., 80% to 120% of base force)
                local forceMult = 0.8 + (math.random() * 0.4)
                
                -- 5. Apply the initial impulse to the Rigidbody!
                rb:AddImpulse(
                    worldDir.x * self.SpawnForce * forceMult, 
                    worldDir.y * self.SpawnForceY * forceMult, 
                    worldDir.z * self.SpawnForce * forceMult
                )
            end

            self._appliedInitialForce = true
        end
    end,

    OnDisable = function(self)

    end,

    OnCollisionEnter = function(self, otherEntityId)
        local otherEntityLayer = Engine.GetEntityLayer(otherEntityId)
        if otherEntityLayer == "Ground" then
            --print("[FeatherSkillRemnant] Collided with ground")
            self._rb = self:GetComponent("RigidBodyComponent")
            self._collider = self:GetComponent("ColliderComponent")

            if self._rb then
                self._rb.motionID = 1 -- Set to Kinematic/Static
                self._rb.motion_dirty = true
                self._rb.isTrigger = true
                self._rb.gravityFactor = 0.0
            end

            if self._collider then
                self._collider.enabled = false
            end
        end
    end,
}