require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

local function eulerToQuat(pitch, yaw, roll)
    local p = math.rad(pitch or 0) * 0.5
    local y = math.rad(yaw or 0)   * 0.5
    local r = math.rad(roll or 0)  * 0.5

    local sinP, cosP = math.sin(p), math.cos(p)
    local sinY, cosY = math.sin(y), math.cos(y)
    local sinR, cosR = math.sin(r), math.cos(r)

    return {
        w = cosP * cosY * cosR + sinP * sinY * sinR,
        x = sinP * cosY * cosR - cosP * sinY * sinR,
        y = cosP * sinY * cosR + sinP * cosY * sinR,
        z = cosP * cosY * sinR - sinP * sinY * cosR
    }
end

return Component {
    mixins = { TransformMixin },

    fields = {
        FeatherSkillRequirement = 20,
        FeatherSkillPrefabPath = "Resources/Prefabs/FeatherSkill.prefab",

        FeatherSkillCooldown = 10.0,
        SkillCastDuration = 3.0, 
        
        SpawnHeightOffset = 0.6,   
        SpawnForwardOffset = 0.2, 
    },

    Awake = function(self)
        self._numFeathers = 0

        if event_bus and event_bus.subscribe then
            if self._featherCollectedSub then return end

            self._featherCollectedSub = event_bus.subscribe("featherCollected", function(payload)
                if payload then
                    self._numFeathers = self._numFeathers + 1
                end
            end)
        end
    end,

    Start = function(self)
        self._featherSkillCooldown = 0
        self._castingTimer = 0 
    end,

    Update = function(self, dt) 
        if self._castingTimer > 0 then
            self._castingTimer = self._castingTimer - dt
            if self._castingTimer <= 0 then
                self._castingTimer = 0
                _G.player_is_casting_skill = false 
            end
        end

        if self._featherSkillCooldown > 0 then
            self._featherSkillCooldown = self._featherSkillCooldown - dt
            if self._featherSkillCooldown <= 0 then
                self._featherSkillCooldown = 0
            end
        end

        if self._numFeathers >= self.FeatherSkillRequirement and self._featherSkillCooldown <= 0 then
            if Input and Input.IsActionPressed and Input.IsActionJustPressed("FeatherSkill") then
                
                local canCast = not _G.player_is_jumping and 
                                not _G.player_is_rolling and 
                                not _G.player_is_landing and 
                                not _G.player_is_hurt and 
                                not _G.player_is_dead and 
                                not _G.player_is_dashing and 
                                not _G.player_is_attacking and
                                not _G.player_is_frozen and
                                not _G.player_is_casting_skill

                if canCast then
                    _G.player_is_casting_skill = true
                    self._castingTimer = self.SkillCastDuration

                    if event_bus and event_bus.publish then
                        event_bus.publish("force_player_rotation_to_camera", true)
                    end

                    local animator = self:GetComponent("AnimationComponent")
                    if animator then
                        animator:SetTrigger("FeatherSkill")
                    end
                    
                    local featherSkillPrefab = Prefab.InstantiatePrefab(self.FeatherSkillPrefabPath)

                    if Engine.SetParentEntity then
                        Engine.SetParentEntity(featherSkillPrefab, self.entityId)
                    end

                    local featherSkillPrefabTr = GetComponent(featherSkillPrefab, "Transform")
                    if featherSkillPrefabTr then
                        featherSkillPrefabTr.localPosition.x = 0
                        featherSkillPrefabTr.localPosition.y = self.SpawnHeightOffset
                        featherSkillPrefabTr.localPosition.z = self.SpawnForwardOffset
                        
                        featherSkillPrefabTr.localRotation.w = 1.0
                        featherSkillPrefabTr.localRotation.x = 0
                        featherSkillPrefabTr.localRotation.y = 0
                        featherSkillPrefabTr.localRotation.z = 0
                        
                        featherSkillPrefabTr.isDirty = true
                    end
                    self._featherSkillCooldown = self.FeatherSkillCooldown
                end
            end
        end
    end,

    OnDisable = function(self)
        _G.player_is_casting_skill = false
        if event_bus and event_bus.unsubscribe then
            if self._featherCollectedSub then
                event_bus.unsubscribe(self._featherCollectedSub)
                self._featherCollectedSub = nil
            end
        end
    end,
}