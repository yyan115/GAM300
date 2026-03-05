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

    GreyOutSkillIcon = function(self)
        self._spriteRender.color.x = 69.0 / 255.0
        self._spriteRender.color.y = 69.0 / 255.0
        self._spriteRender.color.z = 69.0 / 255.0
    end,

    SetSkillIconColorToWhite = function(self)
        self._spriteRender.color.x = 1.0
        self._spriteRender.color.y = 1.0
        self._spriteRender.color.z = 1.0
    end,

    Start = function(self)
        self._spriteRender = self:GetComponent("SpriteRenderComponent")
        self._spriteRender.fillValue = 1.0
        self:GreyOutSkillIcon()

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

        if _G._numFeathers < _G._featherSkillRequirement then
            self:GreyOutSkillIcon()
        else
            self:SetSkillIconColorToWhite()
        end
    end,
}
