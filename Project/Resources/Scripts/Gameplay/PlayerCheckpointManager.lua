require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

return Component {
    fields = {
        checkpointEntityName = "Checkpoint1",
        tooltipEntityName    = "checkpointTooltip",
        CheckpointRadius     = 50.0,
    },

    Start = function(self)
        self._activated = false
        self._tooltipShown = false

        -- Find player
        self._playerEnt = Engine.GetEntityByName("Player")
        self._playerTr  = self._playerEnt and GetComponent(self._playerEnt, "Transform") or nil

        -- Find checkpoint
        self._cpEnt = Engine.GetEntityByName(self.checkpointEntityName)
        self._cpTr  = self._cpEnt and GetComponent(self._cpEnt, "Transform") or nil

        -- Find tooltip and cache sprite (same pattern as TutorialPrompts)
        self._tooltipEnt    = Engine.GetEntityByName(self.tooltipEntityName)
        self._tooltipSprite = nil
        if self._tooltipEnt then
            local ac = GetComponent(self._tooltipEnt, "ActiveComponent")
            if ac then ac.isActive = true end
            self._tooltipSprite = GetComponent(self._tooltipEnt, "SpriteRenderComponent")
            if self._tooltipSprite then
                self._tooltipSprite.alpha     = 0.0
                self._tooltipSprite.isVisible = false
            end
            if ac then ac.isActive = false end
        end
    end,

    Update = function(self, dt)
        if not self._playerTr or not self._cpTr then return end

        local pp = self._playerTr.worldPosition
        local cp = self._cpTr.worldPosition
        if not pp or not cp then return end

        local dx, dz = pp.x - cp.x, pp.z - cp.z
        local dist = math.sqrt(dx*dx + dz*dz)
        local isNear = dist <= self.CheckpointRadius

        if isNear then
            -- Show tooltip
            if not self._tooltipShown and self._tooltipEnt then
                local ac = GetComponent(self._tooltipEnt, "ActiveComponent")
                if ac then ac.isActive = true end
                if self._tooltipSprite then
                    self._tooltipSprite.isVisible = true
                    self._tooltipSprite.alpha = 1.0
                end
                self._tooltipShown = true
            end

            -- Auto-activate checkpoint (once)
            if not self._activated then
                self._activated = true
                self:_activateCheckpoint()
            end
        else
            -- Hide tooltip when player leaves radius
            if self._tooltipShown and self._tooltipEnt then
                local ac = GetComponent(self._tooltipEnt, "ActiveComponent")
                if ac then ac.isActive = false end
                if self._tooltipSprite then
                    self._tooltipSprite.isVisible = false
                    self._tooltipSprite.alpha = 0.0
                end
                self._tooltipShown = false
            end
        end
    end,

    _activateCheckpoint = function(self)
        if not self._cpEnt then return end
        local children = Engine.GetChildrenEntities(self._cpEnt)

        if children[3] then
            local tr = GetComponent(children[3], "Transform")
            if tr then
                self._respawnPos = tr.worldPosition
            end
            if event_bus and event_bus.publish then
                event_bus.publish("activatedCheckpoint", children[3])
                event_bus.publish("playerHeal", 5)
            end
        end

        if children[2] then
            local lc = GetComponent(children[2], "SpotLightComponent")
            if lc then lc.enabled = true end
        end

        local audio = GetComponent(self._cpEnt, "AudioComponent")
        if audio then audio:Play() end
    end,

    OnDisable = function(self)
        if self._tooltipShown and self._tooltipEnt then
            local ac = GetComponent(self._tooltipEnt, "ActiveComponent")
            if ac then ac.isActive = false end
            self._tooltipShown = false
        end
    end,
}
