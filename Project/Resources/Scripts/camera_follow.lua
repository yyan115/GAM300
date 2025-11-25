-- Scripts/camera_follow.lua

require("extension.engine_bootstrap")

local Component       = require("extension.mono_helper")
local TransformMixin  = require("extension.transform_mixin")

return Component {
    fields = {
        name       = "CameraFollow",
        targetName = "AgentWithScript",  -- overridden in inspector
        offset     = { x = 0.0, y = 1.0, z = 8.0 },
        lerpSpeed  = 5.0
    },

    mixins = {
        TransformMixin
    },

    Awake = function(self)
        self._targetTransform  = nil
        self._cameraTransform  = nil
        print("[CameraFollow] Awake, entityId =", tostring(self.entityId))
    end,

    Start = function(self)
        self._cameraTransform = self:GetComponent("Transform")
        if not self._cameraTransform then
            print("[CameraFollow] No Transform on camera entity!")
        else
            print("[CameraFollow] Start, camera Transform OK")
        end
        print("[CameraFollow] Start, targetName =", tostring(self.targetName))
        print("[CameraFollow] Engine.FindTransformByName =", tostring(Engine and Engine.FindTransformByName))
    end,

    Update = function(self, dt)
        -- debug: confirm Update is called
        -- (You can comment this out later if spammy)
        -- print("[CameraFollow] Update dt=", dt)

        if (not self._targetTransform) and self.targetName and self.targetName ~= "" then
            print("[CameraFollow] Trying to find target:", self.targetName)
            local tgtT = Engine.FindTransformByName(self.targetName)
            print("[CameraFollow] Engine.FindTransformByName returned:", tgtT)

            if tgtT then
                self._targetTransform = tgtT
                print("[CameraFollow] Now following:", tostring(self.targetName))
            else
                -- Only print occasionally to avoid spam; you can remove if too noisy
                -- print("[CameraFollow] No Transform found for:", tostring(self.targetName))
            end
        end

        if not (self._cameraTransform and self._targetTransform) then
            return
        end

        local camT = self._cameraTransform
        local tgtT = self._targetTransform

        local cx = camT.localPosition.x
        local cy = camT.localPosition.y
        local cz = camT.localPosition.z

        local tx = tgtT.localPosition.x
        local ty = tgtT.localPosition.y
        local tz = tgtT.localPosition.z

        local ox = self.offset.x or 0.0
        local oy = self.offset.y or 0.0
        local oz = self.offset.z or 0.0

        local dx = tx + ox
        local dy = ty + oy
        local dz = tz + oz

        local s = self.lerpSpeed or 0.0
        local t = s > 0.0 and math.min(dt * s, 1.0) or 1.0

        camT.localPosition.x = cx + (dx - cx) * t
        camT.localPosition.y = cy + (dy - cy) * t
        camT.localPosition.z = cz + (dz - cz) * t

        camT.isDirty = true
    end
}