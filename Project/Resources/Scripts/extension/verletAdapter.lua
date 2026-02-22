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

-- Single substep of integration + constraints. Called multiple times per frame by M.Step.
local function stepOnce(state, dt, params)
    local n = (params and params.n) or state.n
    local gmag = math.abs((params and params.VerletGravity) or 9.81)
    local damping = math.max(0, math.min(1, (params and params.VerletDamping) or 0.02))
    local dt2 = dt * dt

    local positions = state.positions
    local prev = state.prev
    local invMass = state.invMass

    -- Determine if we need bidirectional constraint solving (both endpoints pinned)
    local needsBidirectional = params and (not params.IsElastic) and params.pinnedLast and type(params.endPos) == "table"

    -- integrate dynamic particles
    for i = 1, n do
        local inv = (invMass[i] ~= nil) and invMass[i] or 1
        if inv > 0 then
            local px,py,pz = positions[i][1], positions[i][2], positions[i][3]
            local ppx,ppy,ppz = prev[i][1], prev[i][2], prev[i][3]
            local vx = (px - ppx) * (1 - damping)
            local vy = (py - ppy) * (1 - damping)
            local vz = (pz - ppz) * (1 - damping)
            local nx = px + vx
            local ny = py + vy + (-gmag) * dt2
            local nz = pz + vz
            prev[i][1], prev[i][2], prev[i][3] = px, py, pz
            positions[i][1], positions[i][2], positions[i][3] = nx, ny, nz
        end
    end

    -- enforce pinned start position
    if params and type(params.startPos) == "table" then
        positions[1][1], positions[1][2], positions[1][3] =
            params.startPos[1], params.startPos[2], params.startPos[3]
        prev[1][1], prev[1][2], prev[1][3] =
            params.startPos[1], params.startPos[2], params.startPos[3]
    end

    -- enforce pinned end position
    if params and params.pinnedLast and type(params.endPos) == "table" then
        positions[n][1], positions[n][2], positions[n][3] =
            params.endPos[1], params.endPos[2], params.endPos[3]
        prev[n][1], prev[n][2], prev[n][3] =
            params.endPos[1], params.endPos[2], params.endPos[3]
    end

    -- constraints
    local segLen = (params and params.segmentLen) or ( (n>1) and (math.max((params and params.totalLen) or 0.000001, EPS) / (n-1)) or 0 )
    if params and params.ClampSegment then segLen = math.min(segLen, params.ClampSegment) end

    local iterations = math.min(20, tonumber((params and params.ConstraintIterations) or 2))

    for it = 1, iterations do
        if needsBidirectional then
            -- Forward pass
            for i = 2, n do
                local a = positions[i-1]
                local b = positions[i]
                local ax,ay,az = a[1], a[2], a[3]
                local bx,by,bz = b[1], b[2], b[3]
                local dx,dy,dz = bx - ax, by - ay, bz - az
                local dist = vec_len(dx,dy,dz)
                if dist < EPS then dist = EPS end
                local target = segLen
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

            -- Backward pass
            for i = n, 2, -1 do
                local a = positions[i-1]
                local b = positions[i]
                local ax,ay,az = a[1], a[2], a[3]
                local bx,by,bz = b[1], b[2], b[3]
                local dx,dy,dz = bx - ax, by - ay, bz - az
                local dist = vec_len(dx,dy,dz)
                if dist < EPS then dist = EPS end
                local target = segLen
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
        else
            -- Single forward pass
            for i = 2, n do
                local a = positions[i-1]
                local b = positions[i]
                local ax,ay,az = a[1], a[2], a[3]
                local bx,by,bz = b[1], b[2], b[3]
                local dx,dy,dz = bx - ax, by - ay, bz - az
                local dist = vec_len(dx,dy,dz)
                if dist < EPS then dist = EPS end
                local target = segLen
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
        end

        -- re-pin anchors every iteration
        if params and type(params.startPos) == "table" then
            positions[1][1], positions[1][2], positions[1][3] =
                params.startPos[1], params.startPos[2], params.startPos[3]
            prev[1][1], prev[1][2], prev[1][3] =
                params.startPos[1], params.startPos[2], params.startPos[3]
        end
        if params and params.pinnedLast and type(params.endPos) == "table" then
            positions[n][1], positions[n][2], positions[n][3] =
                params.endPos[1], params.endPos[2], params.endPos[3]
            prev[n][1], prev[n][2], prev[n][3] =
                params.endPos[1], params.endPos[2], params.endPos[3]
        end
    end

    -- final strict inelastic clamp
    if params and (not params.IsElastic) and params.LinkMaxDistance and params.LinkMaxDistance > 0 then
        local maxd = params.LinkMaxDistance
        if params.pinnedLast and params.endPos then
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

    -- Ground clamp: prevent links from going below terrain.
    -- Uses a single pre-computed groundY from ChainController (one raycast per frame, O(1)).
    -- Only a simple Y comparison per link here, no additional raycasts.
    if params and params.GroundClamp and params.groundY ~= nil then
        local groundY = params.groundY
        for i = 1, n do
            if (invMass[i] or 0) > 0 then  -- only clamp dynamic links
                if positions[i][2] < groundY then
                    positions[i][2] = groundY
                    prev[i][2] = groundY  -- kill downward velocity at ground
                end
            end
        end
    end
end

function M.Step(state, dt, params)
    if not state or dt <= 0 or (state.n or 0) == 0 then return end

    local subSteps = math.max(1, tonumber((params and params.SubSteps) or 4))
    local subDt = dt / subSteps

    for step = 1, subSteps do
        stepOnce(state, subDt, params)
    end

    return state.positions, state.prev, state.invMass
end

-- Wall collision: raycast between every WallClampInterval links once per frame.
-- If a segment hits geometry, links between the sample points are nudged back
-- by WallClampRadius along the reverse ray direction.
-- Default radius is 0 (no nudge), tune upward if links clip into walls.
function M.ApplyWallClamp(state, params)
    if not params or not params.WallClamp then return end
    if not Physics or not Physics.Raycast then return end

    local n = params.n or state.n
    local interval = math.max(2, tonumber(params.WallClampInterval) or 10)
    local radius = tonumber(params.WallClampRadius) or 0
    local positions = state.positions
    local prev = state.prev
    local invMass = state.invMass

    local i = 1
    while i <= n do
        local j = math.min(i + interval, n)

        local ax, ay, az = positions[i][1], positions[i][2], positions[i][3]
        local bx, by, bz = positions[j][1], positions[j][2], positions[j][3]

        local dx, dy, dz = bx - ax, by - ay, bz - az
        local dist = math.sqrt(dx*dx + dy*dy + dz*dz)

        if dist > 1e-6 then
            local nx, ny, nz = dx/dist, dy/dist, dz/dist
            local hitDist = Physics.Raycast(ax, ay, az, nx, ny, nz, dist)

            if hitDist and hitDist > 0 and hitDist < dist then
                local pushX = -nx * radius
                local pushY = -ny * radius
                local pushZ = -nz * radius

                for k = i, j do
                    if (invMass[k] or 0) > 0 then
                        positions[k][1] = positions[k][1] + pushX
                        positions[k][2] = positions[k][2] + pushY
                        positions[k][3] = positions[k][3] + pushZ
                        prev[k][1] = prev[k][1] + pushX
                        prev[k][2] = prev[k][2] + pushY
                        prev[k][3] = prev[k][3] + pushZ
                    end
                end
            end
        end

        i = i + interval
    end
end

return M