-- LinkTransformHandler.lua
-- Cache transforms, provide ApplyPositions and ApplyRotations (parallel transport)
local M = {}

local function normalize(x,y,z)
    local l = math.sqrt(x*x + y*y + z*z)
    if l < 1e-8 then return 0,0,0 end
    return x/l, y/l, z/l
end
local function dot(ax,ay,az, bx,by,bz) return (ax or 0)*(bx or 0) + (ay or 0)*(by or 0) + (az or 0)*(bz or 0) end
local function cross(ax,ay,az, bx,by,bz) return (ay or 0)*(bz or 0) - (az or 0)*(by or 0), (az or 0)*(bx or 0) - (ax or 0)*(bz or 0), (ax or 0)*(by or 0) - (ay or 0)*(bx or 0) end

-- Minimal safe write position helper uses component._write_world_pos if present, else tr:SetPosition/localPosition
local function write_pos_safe(component, tr, x,y,z)
    if component and type(component._write_world_pos) == "function" then
        return component:_write_world_pos(tr, x,y,z)
    end
    if type(tr) == "table" and type(tr.SetPosition) == "function" then
        pcall(function() tr:SetPosition(x,y,z) end)
        return true
    end
    if type(tr.localPosition) ~= "nil" then
        pcall(function()
            local pos = tr.localPosition
            if type(pos) == "table" or type(pos) == "userdata" then
                pos.x, pos.y, pos.z = x,y,z
                tr.isDirty = true
            end
        end)
        return true
    end
    return false
end

function M.New(component)
    local self = {}
    self.component = component
    self.transforms = {}
    self.proxies = {}
    -- rotation continuity storage
    self.qprev = {}
    return setmetatable(self, {__index = M})
end

function M:InitTransforms(transformArray)
    self.transforms = transformArray or {}
    -- cache a minimal proxy API to avoid repeated GetComponent checks
    self.proxies = {}
    for i, tr in ipairs(self.transforms) do
        local proxy = {}
        function proxy:GetPosition()
            -- prefer component reader if exists
            if self._component_owner and type(self._component_owner._read_world_pos) == "function" then
                local ok, a,b,c = pcall(function() return self._component_owner:_read_world_pos(tr) end)
                if ok then
                    if type(a) == "table" then return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0 end
                    if type(a) == "number" then return a,b,c end
                end
            end
            if tr and type(tr.GetPosition) == "function" then
                local ok, a,b,c = pcall(function() return tr:GetPosition() end)
                if ok then
                    if type(a) == "table" then return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0 end
                    if type(a) == "number" then return a,b,c end
                end
            end
            if tr and type(tr.localPosition) ~= "nil" then
                local pos = tr.localPosition
                return pos.x or pos[1] or 0, pos.y or pos[2] or 0, pos.z or pos[3] or 0
            end
            return 0,0,0
        end
        -- cached setter uses write_pos_safe closure
        function proxy:SetPosition(x,y,z)
            write_pos_safe(self._component_owner, tr, x,y,z)
        end
        proxy._component_owner = self.component
        self.proxies[i] = proxy
    end
end

function M:ApplyPositions(positions)
    -- positions: table indexed 1..n each {x,y,z}
    for i, proxy in ipairs(self.proxies) do
        local p = positions[i]
        if p and proxy and proxy.SetPosition then
            proxy:SetPosition(p[1], p[2], p[3])
        end
    end
end

-- Expose UpdateRotations using parallel-transport-based implementation derived from original
function M:ApplyRotations(positions, startPos, endPos, maxStepRad, altTwist)
    -- positions: array 1..n; startPos/endPos optional; maxStepRad optional
    local n = #self.proxies
    if n == 0 then return end
    local rt = {}
    -- build forward array
    local forward = {}
    for i = 1, n do
        local fx,fy,fz = 0,0,0
        if n == 1 then
            fx,fy,fz = (endPos[1]-startPos[1]), (endPos[2]-startPos[2]), (endPos[3]-startPos[3])
        else
            if i == 1 then
                local nx = positions[2][1]; local ny = positions[2][2]; local nz = positions[2][3]
                fx,fy,fz = nx - positions[1][1], ny - positions[1][2], nz - positions[1][3]
            elseif i == n then
                local px = positions[n-1][1]; local py = positions[n-1][2]; local pz = positions[n-1][3]
                fx,fy,fz = positions[n][1] - px, positions[n][2] - py, positions[n][3] - pz
            else
                local px = positions[i-1][1]; local py = positions[i-1][2]; local pz = positions[i-1][3]
                local nx = positions[i+1][1]; local ny = positions[i+1][2]; local nz = positions[i+1][3]
                fx,fy,fz = (nx - px) * 0.5, (ny - py) * 0.5, (nz - pz) * 0.5
            end
        end
        local nfx, nfy, nfz = normalize(fx,fy,fz)
        if nfx == 0 and nfy == 0 and nfz == 0 then
            -- fallback
            nfx, nfy, nfz = 1,0,0
        end
        forward[i] = {nfx, nfy, nfz}
    end

    -- compute world up
    local WORLD_UP = {0,1,0}
    maxStepRad = maxStepRad or math.rad(60)

    for i = 1, n do
        local fx,fy,fz = forward[i][1], forward[i][2], forward[i][3]
        local refUpX, refUpY, refUpZ = WORLD_UP[1], WORLD_UP[2], WORLD_UP[3]
        if math.abs(dot(fx,fy,fz, refUpX, refUpY, refUpZ)) > 0.99 then
            refUpX, refUpY, refUpZ = 1,0,0
        end
        local rx,ry,rz = cross(refUpX, refUpY, refUpZ, fx, fy, fz)
        rx,ry,rz = normalize(rx,ry,rz)
        if rx == 0 and ry == 0 and rz == 0 then
            if math.abs(fx) < 0.9 then rx,ry,rz = 1,0,0 else rx,ry,rz = 0,1,0 end
            local proj = dot(rx,ry,rz, fx,fy,fz)
            rx,ry,rz = normalize(rx - proj * fx, ry - proj * fy, rz - proj * fz)
        end
        local ux,uy,uz = cross(fx,fy,fz, rx,ry,rz)
        ux,uy,uz = normalize(ux,uy,uz)

        if altTwist and (i % 2) == 0 then
            -- rotate rx,ux by +90 degrees about forward
            local angle = math.pi * 0.5
            -- small local quat rotate (fast path)
            -- convert axis-angle to rotation of vector v: v' = v*cos + (axis x v)*sin + axis*(axis·v)*(1-cos)
            local ca = math.cos(angle); local sa = math.sin(angle)
            local ax,ay,az = fx,fy,fz
            local cross_rx_x = (ay * rz - az * ry)
            local cross_rx_y = (az * rx - ax * rz)
            local cross_rx_z = (ax * ry - ay * rx)
            local dota = ax*rx + ay*ry + az*rz
            rx = rx*ca + cross_rx_x*sa + ax*(dota*(1-ca))
            ry = ry*ca + cross_rx_y*sa + ay*(dota*(1-ca))
            rz = rz*ca + cross_rx_z*sa + az*(dota*(1-ca))
            -- same for ux
            local cross_ux_x = (ay * uz - az * uy)
            local cross_ux_y = (az * ux - ax * uz)
            local cross_ux_z = (ax * uy - ay * ux)
            local dota2 = ax*ux + ay*uy + az*uz
            ux = ux*ca + cross_ux_x*sa + ax*(dota2*(1-ca))
            uy = uy*ca + cross_ux_y*sa + ay*(dota2*(1-ca))
            uz = uz*ca + cross_ux_z*sa + az*(dota2*(1-ca))
            rx,ry,rz = normalize(rx,ry,rz)
            ux,uy,uz = normalize(ux,uy,uz)
        end

        -- build matrix columns RIGHT(rx,ry,rz), UP(ux,uy,uz), FORWARD(fx,fy,fz)
        -- convert to quaternion via a compact mat->quat
        -- inline mat3_to_quat (same logic as original) - keep identical math for stability
        local m00,m10,m20 = rx,ry,rz
        local m01,m11,m21 = ux,uy,uz
        local m02,m12,m22 = fx,fy,fz
        local tr = m00 + m11 + m22
        local qw,qx,qy,qz
        if tr > 0 then
            local S = math.sqrt(tr + 1.0) * 2.0
            qw = 0.25 * S
            qx = (m21 - m12) / S
            qy = (m02 - m20) / S
            qz = (m10 - m01) / S
        else
            if (m00 > m11) and (m00 > m22) then
                local S = math.sqrt(1.0 + m00 - m11 - m22) * 2.0
                qw = (m21 - m12) / S
                qx = 0.25 * S
                qy = (m01 + m10) / S
                qz = (m02 + m20) / S
            elseif m11 > m22 then
                local S = math.sqrt(1.0 + m11 - m00 - m22) * 2.0
                qw = (m02 - m20) / S
                qx = (m01 + m10) / S
                qy = 0.25 * S
                qz = (m12 + m21) / S
            else
                local S = math.sqrt(1.0 + m22 - m00 - m11) * 2.0
                qw = (m10 - m01) / S
                qx = (m02 + m20) / S
                qy = (m12 + m21) / S
                qz = 0.25 * S
            end
        end
        qw,qx,qy,qz = qw or 1, qx or 0, qy or 0, qz or 0

        -- normalize target quaternion
        local tlen = math.sqrt(qw*qw + qx*qx + qy*qy + qz*qz)
        if tlen > 1e-12 then qw,qx,qy,qz = qw/tlen, qx/tlen, qy/tlen, qz/tlen else qw,qx,qy,qz = 1,0,0,0 end

        -- continuity: load previous quaternion (self.qprev)
        local prev_w,prev_x,prev_y,prev_z = 1,0,0,0
        if self.qprev[i] and #self.qprev[i] == 4 then
            prev_w,prev_x,prev_y,prev_z = self.qprev[i][1], self.qprev[i][2], self.qprev[i][3], self.qprev[i][4]
            local plen = math.sqrt(prev_w*prev_w + prev_x*prev_x + prev_y*prev_y + prev_z*prev_z)
            if plen > 1e-12 then prev_w,prev_x,prev_y,prev_z = prev_w/plen, prev_x/plen, prev_y/plen, prev_z/plen else prev_w,prev_x,prev_y,prev_z = 1,0,0,0 end
        end

        local dotq = prev_w*qw + prev_x*qx + prev_y*qy + prev_z*qz
        if dotq < 0 then qw,qx,qy,qz = -qw, -qx, -qy, -qz; dotq = -dotq end
        if dotq > 1 then dotq = 1 elseif dotq < -1 then dotq = -1 end
        local ang = math.acos(math.max(-1, math.min(1, dotq)))
        local final_w, final_x, final_y, final_z = qw,qx,qy,qz
        if ang > maxStepRad + 1e-9 then
            local t = maxStepRad / ang
            if dotq > 0.9995 then
                final_w = prev_w + t * (qw - prev_w)
                final_x = prev_x + t * (qx - prev_x)
                final_y = prev_y + t * (qy - prev_y)
                final_z = prev_z + t * (qz - prev_z)
                local l = math.sqrt(final_w*final_w + final_x*final_x + final_y*final_y + final_z*final_z)
                if l > 1e-12 then final_w,final_x,final_y,final_z = final_w/l,final_x/l,final_y/l,final_z/l else final_w,final_x,final_y,final_z = 1,0,0,0 end
            else
                local theta = math.acos(dotq)
                local sinTheta = math.sin(theta)
                if sinTheta < 1e-12 then
                    final_w,final_x,final_y,final_z = prev_w, prev_x, prev_y, prev_z
                else
                    local scale0 = math.sin((1 - t) * theta) / sinTheta
                    local scale1 = math.sin(t * theta) / sinTheta
                    final_w = prev_w * scale0 + qw * scale1
                    final_x = prev_x * scale0 + qx * scale1
                    final_y = prev_y * scale0 + qy * scale1
                    final_z = prev_z * scale0 + qz * scale1
                    local l = math.sqrt(final_w*final_w + final_x*final_x + final_y*final_y + final_z*final_z)
                    if l > 1e-12 then final_w,final_x,final_y,final_z = final_w/l, final_x/l, final_y/l, final_z/l else final_w,final_x,final_y,final_z = 1,0,0,0 end
                end
            end
        end

        -- write rotation to transform safely
        local tr = self.transforms[i]
        pcall(function()
            if tr and tr.localRotation then
                local rot = tr.localRotation
                if type(rot) == "table" or type(rot) == "userdata" then
                    rot.w, rot.x, rot.y, rot.z = final_w, final_x, final_y, final_z
                    tr.isDirty = true
                end
            end
        end)

        self.qprev[i] = { final_w, final_x, final_y, final_z }
    end
end

function M:Dump()
    for i, proxy in ipairs(self.proxies) do
        local x,y,z = proxy:GetPosition()
        if self.component and type(self.component.Log) == "function" then
            self.component:Log(string.format("Link %d pos=(%.3f, %.3f, %.3f)", i, x or 0, y or 0, z or 0))
        end
    end
end

return M
