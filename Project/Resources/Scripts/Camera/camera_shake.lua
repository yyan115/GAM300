-- Camera/camera_shake.lua
-- Drives the built-in CameraComponent shake fields.
--
-- SETUP: Attach to the same entity as your CameraComponent.
--
-- TRIGGER (event bus — from any script):
--   event_bus.publish("camera_shake", { intensity=0.4, duration=0.5, frequency=20 })
--   All three fields are optional; omitted ones fall back to this component's defaults.
--
-- TRIGGER (direct Lua call — if you hold a reference to this component):
--   cameraShake:Shake({ intensity=0.4, duration=0.5, frequency=20 })
--   Or with no args to use defaults:
--   cameraShake:Shake()

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local IM        = require("extension.inspector_meta")

return Component {

    fields = {
        -- === Shake Defaults ===
        DefaultIntensity = 0.3,
        DefaultDuration  = 0.4,
        DefaultFrequency = 25.0,
    },

    meta = {
        DefaultIntensity = IM.number(0, 5,   0.01),
        DefaultDuration  = IM.number(0, 10,  0.05),
        DefaultFrequency = IM.number(1, 120, 1),
    },

    _cam       = nil,
    _shakeSub  = nil,

    -- -------------------------------------------------------
    Awake = function(self)
        self._cam = self:GetComponent("CameraComponent")
        if not self._cam then
            if cpp_log then cpp_log("[CameraShake] WARNING: no CameraComponent found on this entity.") end
        end
    end,

    -- -------------------------------------------------------
    Start = function(self)
        self._shakeSub = event_bus.subscribe("camera_shake", function(payload)
            self:Shake(payload)
        end)
    end,

    -- -------------------------------------------------------
    -- Public method: call directly or via event bus.
    -- payload (optional table): { intensity, duration, frequency }
    Shake = function(self, payload)
        local cam = self._cam
        if not cam then return end

        local p = payload or {}
        cam.shakeIntensity = p.intensity or self.DefaultIntensity
        cam.shakeDuration  = p.duration  or self.DefaultDuration
        cam.shakeFrequency = p.frequency or self.DefaultFrequency
        cam.shakeTimer     = 0
        cam.isShaking      = true
    end,

    -- -------------------------------------------------------
    OnDestroy = function(self)
        if self._shakeSub then
            event_bus.unsubscribe(self._shakeSub)
            self._shakeSub = nil
        end
    end,
}
