require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    fields = {
        AOEExplosionDmg = 10.0,
        AOEExplosionRadius = 5.0,
        FlashDuration = 0.2,
        TargetBloomIntensity = 2.0,
    },

    Awake = function(self)

    end,

    Start = function(self)
        self._bloomComp = self:GetComponent("BloomComponent")
        -- Capture the initial bloom to ensure a smooth transition
        if self._bloomComp then
            self._initialBloom = self._bloomComp.bloomIntensity or 1.0
        else
            self._initialBloom = 1.0
        end

        -- self._transform = self:GetComponent("Transform")
        -- self._transform.localScale.x = self.AOEExplosionRadius
        -- self._transform.localScale.y = self.AOEExplosionRadius
        -- self._transform.localScale.z = self.AOEExplosionRadius

        self._flashDuration = self.FlashDuration
    end,

    Update = function(self, dt)
        self._flashDuration = self._flashDuration - dt

        -- Calculate the flashing effect
        self._bloomComp = self:GetComponent("BloomComponent")
        if self._bloomComp and self.FlashDuration > 0 then
            local elapsed = self.FlashDuration - self._flashDuration
            local halfDuration = self.FlashDuration * 0.5

            if elapsed <= halfDuration then
                -- First half: Lerp from Initial to Target
                local t = elapsed / halfDuration
                self._bloomComp.bloomIntensity = self._initialBloom + (self.TargetBloomIntensity - self._initialBloom) * t
            else
                -- Second half: Lerp from Target down to 1.0
                local t = (elapsed - halfDuration) / halfDuration
                -- Clamp t to 1.0 just in case of frame hitches right before destruction
                if t > 1.0 then t = 1.0 end 
                self._bloomComp.bloomIntensity = self.TargetBloomIntensity + (1.0 - self.TargetBloomIntensity) * t
            end
        end

        if self._flashDuration <= 0.0 then
            Engine.DestroyEntity(self.entityId)
        end
    end,

    OnDisable = function(self)

    end,

    _toRoot = function(self, entityId)
        local targetId = entityId
        if Engine and Engine.GetParentEntity then
            while true do
                local parentId = Engine.GetParentEntity(targetId)
                if not parentId or parentId < 0 then break end
                targetId = parentId
            end
        end
        return targetId
    end,

    OnTriggerEnter = function(self, otherEntityId)
        --print("[FeatherSkillExplosion] ONTRIGGERENTER")
        local rootId = self:_toRoot(otherEntityId)
        local tagComp = GetComponent(rootId, "TagComponent")
        -- The explosion trigger collides with walls, ground, and anything else
        -- in range. Most of those have no TagComponent, so we must nil-check
        -- before touching it or the feather skill crashes on impact.
        if not tagComp then return end

        if Tag.Compare(tagComp.tagIndex, "Enemy") or Tag.Compare(tagComp.tagIndex, "Boss") then
            if event_bus and event_bus.publish then
                event_bus.publish("deal_damage_to_entity", {
                    entityId = rootId,
                    damage   = self.AOEExplosionDmg,
                    hitType  = "FEATHER",
                })
            end
        end
    end,
}