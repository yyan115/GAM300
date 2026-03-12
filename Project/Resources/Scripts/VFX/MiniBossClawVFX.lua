require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },
    
    fields = {
        Lifetime = 0.35,
        BaseScale = 3.5,
        ForwardOffset = 0.5,
        HeightOffset = 0.0,
        AttackThreshold = 0.42 -- The normalized time to trigger the visual
    },
    
    Start = function(self)
        self.model = self:GetComponent("ModelRenderComponent")
        self.active = false
        self.isWaitingForAnim = false -- New state flag
        self.timer = 0
        
        if self.model then self.model.isVisible = false end

        if _G.event_bus then
            self._slashSub = _G.event_bus.subscribe("miniboss_vfx", function(data)
                self:OnEventReceived(data)
            end)
        end
    end,

    OnEventReceived = function(self, data)
        self._pendingData = data
        self._MiniBossAnim = GetComponent(data.entityId, "AnimationComponent")

        self.active = true
        self.isWaitingForAnim = true -- Start in waiting mode
        self.timer = 0
        
        -- We don't call ActivateSlash here anymore; we let Update handle the timing
    end,

    ActivateSlash = function(self, data)
        local px, py, pz = data.pos.x, data.pos.y, data.pos.z
        local rw, rx, ry, rz = data.rot.w, data.rot.x, data.rot.y, data.rot.z

        -- Calculate Forward Direction
        local fx = 2 * (rx * rz + rw * ry)
        local fy = 2 * (ry * rz - rw * rx)
        local fz = 1 - 2 * (rx * rx + ry * ry)

        -- Apply Offsets
        local finalX = px + (fx * self.ForwardOffset)
        local finalY = py + (fy * self.ForwardOffset) + self.HeightOffset
        local finalZ = pz + (fz * self.ForwardOffset)

        self:SetPosition(finalX, finalY, finalZ)
        self:SetRotation(rw, rx, ry, rz)
        self:SetScale(self.BaseScale, self.BaseScale, self.BaseScale)

        if self.model then self.model.isVisible = true end
    end,

    Update = function(self, dt)
        if not self.active then return end
        local dtSec = dt or 0

        if self._MiniBossAnim then
            local currentState = self._MiniBossAnim:GetCurrentState()
            
            -- Interruption Check
            if currentState ~= "Melee Attack" then
                self:ResetVFX()
                return
            end

            -- Normalized Time Check
            if self.isWaitingForAnim then
                local normTime = self._MiniBossAnim:GetNormalizedTime()
                if normTime >= self.AttackThreshold then
                    self.isWaitingForAnim = false -- Stop waiting
                    self:ActivateSlash(self._pendingData)
                else
                    return -- Keep waiting, don't start lifetime timer yet
                end
            end
        end

        -- Lifetime countdown (only starts after isWaitingForAnim is false)
        self.timer = self.timer + dtSec
        if self.timer >= self.Lifetime then
            self:ResetVFX()
        end
    end,

    ResetVFX = function(self)
        self.active = false
        self.isWaitingForAnim = false
        if self.model then self.model.isVisible = false end
    end,

    OnDisable = function(self)
        if _G.event_bus and self._slashSub then
            _G.event_bus.unsubscribe(self._slashSub)
        end
    end
}