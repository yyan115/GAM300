-- extension/verletGravity.lua
-- Lightweight Verlet solver module with correct anchor behavior
-- First link follows player, last link pins at hit point when fully extended

local M = {}

M.EPS = 1e-6

-- Read world pos helper
local function read_world_pos(component, tr)
    if component and type(component._read_world_pos) == "function" then
        return component:_read_world_pos(tr)
    end
    if not tr then return 0,0,0 end

    local ok, a, b, c = pcall(function() return tr:GetPosition() end)
    if ok then
        if type(a) == "table" then
            return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
        end
        if type(a) == "number" and type(b) == "number" and type(c) == "number" then
            return a, b, c
        end
    end
    return 0,0,0
end

local function write_world_pos(component, tr, x, y, z)
    if component and type(component._write_world_pos) == "function" then
        return component:_write_world_pos(tr, x, y, z)
    end
    if not tr then return false end

    if type(tr) == "table" and type(tr.SetPosition) == "function" then
        pcall(function() tr:SetPosition(x, y, z) end)
        return true
    end
    return false
end

function M.InitVerlet(component)
    component._runtime = component._runtime or {}
    local rt = component._runtime
    rt.childTransforms = rt.childTransforms or {}
    rt.p = rt.p or {}
    rt.pprev = rt.pprev or {}
    rt.invMass = rt.invMass or {}

    local n = #rt.childTransforms
    if n == 0 then return end

    for i = 1, n do
        local tr = rt.childTransforms[i]
        local x, y, z = read_world_pos(component, tr)
        rt.p[i] = rt.p[i] or { x, y, z }
        rt.pprev[i] = rt.pprev[i] or { x, y, z }
        rt.invMass[i] = rt.invMass[i] or 1
    end

    -- First link is always kinematic (follows player)
    rt.invMass[1] = 0
end

function M.VerletStep(component, dt)
    if not component or dt <= 0 then return end
    component._runtime = component._runtime or {}
    local rt = component._runtime
    if not rt.p or #rt.p == 0 then return end

    local n = #rt.p
    local gmag = math.abs(component.VerletGravity or 9.81)
    local damping = math.max(0, math.min(1, component.VerletDamping or 0.02))
    local dt2 = dt * dt

    -- Verlet integration for all free particles
    for i = 1, n do
        if (rt.invMass[i] or 1) > 0 then
            local px, py, pz = rt.p[i][1], rt.p[i][2], rt.p[i][3]
            local ppx, ppy, ppz = rt.pprev[i][1], rt.pprev[i][2], rt.pprev[i][3]
            local vx = (px - ppx) * (1 - damping)
            local vy = (py - ppy) * (1 - damping)
            local vz = (pz - ppz) * (1 - damping)
            local ax, ay, az = 0, -gmag, 0
            local nx = px + vx + ax * dt2
            local ny = py + vy + ay * dt2
            local nz = pz + vz + az * dt2
            rt.pprev[i][1], rt.pprev[i][2], rt.pprev[i][3] = px, py, pz
            rt.p[i][1], rt.p[i][2], rt.p[i][3] = nx, ny, nz
        end
    end

    -- CRITICAL FIX: Anchor first link to player position (start of chain)
    local sx, sy, sz = 0, 0, 0
    if type(component._get_start_world) == "function" then
        sx, sy, sz = component:_get_start_world()
    end
    rt.p[1][1], rt.p[1][2], rt.p[1][3] = sx, sy, sz
    rt.pprev[1][1], rt.pprev[1][2], rt.pprev[1][3] = sx, sy, sz
    rt.invMass[1] = 0  -- First link always kinematic

    -- Compute segment length
    local dx_e = (component.endPosition and component.endPosition[1] or sx) - sx
    local dy_e = (component.endPosition and component.endPosition[2] or sy) - sy
    local dz_e = (component.endPosition and component.endPosition[3] or sz) - sz
    local curEndDist = math.sqrt(dx_e*dx_e + dy_e*dy_e + dz_e*dz_e)
    local totalLen = (component.MaxLength and component.MaxLength > 0) and component.MaxLength or math.max(curEndDist, M.EPS)
    local segmentLen = (#rt.p > 1) and (totalLen / (#rt.p - 1)) or 0

    -- Determine which links should be active based on chainLength
    local chainLen = component.chainLength or 0
    for i = 1, #rt.p do
        local requiredDist = (i - 1) * segmentLen
        if chainLen + 1e-6 < requiredDist then
            -- Link hasn't been "deployed" yet - keep it kinematic at start
            rt.invMass[i] = 0
            rt.p[i][1], rt.p[i][2], rt.p[i][3] = sx, sy, sz
            rt.pprev[i][1], rt.pprev[i][2], rt.pprev[i][3] = sx, sy, sz
        else
            if i ~= 1 then 
                rt.invMass[i] = 1  -- Link is active and can move freely
            end
        end
    end

    -- CRITICAL FIX: If chain is fully extended (stopped extending), pin LAST link to end position
    local lastIdx = #rt.p
    local lastRequired = (lastIdx - 1) * segmentLen
    local isFullyExtended = not component.isExtending and chainLen + 1e-6 >= lastRequired
    
    if isFullyExtended and component.endPosition then
        -- Last link is pinned to world-space end position (hit point)
        rt.p[lastIdx][1] = component.endPosition[1]
        rt.p[lastIdx][2] = component.endPosition[2]
        rt.p[lastIdx][3] = component.endPosition[3]
        rt.pprev[lastIdx][1], rt.pprev[lastIdx][2], rt.pprev[lastIdx][3] = 
            rt.p[lastIdx][1], rt.p[lastIdx][2], rt.p[lastIdx][3]
        rt.invMass[lastIdx] = 0  -- Last link becomes kinematic when pinned
    end

    -- Distance constraints
    local iters = math.max(1, math.min(8, tonumber(component.ConstraintIterations or 2)))
    for it = 1, iters do
        for i = 2, #rt.p do
            local a = rt.p[i-1]
            local b = rt.p[i]
            local ax, ay, az = a[1], a[2], a[3]
            local bx, by, bz = b[1], b[2], b[3]
            local dx, dy, dz = bx - ax, by - ay, bz - az
            local dist = math.sqrt(dx*dx + dy*dy + dz*dz)
            if dist < M.EPS then dist = M.EPS end
            local diff = (dist - segmentLen) / dist
            local invA = rt.invMass[i-1] or 0
            local invB = rt.invMass[i] or 0
            local w = invA + invB
            if w > M.EPS then
                local fa = (invA / w) * diff
                local fb = (invB / w) * diff
                if invA > 0 then
                    a[1] = ax + dx * fa
                    a[2] = ay + dy * fa
                    a[3] = az + dz * fa
                end
                if invB > 0 then
                    b[1] = bx - dx * fb
                    b[2] = by - dy * fb
                    b[3] = bz - dz * fb
                end
            end
        end

        -- Re-apply anchors after each constraint iteration
        rt.p[1][1], rt.p[1][2], rt.p[1][3] = sx, sy, sz
        rt.pprev[1][1], rt.pprev[1][2], rt.pprev[1][3] = sx, sy, sz
        
        if isFullyExtended and component.endPosition then
            rt.p[lastIdx][1] = component.endPosition[1]
            rt.p[lastIdx][2] = component.endPosition[2]
            rt.p[lastIdx][3] = component.endPosition[3]
            rt.pprev[lastIdx][1], rt.pprev[lastIdx][2], rt.pprev[lastIdx][3] = 
                rt.p[lastIdx][1], rt.p[lastIdx][2], rt.p[lastIdx][3]
        end
    end

    -- Write back to transforms
    for i = 1, #rt.p do
        local tr = rt.childTransforms[i]
        if tr then
            write_world_pos(component, tr, rt.p[i][1], rt.p[i][2], rt.p[i][3])
        end
    end
end

function M.DumpLinkPositions(component)
    local rt = component._runtime or {}
    rt.childTransforms = rt.childTransforms or {}
    for i, tr in ipairs(rt.childTransforms) do
        local x, y, z = read_world_pos(component, tr)
        local name = component and component._read_transform_name and component:_read_transform_name(tr) or tostring(tr)
        if component and type(component.Log) == "function" then
            component:Log(string.format("Link %d name=%s worldPos=(%.3f, %.3f, %.3f)", i, name, x, y, z))
        end
    end
end

return M