-- Camera/camera_utils.lua
-- Shared math utilities for camera scripts

local M = {}

function M.clamp(x, minv, maxv)
    if x < minv then return minv end
    if x > maxv then return maxv end
    return x
end

-- The engine embeds Lua 5.4, whose math.atan supports atan2(y, x).
function M.atan2(y, x)
    return math.atan(y, x)
end

-- Euler angles (degrees) to quaternion components.
function M.eulerToQuatValues(pitch, yaw, roll)
    local p = math.rad(pitch or 0) * 0.5
    local y = math.rad(yaw   or 0) * 0.5
    local r = math.rad(roll  or 0) * 0.5
    local sinP, cosP = math.sin(p), math.cos(p)
    local sinY, cosY = math.sin(y), math.cos(y)
    local sinR, cosR = math.sin(r), math.cos(r)
    return
        cosP * cosY * cosR + sinP * sinY * sinR,
        sinP * cosY * cosR - cosP * sinY * sinR,
        cosP * sinY * cosR + sinP * cosY * sinR,
        cosP * cosY * sinR - sinP * sinY * cosR
end

-- Compatibility wrapper for callers that retain the quaternion as a table.
function M.eulerToQuat(pitch, yaw, roll)
    local w, x, y, z = M.eulerToQuatValues(pitch, yaw, roll)
    return {
        w = w,
        x = x,
        y = y,
        z = z,
    }
end

return M
