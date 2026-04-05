require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    fields = {
        AOEExplosionDmg = 10.0,
        AOEExplosionRadius = 5.0,
        AOEExplosionKnockback = 50.0,
        FlashDuration = 0.2,
        TargetBloomIntensity = 10.0,
        TargetLightIntensity = 1.0,
    },

    Awake = function(self)

    end,

    Start = function(self)
        self._bloomComp = self:GetComponent("BloomComponent")
        self._pointLightComp = self:GetComponent("PointLightComponent")
        -- Capture the initial bloom to ensure a smooth transition
        if self._bloomComp then
            self._initialBloom = self._bloomComp.bloomIntensity or 1.0
        else
            self._initialBloom = 1.0
        end

        -- Capture the initial light intensity to ensure a smooth transition
        if self._pointLightComp then
            self._initialLightIntensity = self._pointLightComp.intensity or 1.0
        else
            self._initialLightIntensity = 0.0
        end

        self._transform = self:GetComponent("Transform")
        self._transform.localScale.x = self.AOEExplosionRadius
        self._transform.localScale.y = self.AOEExplosionRadius
        self._transform.localScale.z = self.AOEExplosionRadius
        self._transform.isDirty = true

        self._flashDuration = self.FlashDuration
    end,

    Update = function(self, dt)
        self._flashDuration = self._flashDuration - dt

        -- Calculate the flashing effect
        self._bloomComp = self:GetComponent("BloomComponent")
        self._pointLightComp = self:GetComponent("PointLightComponent")
        if self._bloomComp and self._pointLightComp and self.FlashDuration > 0 then
            local elapsed = self.FlashDuration - self._flashDuration
            local halfDuration = self.FlashDuration * 0.5

            if elapsed <= halfDuration then
                -- First half: Lerp from Initial to Target
                local t = elapsed / halfDuration
                self._bloomComp.bloomIntensity = self._initialBloom + (self.TargetBloomIntensity - self._initialBloom) * t
                self._pointLightComp.intensity = self._initialLightIntensity + (self.TargetLightIntensity - self._initialLightIntensity) * t
            else
                -- Second half: Lerp from Target down to 1.0
                local t = (elapsed - halfDuration) / halfDuration
                -- Clamp t to 1.0 just in case of frame hitches right before destruction
                if t > 1.0 then t = 1.0 end 
                self._bloomComp.bloomIntensity = self.TargetBloomIntensity + (1.0 - self.TargetBloomIntensity) * t
                self._pointLightComp.intensity = self.TargetLightIntensity + (1.0 - self.TargetLightIntensity) * t
            end
        end

        if self._flashDuration <= 0.0 then
            Engine.DestroyEntity(self.entityId)
        end
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
        local rootId = self:_toRoot(otherEntityId)
        local name = Engine.GetEntityName(otherEntityId)
        --print(string.format("Collided with entity entity %s", name))
        local tagComp = GetComponent(rootId, "TagComponent")
        
        if tagComp and Tag.Compare(tagComp.tagIndex, "Player") then
            --print("ONTRIGGERENTER")
            -- Trigger the damage logic via Event Bus
            local x,y,z = self:GetPosition()
            if _G.event_bus and _G.event_bus.publish then
                _G.event_bus.publish("boss_rain_explosives", {
                    entityId = self.entityId,
                    x=x,y=y,z=z,
                    radius = self.AOEExplosionRadius or 4.0,
                    dmg = self.AOEExplosionDmg or 2,
                    kb = self.AOEExplosionKnockback or 240.0,
                })
            end
        end
    end,

    OnDisable = function(self)

    end,
}