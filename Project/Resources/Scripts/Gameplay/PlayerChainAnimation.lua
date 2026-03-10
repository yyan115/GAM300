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

    PlayThrowChainAnimation = function(self)
        local animator = self:GetComponent("AnimationComponent")
        animator:SetBool("IsAimingChain", false)
        animator:SetTrigger("ThrowChain")
    end,

    PlayPullChainAnimation = function(self)
        local animator = self:GetComponent("AnimationComponent")
        animator:SetTrigger("PullChain")
    end,

    PlaySlamChainAnimation = function(self)
        local animator = self:GetComponent("AnimationComponent")
        animator:SetTrigger("SlamChain")
    end,

    PlayRetractChainAnimation = function(self)
        local animator = self:GetComponent("AnimationComponent")
        animator:SetTrigger("RetractChain")
    end,

    Awake = function(self)

    end,

    Start = function(self)
        if event_bus and event_bus.subscribe then
            -- Prevent double-subscription on the SAME instance
            if self._chainAimSub then return end

            print("[PlayerChainAnimation] Subscribing to chain.aim_camera")
            self._chainAimSub = event_bus.subscribe("chain.aim_camera", function(payload)
                if payload and payload.active then
                    self:PlayAimThrowAnimation()
                end
            end)

            if self._chainThrowSub then return end

            print("[PlayerChainAnimation] Subscribing to chain.throw_chain")
            self._chainThrowSub = event_bus.subscribe("chain.throw_chain", function(payload)
                if payload then
                    self:PlayThrowChainAnimation()
                end
            end)

            if self._chainPullSub then return end

            print("[PlayerChainAnimation] Subscribing to chain.pull_chain")
            self._chainPullSub = event_bus.subscribe("chain.pull_chain", function(payload)
                if payload then
                    self:PlayPullChainAnimation()
                end
            end)

            print("[PlayerChainAnimation] Subscribing to chain.retract_chain")
            self._chainPullSub = event_bus.subscribe("chain.retract_chain", function(payload)
                if payload then
                    self:PlayRetractChainAnimation()
                end
            end)

            if self._chainSlamSub then return end

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