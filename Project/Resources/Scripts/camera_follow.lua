-- camera_follow.lua
require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

local function clamp(x, minv, maxv)
    if x < minv then return minv end
    if x > maxv then return maxv end
    return x
end

-- portable atan2(y, x)
local function atan2(y, x)
    if math.atan2 then return math.atan2(y, x) end
    local ok, res = pcall(math.atan, y, x)
    if ok then return res end
    if x > 0 then return math.atan(y / x)
    elseif x < 0 and y >= 0 then return math.atan(y / x) + math.pi
    elseif x < 0 and y < 0 then return math.atan(y / x) - math.pi
    elseif x == 0 and y > 0 then return math.pi / 2
    elseif x == 0 and y < 0 then return -math.pi / 2
    else return 0.0 end
end

-- Helper: Euler (deg) → Quaternion
local function eulerToQuat(pitch, yaw, roll)
    local p = math.rad(pitch or 0) * 0.5
    local y = math.rad(yaw or 0) * 0.5
    local r = math.rad(roll or 0) * 0.5
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
        followDistance   = 5.0,
        heightOffset     = 1.0,
        followLerp       = 10.0,
        mouseSensitivity = 0.15,
        minPitch         = -30.0,
        maxPitch         = 60.0,
    },

    Awake = function(self)
        self._yaw   = 180.0   -- start orbit rotated 180° around Y
        self._pitch = 15.0    -- keep a slight downward tilt

        self._targetPos  = { x = 0.0, y = 0.0, z = 0.0 }
        self._hasTarget  = false
        self._posSub     = nil
        self._lastMouseX = 0.0
        self._lastMouseY = 0.0
        self._firstMouse = true

        if event_bus and event_bus.subscribe then
            self._posSub = event_bus.subscribe("player_position", function(payload)
                if not payload then return end
                local x = payload.x or payload[1] or 0.0
                local y = payload.y or payload[2] or 0.0
                local z = payload.z or payload[3] or 0.0
                self._targetPos.x, self._targetPos.y, self._targetPos.z = x, y + 1, z
                self._hasTarget = true
            end)
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe and self._posSub then
            event_bus.unsubscribe(self._posSub)
            self._posSub = nil
        end
    end,

    _updateMouseLook = function(self, dt)
        if not (Input and Input.GetMouseButton and Input.GetMouseX and Input.GetMouseY) then return end
        local xpos, ypos = Input.GetMouseX(), Input.GetMouseY()
        if self._firstMouse then
            self._firstMouse = false
            self._lastMouseX, self._lastMouseY = xpos, ypos
            return
        end
        local xoffset = (xpos - self._lastMouseX) * (self.mouseSensitivity or 0.15)
        local yoffset = (self._lastMouseY - ypos) * (self.mouseSensitivity or 0.15)
        self._lastMouseX, self._lastMouseY = xpos, ypos
        self._yaw   = self._yaw   - xoffset
        self._pitch = clamp(self._pitch + yoffset, self.minPitch or -80.0, self.maxPitch or 80.0)

    end,

    Update = function(self, dt)
        if not (self.GetPosition and self.SetPosition and self.SetRotation) then return end
        if not self._hasTarget then return end

        self:_updateMouseLook(dt)

        local tx, ty, tz = self._targetPos.x, self._targetPos.y, self._targetPos.z
        local radius   = self.followDistance or 5.0
        local pitchRad = math.rad(self._pitch)
        local yawRad   = math.rad(self._yaw)

        local horizontalRadius = radius * math.cos(pitchRad)
        local offsetX = horizontalRadius * math.sin(yawRad)
        local offsetZ = horizontalRadius * math.cos(yawRad)
        local offsetY = radius * math.sin(pitchRad) + (self.heightOffset or 0.0)

        local desiredX, desiredY, desiredZ = tx + offsetX, ty + offsetY, tz + offsetZ

        -- Smooth follow
        local cx, cy, cz = 0.0, 0.0, 0.0
        local px, py, pz = self:GetPosition()
        if type(px) == "table" then
            cx, cy, cz = px.x or 0.0, px.y or 0.0, px.z or 0.0
        else
            cx, cy, cz = px or 0.0, py or 0.0, pz or 0.0
        end

        local t = 1.0 - math.exp(-(self.followLerp or 10.0) * dt)
        local newX = cx + (desiredX - cx) * t
        local newY = cy + (desiredY - cy) * t
        local newZ = cz + (desiredZ - cz) * t
        self:SetPosition(newX, newY, newZ)

        -- Look at target: compute yaw/pitch, then convert to quaternion
        local fx, fy, fz = tx - newX, ty - newY, tz - newZ
        local flen = math.sqrt(fx*fx + fy*fy + fz*fz)
        if flen > 0.0001 then
            fx, fy, fz = fx/flen, fy/flen, fz/flen
            local yawDeg   = math.deg(atan2(fx, fz))
            local pitchDeg = -math.deg(math.asin(fy))
            local quat = eulerToQuat(pitchDeg, yawDeg, 0.0)
            self:SetRotation(quat.w, quat.x, quat.y, quat.z)
        end

        self.isDirty = true
    end,
}
