-- Camera/camera_input.lua
-- Mouse look (orbit yaw/pitch) and scroll-wheel zoom handling

local utils = require("Camera.camera_utils")
local clamp = utils.clamp

local event_bus = _G.event_bus

local M = {}

-- Read mouse/touch delta and update self._yaw / self._pitch.
-- Also publishes camera_yaw to event_bus and _G.CAMERA_YAW.
function M.updateMouseLook(self, dt)
    if not (Input and Input.GetAxisXY) then
        --print("[CameraFollow] ERROR: Input.GetAxisXY not available")
        return
    end

    local lookX, lookY = Input.GetAxisXY("Look")

    local isAndroid      = self._isAndroid
    local baseSensitivity = self.mouseSensitivity or 0.15
    -- Android touch coords are normalized (0-1); needs much higher sensitivity than pixel deltas
    local sensitivity    = isAndroid and 550.0 or baseSensitivity

    local xoffset = lookX * sensitivity
    local yoffset = lookY * sensitivity

    local shouldLock = self.lockCameraRotation
        or (self._actionModeActive and self.actionModeLockRotation)

    if not shouldLock then
        self._yaw   = self._yaw - xoffset
        self._pitch = clamp(self._pitch + yoffset, self.minPitch or -80.0, self.maxPitch or 80.0)

        -- Keep chain-aim yaw/pitch in sync so mouse look works while aiming
        if self._chainAimYaw then
            self._chainAimYaw   = self._chainAimYaw - xoffset
            self._chainAimPitch = clamp(
                self._chainAimPitch + yoffset,
                self.minPitch or -80.0,
                self.maxPitch or 80.0
            )

            -- Flag that the player is manually aiming so aim assist backs off
            local threshold = self.chainAimManualThreshold or 0.3
            if xoffset * xoffset + yoffset * yoffset > threshold * threshold then
                self._chainAimManualTimer = self.chainAimManualCooldown or 0.4
            end
        end

        -- Track normal (non-action) pitch for smooth transitions
        if not self._actionModeActive then
            self._normalPitch = self._pitch
        end
    end

    -- Expose camera yaw globally for player movement scripts
    _G.CAMERA_YAW = self._yaw
    if event_bus and event_bus.publish then
        event_bus.publish("camera_yaw", self._yaw)
    end
end

-- Handle scroll-wheel zoom; updates self._normalDistance and self.followDistance.
-- Always drains the scroll buffer to prevent delta accumulation during blocked modes.
function M.updateScrollZoom(self)
    if not (Input and Input.GetScrollY) then return end

    local scrollY = Input.GetScrollY()  -- always drain, even if we discard the value
    if scrollY == 0 then return end

    -- Discard during cinematic or chain aim — but the drain above already consumed the delta
    if self._cinematicActive then return end
    if self._chainAimBlend and self._chainAimBlend > 0.0 then return end

    local zoomSpeed = self.zoomSpeed or 1.0
    self._normalDistance = clamp(
        self._normalDistance - scrollY * zoomSpeed,
        self.minZoom or 2.0,
        self.maxZoom or 15.0
    )
    -- Do NOT snap followDistance here — the Update lerp (zoomLerpSpeed)
    -- eases the camera smoothly to the new target distance each frame.
end

return M
