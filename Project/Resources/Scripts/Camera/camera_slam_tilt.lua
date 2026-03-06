-- Camera/camera_slam_tilt.lua
-- Additive pitch-tilt effect for slam attacks.
-- Tilts the camera up (anticipation) then quickly pitches down (impact).
--
-- TRIGGER (event bus — from any script):
--   event_bus.publish("camera_slam", {
--       upPitch        = 8,    -- degrees upward during windup  (optional)
--       downPitch      = -15,  -- degrees downward at impact    (optional)
--       upDuration     = 0.10, -- seconds for windup phase      (optional)
--       downDuration   = 0.15, -- seconds for impact phase      (optional)
--       returnDuration = 0.25, -- seconds to ease back to 0     (optional)
--   })
--   Omitted fields fall back to the defaults set in camera_follow's fields table.

local M = {}

-- Call from camera_follow Awake.  Adds slam-tilt state and subscribes to the event.
function M.init(self)
    self._slamPitchOffset   = 0.0
    self._slamPhase         = nil   -- "up" | "down" | "return" | nil
    self._slamTimer         = 0.0

    -- Per-trigger parameters (overridable per-call)
    self._slamUpPitch        = 0.0
    self._slamDownPitch      = 0.0
    self._slamUpDuration     = 0.0
    self._slamDownDuration   = 0.0
    self._slamReturnDuration = 0.0

    local event_bus = _G.event_bus
    if event_bus and event_bus.subscribe then
        self._slamSub = event_bus.subscribe("camera_slam", function(p)
            M.trigger(self, p)
        end)
    end
end

-- Trigger the slam animation.  payload is optional; missing keys use inspector defaults.
function M.trigger(self, payload)
    local p = payload or {}
    self._slamUpPitch        = p.upPitch        or self.SlamUpPitch        or 8.0
    self._slamDownPitch      = p.downPitch       or self.SlamDownPitch      or -15.0
    self._slamUpDuration     = p.upDuration      or self.SlamUpDuration     or 0.10
    self._slamDownDuration   = p.downDuration    or self.SlamDownDuration   or 0.15
    self._slamReturnDuration = p.returnDuration  or self.SlamReturnDuration or 0.25
    self._slamPhase          = "up"
    self._slamTimer          = 0.0
end

-- Call from camera_follow Update (before computing pitchRad).
-- Returns the additive pitch offset (degrees) to add to self._pitch.
function M.update(self, dt)
    -- Inspector trigger: fire once when toggled on, then reset
    if self.TriggerSlam then
        self.TriggerSlam = false
        M.trigger(self, nil)
    end

    if not self._slamPhase then
        self._slamPitchOffset = 0.0
        return 0.0
    end

    self._slamTimer = self._slamTimer + dt

    if self._slamPhase == "up" then
        local dur = math.max(self._slamUpDuration, 0.001)
        local t   = math.min(self._slamTimer / dur, 1.0)
        -- ease-out quad
        t = 1.0 - (1.0 - t) * (1.0 - t)
        self._slamPitchOffset = self._slamUpPitch * t
        if self._slamTimer >= dur then
            self._slamPhase = "down"
            self._slamTimer = 0.0
        end

    elseif self._slamPhase == "down" then
        local dur = math.max(self._slamDownDuration, 0.001)
        local t   = math.min(self._slamTimer / dur, 1.0)
        -- ease-in quad (fast start for snappy impact)
        t = t * t
        self._slamPitchOffset = self._slamUpPitch + (self._slamDownPitch - self._slamUpPitch) * t
        if self._slamTimer >= dur then
            self._slamPhase = "return"
            self._slamTimer = 0.0
        end

    elseif self._slamPhase == "return" then
        local dur = math.max(self._slamReturnDuration, 0.001)
        local t   = math.min(self._slamTimer / dur, 1.0)
        -- smooth-step ease back to zero
        local st = t * t * (3.0 - 2.0 * t)
        self._slamPitchOffset = self._slamDownPitch * (1.0 - st)
        if self._slamTimer >= dur then
            self._slamPhase       = nil
            self._slamPitchOffset = 0.0
        end
    end

    return self._slamPitchOffset
end

-- Call from camera_follow OnDisable to clean up the subscription.
function M.cleanup(self)
    local event_bus = _G.event_bus
    if event_bus and event_bus.unsubscribe and self._slamSub then
        event_bus.unsubscribe(self._slamSub)
        self._slamSub = nil
    end
end

return M
