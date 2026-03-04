require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

-- Helper function to convert Euler angles (degrees) to a Quaternion
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
        PlayerHandEntityName = "Kusane_Player_LeftHand",

        FeatherSkillCooldown = 10.0,
        
        SpawnPitchOffset = 0.0,
        SpawnYawOffset = 0.0,
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
        self._transform = self:GetComponent("Transform")
        self._playerHandEntity = Engine.GetEntityByName(self.PlayerHandEntityName)
        self._featherSkillCooldown = 0
    end,

    Update = function(self, dt) 
        if self._featherSkillCooldown > 0 then
            self._featherSkillCooldown = self._featherSkillCooldown - dt
            if self._featherSkillCooldown <= 0 then
                self._featherSkillCooldown = 0
            end
        end

        if self._numFeathers >= self.FeatherSkillRequirement and self._featherSkillCooldown <= 0 then
            if Input and Input.IsActionPressed and Input.IsActionJustPressed("FeatherSkill") then
                
                local featherSkillPrefab = Prefab.InstantiatePrefab(self.FeatherSkillPrefabPath)

                -- Parent to the Hand so it perfectly follows the casting animation!
                if Engine.SetParentEntity then
                    Engine.SetParentEntity(featherSkillPrefab, self._playerHandEntity)
                end

                local featherSkillPrefabTr = GetComponent(featherSkillPrefab, "Transform")
                featherSkillPrefabTr.localPosition.x = 0
                featherSkillPrefabTr.localPosition.y = 0
                featherSkillPrefabTr.localPosition.z = 0
                
                -- Set local rotation to offsets only (no camera math here)
                local camQuat = eulerToQuat(self.SpawnPitchOffset, self.SpawnYawOffset, 0)
                
                featherSkillPrefabTr.localRotation.w = camQuat.w
                featherSkillPrefabTr.localRotation.x = camQuat.x
                featherSkillPrefabTr.localRotation.y = camQuat.y
                featherSkillPrefabTr.localRotation.z = camQuat.z
                
                featherSkillPrefabTr.isDirty = true
                self._featherSkillCooldown = self.FeatherSkillCooldown
            end
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe then
            if self._featherCollectedSub then
                event_bus.unsubscribe(self._featherCollectedSub)
                self._featherCollectedSub = nil
            end
        end
    end,
}