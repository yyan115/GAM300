-- CameraFlythrough.lua
-- Cinematic camera flythrough tool for capturing trailer shots.
--
-- USAGE:
--   1. Create an empty entity, attach this script.
--   2. Add child entities as waypoints (position + rotation).
--      Order = child index (first child = first waypoint).
--   3. Press the ToggleKey to start/stop (F5-F10 for up to 6 shots).
--   4. Camera smoothly flies through all waypoints using Catmull-Rom splines.
--
-- FIELDS (editable in inspector):
--   TotalDuration   – seconds for the entire path
--   Loop            – restart from beginning when done
--   ToggleKey       – key to start/stop (default "F5")
--   PauseKey        – key to pause/resume (default "F6")
--   EaseInOut       – apply smooth acceleration/deceleration

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

-- ── Math helpers ────────────────────────────────────────────────────────────

local function catmullRom(p0, p1, p2, p3, t)
    local t2 = t * t
    local t3 = t2 * t
    return 0.5 * ((2.0 * p1)
        + (-p0 + p2) * t
        + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2
        + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3)
end

local function lerpVal(a, b, t)
    return a + (b - a) * t
end

-- Normalise a quaternion
local function quatNorm(q)
    local len = math.sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z)
    if len < 0.0001 then return {w=1, x=0, y=0, z=0} end
    return { w=q.w/len, x=q.x/len, y=q.y/len, z=q.z/len }
end

-- Spherical lerp for quaternions
local function slerp(a, b, t)
    -- Ensure shortest path
    local dot = a.w*b.w + a.x*b.x + a.y*b.y + a.z*b.z
    local b2 = b
    if dot < 0 then
        b2 = { w=-b.w, x=-b.x, y=-b.y, z=-b.z }
        dot = -dot
    end

    if dot > 0.9995 then
        -- Very close, just lerp
        local r = {
            w = lerpVal(a.w, b2.w, t),
            x = lerpVal(a.x, b2.x, t),
            y = lerpVal(a.y, b2.y, t),
            z = lerpVal(a.z, b2.z, t),
        }
        return quatNorm(r)
    end

    local theta0 = math.acos(dot)
    local theta = theta0 * t
    local sinTheta = math.sin(theta)
    local sinTheta0 = math.sin(theta0)

    local s0 = math.cos(theta) - dot * sinTheta / sinTheta0
    local s1 = sinTheta / sinTheta0

    return quatNorm({
        w = a.w * s0 + b2.w * s1,
        x = a.x * s0 + b2.x * s1,
        y = a.y * s0 + b2.y * s1,
        z = a.z * s0 + b2.z * s1,
    })
end

-- Smooth ease in/out (hermite)
local function smoothstep(t)
    return t * t * (3.0 - 2.0 * t)
end

-- Walk up the hierarchy to get world-space position and rotation
local function getWorldTransform(entityId)
    local wPos = { x=0, y=0, z=0 }
    local wRot = { w=1, x=0, y=0, z=0 }

    local function rotateVec(q, v)
        local tx = 2 * (q.y * v.z - q.z * v.y)
        local ty = 2 * (q.z * v.x - q.x * v.z)
        local tz = 2 * (q.x * v.y - q.y * v.x)
        return {
            x = v.x + q.w * tx + (q.y * tz - q.z * ty),
            y = v.y + q.w * ty + (q.z * tx - q.x * tz),
            z = v.z + q.w * tz + (q.x * ty - q.y * tx)
        }
    end

    local function multiplyQuat(q1, q2)
        return {
            w = q1.w*q2.w - q1.x*q2.x - q1.y*q2.y - q1.z*q2.z,
            x = q1.w*q2.x + q1.x*q2.w + q1.y*q2.z - q1.z*q2.y,
            y = q1.w*q2.y - q1.x*q2.z + q1.y*q2.w + q1.z*q2.x,
            z = q1.w*q2.z + q1.x*q2.y - q1.y*q2.x + q1.z*q2.w
        }
    end

    local currId = entityId
    while currId and currId >= 0 do
        local tr = GetComponent(currId, "Transform")
        if tr then
            if tr.localScale then
                wPos.x = wPos.x * tr.localScale.x
                wPos.y = wPos.y * tr.localScale.y
                wPos.z = wPos.z * tr.localScale.z
            end
            wPos = rotateVec(tr.localRotation, wPos)
            wPos.x = wPos.x + tr.localPosition.x
            wPos.y = wPos.y + tr.localPosition.y
            wPos.z = wPos.z + tr.localPosition.z
            wRot = multiplyQuat(tr.localRotation, wRot)
        end
        if Engine and Engine.GetParentEntity then
            local pId = Engine.GetParentEntity(currId)
            if pId == currId then break end
            currId = pId
        else
            break
        end
    end
    return wPos, wRot
end

-- ── Component ───────────────────────────────────────────────────────────────

return Component {
    mixins = { TransformMixin },

    fields = {
        TotalDuration = 10.0,
        Loop = false,
        -- Which F-key triggers this flythrough (5 = F5, 6 = F6, ... 10 = F10)
        FKey = 5,
        EaseInOut = true,
    },

    Awake = function(self)
        self._active = false
        self._paused = false
        self._timer = 0.0
        self._waypoints = nil
        self._keyEnum = nil
        self._debugPrinted = false

        print("========== [CameraFlythrough] AWAKE CALLED ==========")
        print("[CameraFlythrough] entityId=" .. tostring(self.entityId) .. ", FKey=" .. tostring(self.FKey))
        print("[CameraFlythrough] _G.Input = " .. tostring(_G.Input))
        print("[CameraFlythrough] _G.Engine = " .. tostring(_G.Engine))

        -- Listen for other flythroughs starting so we auto-stop
        if event_bus and event_bus.subscribe then
            self._stopSub = event_bus.subscribe("flythrough.stop_others", function(senderId)
                if senderId ~= self.entityId and self._active then
                    self._active = false
                    self._paused = false
                end
            end)
        end
    end,

    Start = function(self)
        self:_buildWaypoints()
        if self._waypoints then
            print("[CameraFlythrough] Ready — " .. #self._waypoints .. " waypoints, F" .. self.FKey)
        end
    end,

    _buildWaypoints = function(self)
        local count = Engine.GetChildCount(self.entityId)
        if count < 2 then
            print("[CameraFlythrough] Need at least 2 child waypoints, found " .. count)
            self._waypoints = nil
            return
        end

        local wps = {}
        for i = 0, count - 1 do
            local childId = Engine.GetChildAtIndex(self.entityId, i)
            local pos, rot = getWorldTransform(childId)
            wps[#wps + 1] = { pos = pos, rot = rot }
        end
        self._waypoints = wps
    end,

    Update = function(self, dt)
        -- One-time debug dump
        if not self._debugPrinted then
            self._debugPrinted = true
            print("========== [CameraFlythrough] FIRST UPDATE ==========")
            print("[CameraFlythrough] Keyboard = " .. tostring(Keyboard))
            if Keyboard then
                print("[CameraFlythrough] Keyboard.Key = " .. tostring(Keyboard.Key))
                if Keyboard.Key then
                    print("[CameraFlythrough] Keyboard.Key.F5 = " .. tostring(Keyboard.Key.F5))
                end
                print("[CameraFlythrough] Keyboard.IsKeyPressed = " .. tostring(Keyboard.IsKeyPressed))
            end
            local childCount = Engine and Engine.GetChildCount and Engine.GetChildCount(self.entityId) or "N/A"
            print("[CameraFlythrough] Child count = " .. tostring(childCount))
        end

        -- Lazy-resolve the key enum
        if not self._keyEnum then
            if Keyboard and Keyboard.Key then
                local fkeyMap = {
                    [5]  = Keyboard.Key.F5,
                    [6]  = Keyboard.Key.F6,
                    [7]  = Keyboard.Key.F7,
                    [8]  = Keyboard.Key.F8,
                    [9]  = Keyboard.Key.F9,
                    [10] = Keyboard.Key.F10,
                }
                self._keyEnum = fkeyMap[self.FKey]
                if self._keyEnum then
                    print("[CameraFlythrough] Resolved F" .. tostring(self.FKey) .. " = " .. tostring(self._keyEnum))
                else
                    print("[CameraFlythrough] ERROR: No key enum for FKey=" .. tostring(self.FKey))
                end
            else
                print("[CameraFlythrough] WARNING: Keyboard or Keyboard.Key not available!")
            end
        end

        -- Toggle on/off
        if self._keyEnum and Keyboard and Keyboard.IsKeyPressed then
            if Keyboard.IsKeyPressed(self._keyEnum) then
                print("========== [CameraFlythrough] F" .. tostring(self.FKey) .. " PRESSED! ==========")
                if self._active then
                    self:_stop()
                else
                    self:_start()
                end
            end
        end

        if not self._active or self._paused then return end
        if not self._waypoints or #self._waypoints < 2 then return end

        self._timer = self._timer + dt
        local totalT = self._timer / self.TotalDuration

        if totalT >= 1.0 then
            if self.Loop then
                self._timer = 0.0
                totalT = 0.0
            else
                self:_stop()
                return
            end
        end

        -- Apply easing
        local t = totalT
        if self.EaseInOut then
            t = smoothstep(t)
        end

        -- Evaluate spline
        local pos, rot = self:_evaluate(t)

        -- Publish to camera system
        if event_bus and event_bus.publish then
            event_bus.publish("cinematic.target", {
                position = pos,
                rotation = { qw = rot.w, qx = rot.x, qy = rot.y, qz = rot.z },
                lerpT = 1.0,
                phase = "staying",
            })
        end
    end,

    _evaluate = function(self, t)
        local wps = self._waypoints
        local n = #wps
        local segments = n - 1

        -- Map t to segment
        local scaled = t * segments
        local seg = math.floor(scaled)
        local localT = scaled - seg
        if seg >= segments then
            seg = segments - 1
            localT = 1.0
        end

        -- Catmull-Rom indices (clamp at boundaries)
        local i0 = math.max(1, seg)
        local i1 = seg + 1
        local i2 = math.min(n, seg + 2)
        local i3 = math.min(n, seg + 3)

        local p0, p1, p2, p3 = wps[i0].pos, wps[i1].pos, wps[i2].pos, wps[i3].pos

        local pos = {
            x = catmullRom(p0.x, p1.x, p2.x, p3.x, localT),
            y = catmullRom(p0.y, p1.y, p2.y, p3.y, localT),
            z = catmullRom(p0.z, p1.z, p2.z, p3.z, localT),
        }

        -- Slerp rotation between the two main waypoints
        local rot = slerp(wps[i1].rot, wps[i2].rot, localT)

        return pos, rot
    end,

    _start = function(self)
        self:_buildWaypoints()
        if not self._waypoints or #self._waypoints < 2 then
            print("[CameraFlythrough] Cannot start — need at least 2 child waypoints")
            return
        end

        -- Stop any other active flythrough first
        if event_bus and event_bus.publish then
            event_bus.publish("flythrough.stop_others", self.entityId)
        end

        self._active = true
        self._paused = false
        self._timer = 0.0

        if event_bus and event_bus.publish then
            event_bus.publish("flythrough.active", true)
            event_bus.publish("set_attacks_enabled", false)
        end
        print("[CameraFlythrough] Started — " .. #self._waypoints .. " waypoints, " .. self.TotalDuration .. "s")
    end,

    _stop = function(self)
        self._active = false
        self._paused = false

        if event_bus and event_bus.publish then
            event_bus.publish("flythrough.active", false)
            event_bus.publish("set_attacks_enabled", true)
        end
        print("[CameraFlythrough] Stopped")
    end,
}
