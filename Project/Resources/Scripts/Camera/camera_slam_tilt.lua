-- Camera/camera_slam_tilt.lua
-- Slams the camera down to a fixed ground angle then returns to a set forward angle.
--
-- Down phase  → absolute pitch lerp to SlamGroundAngle regardless of start angle.
-- Return      → lerps from SlamGroundAngle to SlamReturnAngle (default 0 = straight forward).
-- On finish   → writes self._pitch so the camera stays at SlamReturnAngle.
--
-- TRIGGER (event bus):
--   event_bus.publish("camera_slam", {
--       groundAngle    = 60,   -- absolute pitch at impact   (optional)
--       returnAngle    = 0,    -- absolute pitch to settle at (optional)
--       downDuration   = 0.15, -- seconds for impact         (optional)
--       returnDuration = 0.25, -- seconds to ease back       (optional)
--   })

local M = {}

function M.init(self)
    self._slamPhase        = nil
    self._slamTimer        = 0.0
    self._slamAbsPitch     = nil
    self._slamStartPitch   = 0.0

    self._slamGroundAngle    = 0.0
    self._slamReturnAngle    = 0.0
    self._slamDownDuration   = 0.0
    self._slamReturnDuration = 0.0

    local event_bus = _G.event_bus
    if event_bus and event_bus.subscribe then
        self._slamSub = event_bus.subscribe("camera_slam", function(p)
            M.trigger(self, p)
        end)
    end
end

function M.trigger(self, payload)
    local p = payload or {}
    self._slamGroundAngle    = p.groundAngle    or self.SlamGroundAngle    or 60.0
    self._slamReturnAngle    = p.returnAngle    or self.SlamReturnAngle    or 0.0
    self._slamDownDuration   = p.downDuration   or self.SlamDownDuration   or 0.15
    self._slamReturnDuration = p.returnDuration or self.SlamReturnDuration or 0.25
    self._slamStartPitch     = self._pitch or 15.0
    self._slamPhase          = "down"
    self._slamTimer          = 0.0
    self._slamAbsPitch       = self._slamStartPitch
end

-- Returns absPitch (number or nil).
-- camera_follow uses absPitch directly when non-nil, otherwise uses self._pitch.
function M.update(self, dt)
    -- Inspector trigger
    if self.TriggerSlam then
        self.TriggerSlam = false
        M.trigger(self, nil)
    end

    if not self._slamPhase then
        return nil
    end

    self._slamTimer = self._slamTimer + dt

    -- ── Down: snap to ground angle ────────────────────────────────────────────
    if self._slamPhase == "down" then
        local dur = math.max(self._slamDownDuration, 0.001)
        local t   = math.min(self._slamTimer / dur, 1.0)
        t = t * t  -- ease-in: snappy impact
        self._slamAbsPitch = self._slamStartPitch + (self._slamGroundAngle - self._slamStartPitch) * t

        if self._slamTimer >= dur then
            self._slamAbsPitch = self._slamGroundAngle
            self._slamPhase    = "return"
            self._slamTimer    = 0.0
        end

    -- ── Return: ease to forward angle ─────────────────────────────────────────
    elseif self._slamPhase == "return" then
        local dur = math.max(self._slamReturnDuration, 0.001)
        local t   = math.min(self._slamTimer / dur, 1.0)
        local st  = t * t * (3.0 - 2.0 * t)  -- smooth-step
        self._slamAbsPitch = self._slamGroundAngle + (self._slamReturnAngle - self._slamGroundAngle) * st

        if self._slamTimer >= dur then
            self._slamPhase      = nil
            self._slamAbsPitch   = nil
            -- Trigger screen shake once slam finishes
            self._shakeTimer     = 0.0
            self._shakeDuration  = self.ShakeDuration  or 0.4
            self._shakeIntensity = self.ShakeIntensity or 0.3
            self._shakeFrequency = self.ShakeFrequency or 25.0
        end
    end

    return self._slamAbsPitch
end

function M.cleanup(self)
    local event_bus = _G.event_bus
    if event_bus and event_bus.unsubscribe and self._slamSub then
        event_bus.unsubscribe(self._slamSub)
        self._slamSub = nil
    end
end

return M
