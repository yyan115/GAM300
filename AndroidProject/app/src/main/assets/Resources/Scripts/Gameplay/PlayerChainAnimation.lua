require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    fields = {},

    -- Animation Triggers
    PlayAimThrowAnimation = function(self)
        local animator = self:GetComponent("AnimationComponent")
        if animator then animator:SetBool("IsAimingChain", true) end
    end,

    PlayThrowChainAnimation = function(self)
        local animator = self:GetComponent("AnimationComponent")
        if animator then 
            animator:SetBool("IsAimingChain", false)
            animator:SetTrigger("ThrowChain")
        end
    end,

    PlayPullChainAnimation = function(self)
        local animator = self:GetComponent("AnimationComponent")
        if animator then animator:SetTrigger("PullChain") end
    end,

    PlaySlamChainAnimation = function(self)
        local animator = self:GetComponent("AnimationComponent")
        if animator then animator:SetTrigger("SlamChain") end
    end,

    PlayRetractChainAnimation = function(self)
        local animator = self:GetComponent("AnimationComponent")
        if animator then animator:SetTrigger("RetractChain") end
    end,

    -- Lifecycle
    Start = function(self)
        if event_bus and event_bus.subscribe then
            -- 1. Aiming / Throwing
            self._chainAimSub   = event_bus.subscribe("chain.aim_camera", function(p) if p and p.active then self:PlayAimThrowAnimation() end end)
            self._chainThrowSub = event_bus.subscribe("chain.throw_chain", function(p) if p then self:PlayThrowChainAnimation() end end)

            -- 2. Pulling/Mashing (WE ADDED THIS)
            -- Both a successful pull AND a restricted attempt trigger the "Pull" animation
            local onPull = function(p) if p then self:PlayRetractChainAnimation() end end
            self._chainPullSub    = event_bus.subscribe("chain.pull_chain", onPull)
            self._chainAttemptSub = event_bus.subscribe("chain.pull_attempt", onPull)

            -- 3. Retracting / Slamming
            self._chainRetractSub = event_bus.subscribe("chain.retract_chain", function(p) if p then self:PlayRetractChainAnimation() end end)
            self._chainSlamSub    = event_bus.subscribe("chain.slam_chain", function(p) if p then self:PlaySlamChainAnimation() end end)
        end
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            local subs = { "_chainAimSub", "_chainThrowSub", "_chainPullSub", "_chainAttemptSub", "_chainRetractSub", "_chainSlamSub" }
            for _, s in ipairs(subs) do
                if self[s] then pcall(function() _G.event_bus.unsubscribe(self[s]) end); self[s] = nil end
            end
        end
    end,
}