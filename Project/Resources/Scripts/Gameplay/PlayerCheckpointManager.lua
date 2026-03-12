require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus
local Input = _G.Input

return Component {
    mixins = { TransformMixin },

    fields = {
        CheckpointRadius = 0.50,

        -- Tooltip entity name (sprite that appears when player is near a checkpoint)
        tooltipEntity = "",
    },

    meta = {
        tooltipEntity = { editorHint = "entity" },
    },

    Awake = function(self)

    end,

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        self._checkpointEntities = Engine.GetEntitiesByTag("Checkpoint")

        -- Tooltip sprite entity
        self._tooltipShown = false
        self._nearCheckpoint = nil        -- entity ID of checkpoint we are near (pending activation)
        if self.tooltipEntity ~= "" then
            self._tooltipEnt = Engine.GetEntityByName(self.tooltipEntity)
        end
    end,

    -------------------------------------------------
    -- Tooltip show/hide helper
    -------------------------------------------------
    _setTooltipVisible = function(self, visible)
        if not self._tooltipEnt then return end

        local activeComp = GetComponent(self._tooltipEnt, "ActiveComponent")
        if activeComp then
            activeComp.isActive = visible
        end

        local spriteComp = GetComponent(self._tooltipEnt, "SpriteRenderComponent")
        if spriteComp then
            spriteComp.isVisible = visible
            spriteComp.alpha = visible and 1.0 or 0.0
        end

        -- Set global flag so combat scripts know to skip attacks
        _G.playerNearInteractable = visible
    end,

    -------------------------------------------------
    -- Activate a checkpoint (called when player presses attack near it)
    -------------------------------------------------
    _activateCheckpoint = function(self, entityId)
        self._activatedCheckpoint = entityId
        local checkpointChildren = Engine.GetChildrenEntities(entityId)

        local respawnPointEnt = checkpointChildren[3]
        local respawnPointTr = GetComponent(respawnPointEnt, "Transform")
        local respawnPos = Engine.GetTransformWorldPosition(respawnPointTr)
        self._respawnPos = respawnPos

        self._lightEnt = checkpointChildren[2]
        local lightComp = GetComponent(self._lightEnt, "SpotLightComponent")
        lightComp.enabled = true

        -- Play audio
        local audio = GetComponent(entityId, "AudioComponent")
        if audio then
            audio:Play()
        end

        if event_bus and event_bus.publish then
            event_bus.publish("activatedCheckpoint", respawnPointEnt)
            event_bus.publish("playerHeal", 5)
        end
    end,

    -------------------------------------------------
    -- Check proximity and handle tooltip + interaction
    -------------------------------------------------
    CheckHitCheckpoint = function(self)
        if not self._transform then return end
        local p = self._transform.worldPosition
        if not p then return false end

        local foundNearCheckpoint = nil

        for i, entityId in ipairs(self._checkpointEntities) do
            local transform = GetComponent(entityId, "Transform")
            local pos = transform.worldPosition

            local dx, dy, dz = p.x - pos.x, p.y - pos.y, p.z - pos.z
            local r = self.CheckpointRadius

            if (dx*dx + dy*dy + dz*dz) <= (r*r) then
                -- Player is near this checkpoint
                -- Only consider it if it hasn't already been activated
                if self._activatedCheckpoint ~= entityId then
                    foundNearCheckpoint = entityId
                end
            else
                -- Turn off lights for non-active checkpoints
                local checkpointChildren = Engine.GetChildrenEntities(entityId)
                local lightEnt = checkpointChildren[2]
                local lightComp = GetComponent(lightEnt, "SpotLightComponent")

                if lightEnt ~= self._lightEnt then
                    lightComp.enabled = false
                end
            end
        end

        -- Auto-activate on collision enter (player enters checkpoint radius)
        if foundNearCheckpoint then
            self:_activateCheckpoint(foundNearCheckpoint)
        end

        return false
    end,

    Update = function(self, dt)
        self:CheckHitCheckpoint()
    end,

    OnDisable = function(self)
        -- Clean up tooltip if visible
        if self._tooltipShown then
            self:_setTooltipVisible(false)
            self._tooltipShown = false
        end
    end,
}