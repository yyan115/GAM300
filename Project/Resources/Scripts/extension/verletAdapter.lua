-- verletAdapter.lua (refactored: pure physics, does NOT mutate invMass; controller owns invMass)
local M = {}
local EPS = 1e-6

local function vec_len(x,y,z) return math.sqrt((x or 0)*(x or 0) + (y or 0)*(y or 0) + (z or 0)*(z or 0)) end

function M.Init(state)
    state.positions = state.positions or {}
    state.prev = state.prev or {}
    state.invMass = state.invMass or {}
    state.n = #state.positions
    return state
end

function M.Step(state, dt, params)
    if not state or dt <= 0 or (state.n or 0) == 0 then return end
    local n = state.n
    local gmag = math.abs((params and params.VerletGravity) or 9.81)
    local damping = math.max(0, math.min(1, (params and params.VerletDamping) or 0.02))
    local dt2 = dt * dt

    local positions = state.positions
    local prev = state.prev
    local invMass = state.invMass

    -- integrate dynamic particles (respect invMass array provided by controller; do not mutate it)
    for i = 1, n do
        local inv = (invMass[i] ~= nil) and invMass[i] or 1
        if inv > 0 then
            local px,py,pz = positions[i][1], positions[i][2], positions[i][3]
            local ppx,ppy,ppz = prev[i][1], prev[i][2], prev[i][3]
            local vx = (px - ppx) * (1 - damping)
            local vy = (py - ppy) * (1 - damping)
            local vz = (pz - ppz) * (1 - damping)
            local ax,ay,az = 0, -gmag, 0
            local nx = px + vx + ax * dt2
            local ny = py + vy + ay * dt2
            local nz = pz + vz + az * dt2
            prev[i][1], prev[i][2], prev[i][3] = px, py, pz
            positions[i][1], positions[i][2], positions[i][3] = nx, ny, nz
        end
    end

    -- enforce pinned start/end positions if provided (do not change invMass)
    if params and type(params.startPos) == "table" then
        positions[1][1], positions[1][2], positions[1][3] =
            params.startPos[1], params.startPos[2], params.startPos[3]
        prev[1][1], prev[1][2], prev[1][3] =
            params.startPos[1], params.startPos[2], params.startPos[3]
    end

    if params and params.pinnedLast and type(params.endPos) == "table" then
        local li = n
        positions[li][1], positions[li][2], positions[li][3] =
            params.endPos[1], params.endPos[2], params.endPos[3]
        prev[li][1], prev[li][2], prev[li][3] =
            params.endPos[1], params.endPos[2], params.endPos[3]
    end

    -- constraints: prefer explicit segmentLen from params; fallback to totalLen/(n-1)
    local segLen = (params and params.segmentLen) or ( (n>1) and (math.max((params and params.totalLen) or 0.000001, EPS) / (n-1)) or 0 )
    if params and params.ClampSegment then segLen = math.min(segLen, params.ClampSegment) end

    local iterations = math.max(1, math.min(8, tonumber((params and params.ConstraintIterations) or 2)))

    for it = 1, iterations do
        for i = 2, n do
            local a = positions[i-1]
            local b = positions[i]
            local ax,ay,az = a[1], a[2], a[3]
            local bx,by,bz = b[1], b[2], b[3]
            local dx,dy,dz = bx - ax, by - ay, bz - az
            local dist = vec_len(dx,dy,dz)
            if dist < EPS then dist = EPS end
            local target = segLen
            -- respect a strict per-link clamp if provided (controller decided IsElastic / LinkMaxDistance)
            if params and params.LinkMaxDistance and (not params.IsElastic) then
                if target > params.LinkMaxDistance then target = params.LinkMaxDistance end
            end
            local diff = (dist - target) / dist
            local invA = invMass[i-1] or 0
            local invB = invMass[i] or 0
            local w = invA + invB
            if w > EPS then
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

        -- re-apply pinned anchors each iteration (positions/prev only)
        if params and type(params.startPos) == "table" then
            positions[1][1], positions[1][2], positions[1][3] =
                params.startPos[1], params.startPos[2], params.startPos[3]
            prev[1][1], prev[1][2], prev[1][3] =
                params.startPos[1], params.startPos[2], params.startPos[3]
        end
        if params and params.pinnedLast and type(params.endPos) == "table" then
            local li = n
            positions[li][1], positions[li][2], positions[li][3] =
                params.endPos[1], params.endPos[2], params.endPos[3]
            prev[li][1], prev[li][2], prev[li][3] =
                params.endPos[1], params.endPos[2], params.endPos[3]
        end
    end

    -- final strict inelastic pass if controller requested per-link clamp (positions only)
    if params and (not params.IsElastic) and params.LinkMaxDistance and params.LinkMaxDistance > 0 then
        local maxd = params.LinkMaxDistance
        if (params.pinnedLast and params.endPos) then
            for i = n-1, 1, -1 do
                local a = positions[i+1]; local b = positions[i]
                local dx,dy,dz = b[1]-a[1], b[2]-a[2], b[3]-a[3]
                local dist = vec_len(dx,dy,dz)
                if dist > maxd and dist > EPS then
                    local inv = 1.0 / dist
                    local dirx,diry,dirz = dx * inv, dy * inv, dz * inv
                    b[1], b[2], b[3] = a[1] + dirx * maxd, a[2] + diry * maxd, a[3] + dirz * maxd
                end
            end
        else
            for i = 2, n do
                local a = positions[i-1]; local b = positions[i]
                local dx,dy,dz = b[1]-a[1], b[2]-a[2], b[3]-a[3]
                local dist = vec_len(dx,dy,dz)
                if dist > maxd and dist > EPS then
                    local inv = 1.0 / dist
                    local dirx,diry,dirz = dx * inv, dy * inv, dz * inv
                    b[1], b[2], b[3] = a[1] + dirx * maxd, a[2] + diry * maxd, a[3] + dirz * maxd
                end
            end
        end
    end

    return positions, prev, invMass
end

return M