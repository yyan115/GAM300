require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    fields = {
        FeatherSkillCooldown = 3.0,
    },

    Awake = function(self)
        self._activatedFeatherSkill = false

        if event_bus and event_bus.subscribe then
            if self._activatedFeatherSkillSub then return end

            self._activatedFeatherSkillSub = event_bus.subscribe("activated_feather_skill", function(payload)
                if payload then
                    self._activatedFeatherSkill = true
                end
            end)
        end
    end,

    Start = function(self)
        self._spriteRender = self:GetComponent("SpriteRenderComponent")
        self._spriteRender.fillValue = 1.0

        self._featherSkillCooldown = 0
    end,

    Update = function(self, dt)
        if self._activatedFeatherSkill then
            self._featherSkillCooldown = self.FeatherSkillCooldown
            self._activatedFeatherSkill = false
        end

        if self._featherSkillCooldown > 0 then
            self._featherSkillCooldown = self._featherSkillCooldown - dt
            if self._featherSkillCooldown <= 0 then
                self._featherSkillCooldown = 0
            end

            self._spriteRender.fillValue = 1.0 - (self._featherSkillCooldown / self.FeatherSkillCooldown)
        end
    end,
}
