require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus
local Input = _G.Input

return Component {
    mixins = { TransformMixin },

    fields = {
        CheckpointRadius = 0.50
    },

    Awake = function(self)
        
    end,

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        self._checkpointEntities = Engine.GetEntitiesByTag("Checkpoint")
    end,

    CheckHitCheckpoint = function(self)
        if not self._transform then return end
        local p = self._transform.worldPosition
        if not p then return false end

        --print("CheckHitCheckpoint")
        for i, entityId in ipairs(self._checkpointEntities) do
            --print("[PlayerCheckpointManager] Checking entity", entityId)
            local transform = GetComponent(entityId, "Transform")
            local audio = GetComponent(entityId, "AudioComponent")
            local pos = transform.worldPosition

            local kx = pos.x
            local ky = pos.y
            local kz = pos.z

            local dx, dy, dz = p.x - kx, p.y - ky, p.z - kz
            local r = self.CheckpointRadius
            --print("CheckpointRadius:", self.CheckpointRadius)
            if (dx*dx + dy*dy + dz*dz) <= (r*r) then
                -- Only activate if this checkpoint isn't already active
                if self._activatedCheckpoint ~= entityId then
                    self._activatedCheckpoint = entityId
                    local checkpointChildren = Engine.GetChildrenEntities(self._activatedCheckpoint)

                    local respawnPointEnt = checkpointChildren[3]
                    local respawnPointTr = GetComponent(respawnPointEnt, "Transform")
                    local respawnPos = Engine.GetTransformWorldPosition(respawnPointTr)
                    self._respawnPos = respawnPos

                    self._lightEnt = checkpointChildren[2]
                    local lightComp = GetComponent(self._lightEnt, "SpotLightComponent")
                    lightComp.enabled = true

                    -- play audio only once when newly activated
                    if audio then
                        audio:Play()
                    end

                    if event_bus and event_bus.publish then
                        event_bus.publish("activatedCheckpoint", respawnPointEnt)
                    end
                end

                -- If it's already the active checkpoint, do nothing
            else
                local checkpointChildren = Engine.GetChildrenEntities(entityId)
                local lightEnt = checkpointChildren[2]
                local lightComp = GetComponent(lightEnt, "SpotLightComponent")

                if lightEnt ~= self._lightEnt then
                    lightComp.enabled = false
                end
            end
        end

        return false
    end,

    Update = function(self, dt)
        self:CheckHitCheckpoint()
    end,

    OnDisable = function(self)

    end,
}