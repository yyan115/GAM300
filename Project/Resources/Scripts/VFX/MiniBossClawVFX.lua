require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },
    
    fields = {
        Lifetime = 0.35,
        BaseScale = 3.5
    },
    
    Start = function(self)
        self.model = self:GetComponent("ModelRenderComponent")
        self.active = false
        self.timer = 0
        
        -- Start hidden
        if self.model then self.model.isVisible = false end

        -- Listen for the boss event
        if _G.event_bus then
            self._slashSub = _G.event_bus.subscribe("miniboss_vfx", function(data)
                self:OnEventReceived(data)
            end)
        end
    end,

    OnEventReceived = function(self, data)
        -- Since there's only one instance, it just restarts 
        -- even if it was already playing from a previous hit
        self._pendingData = data
        self._MiniBossAnim = GetComponent(data.entityId, "AnimationComponent")

        -- Immediate setup
        self.active = true
        self.timer = 0
        self:ActivateSlash(data)
    end,

    ActivateSlash = function(self, data)
        -- Extract from event bus 'pos' and 'rot' tables
        local px, py, pz = data.pos.x, data.pos.y, data.pos.z
        local rw, rx, ry, rz = data.rot.w, data.rot.x, data.rot.y, data.rot.z

        -- Position the VFX at the boss snapshot
        self:SetPosition(px, py, pz)
        self:SetRotation(rw, rx, ry, rz)
        self:SetScale(self.BaseScale, self.BaseScale, self.BaseScale)

        -- Show the mesh
        if self.model then self.model.isVisible = true end
    end,

    Update = function(self, dt)
        if not self.active then return end
        local dtSec = dt or 0

        if self._MiniBossAnim then
            local currentState = self._MiniBossAnim:GetCurrentState()
            if currentState ~= "Melee Attack" then
                self.active = false
                if self.model then self.model.isVisible = false end
                return
            end
        end

        -- 2. Lifetime countdown
        self.timer = self.timer + dtSec
        if self.timer >= self.Lifetime then
            self.active = false
            if self.model then self.model.isVisible = false end
        end
    end,

    OnDisable = function(self)
        if _G.event_bus and self._slashSub then
            _G.event_bus.unsubscribe(self._slashSub)
        end
    end
}