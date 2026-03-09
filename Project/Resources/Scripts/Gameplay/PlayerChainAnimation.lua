require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    fields = {

    },

    PlayAimThrowAnimation = function(self)
        local animator = self:GetComponent("AnimationComponent")
        animator:SetBool("IsAimingChain", true)
    end,

    PlayPullChainAnimation = function(self)
        local animator = self:GetComponent("AnimationComponent")
        animator:SetTrigger("PullChain")
    end,

    PlaySlamChainAnimation = function(self)
        local animator = self:GetComponent("AnimationComponent")
        animator:SetTrigger("SlamChain")
    end,

    Awake = function(self)

    end,

    Start = function(self)
        if event_bus and event_bus.subscribe then
            print("[PlayerChainAnimation] Subscribing to chain.aim_throw")
            self._chainAimSub = event_bus.subscribe("chain.aim_throw", function(payload)
                if payload then
                    self:PlayAimThrowAnimation()
                end
            end)

            print("[PlayerChainAnimation] Subscribing to chain.pull_chain")
            self._chainPullSub = event_bus.subscribe("chain.pull_chain", function(payload)
                if payload then
                    self:PlayPullChainAnimation()
                end
            end)

            print("[PlayerChainAnimation] Subscribing to chain.slam_chain")
            self._chainSlamSub = event_bus.subscribe("chain.slam_chain", function(payload)
                if payload then
                    self:PlaySlamChainAnimation()
                end
            end)
        else
            print("[PlayerHealth] ERROR: event_bus not available!")
        end
    end,

    Update = function(self, dt)

    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._chainAimSub then
                pcall(function()
                    _G.event_bus.unsubscribe(self._chainAimSub)
                end)
                self._chainAimSub = nil
            end

            if self._chainPullSub then
                pcall(function()
                    _G.event_bus.unsubscribe(self._chainPullSub)
                end)
                self._chainPullSub = nil
            end

            if self._chainSlamSub then
                pcall(function()
                    _G.event_bus.unsubscribe(self._chainSlamSub)
                end)
                self._chainSlamSub = nil
            end
        end
    end,
}