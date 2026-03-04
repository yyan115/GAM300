-- Camera/camera_utils.lua
-- Shared math utilities for camera scripts

local M = {}

function M.clamp(x, minv, maxv)
    if x < minv then return minv end
    if x > maxv then return maxv end
    return x
end

-- Portable atan2(y, x) that works across Lua versions
function M.atan2(y, x)
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

-- Euler angles (degrees) to quaternion {w, x, y, z}
function M.eulerToQuat(pitch, yaw, roll)
    local p = math.rad(pitch or 0) * 0.5
    local y = math.rad(yaw   or 0) * 0.5
    local r = math.rad(roll  or 0) * 0.5
    local sinP, cosP = math.sin(p), math.cos(p)
    local sinY, cosY = math.sin(y), math.cos(y)
    local sinR, cosR = math.sin(r), math.cos(r)
    return {
        w = cosP * cosY * cosR + sinP * sinY * sinR,
        x = sinP * cosY * cosR - cosP * sinY * sinR,
        y = cosP * sinY * cosR + sinP * cosY * sinR,
        z = cosP * cosY * sinR - sinP * sinY * cosR,
    }
end

return M
