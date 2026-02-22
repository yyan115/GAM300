-- ChainController.lua (refactored with raycast collision detection)
local VerletAdapter = require("extension.verletAdapter")
local M = {}

local function vec_len(x,y,z) return math.sqrt((x or 0)*(x or 0) + (y or 0)*(y or 0) + (z or 0)*(z or 0)) end
local function normalize(x,y,z)
    local L = vec_len(x,y,z)
    if L < 1e-9 then return 0,0,0 end
    return x / L, y / L, z / L
end

function M.New(params)
    local self = {}
    self.params = params or {}
    self.n = math.max(1, tonumber(self.params.NumberOfLinks) or 1)

    -- allocate arrays once (positions/prev/invMass are owned by controller and passed to Verlet)
    self.positions = {}
    self.prev = {}
    self.invMass = {}
    for i = 1, self.n do
        self.positions[i] = {0,0,0}
        self.prev[i] = {0,0,0}
        self.invMass[i] = 1
    end

    self.activeN = self.n      -- how many links from the pool are currently active; set on StartExtension
    self.anchors = {}           -- map index->true; ComputeAnchors only marks anchors (does not mutate invMass)
    self.chainLen = 0.0         -- authoritative logical chain length (meters)
    self.extensionTime = 0.0
    self.isExtending = false
    self.isRetracting = false
    self.lastForward = {0,0,1}  -- use component to set a different forward if desired (StartExtension(forward))
    self.startPos = {0,0,0}
    self.endPos = {0,0,0}
    
    -- Track if endpoint is locked to a collision point
    self.endPointLocked = false
    self.lockedEndPoint = {0,0,0}

    -- Verlet state is a thin wrapper over these arrays
    self.VerletState = VerletAdapter.Init{ positions = self.positions, prev = self.prev, invMass = self.invMass }
    return setmetatable(self, { __index = M })
end

function M:SetStartPos(x,y,z)
    self.startPos = {x or 0, y or 0, z or 0}
end

function M:SetEndPos(x,y,z)
    self.endPos = {x or 0, y or 0, z or 0}
end

-- Accept optional forward vector, maxLength and linkMaxDistance to compute active link count from pool
function M:StartExtension(forward, maxLength, linkMaxDistance)
    self.isExtending = true
    self.isRetracting = false
    self.extensionTime = 0
    self.chainLen = 0
    
    -- Reset endpoint lock when starting new extension
    self.endPointLocked = false
    self.lockedEndPoint = {0,0,0}

    -- Calculate how many links are needed from the pool based on max length and link max distance
    local maxLen = tonumber(maxLength) or tonumber(self.params.MaxLength) or 0
    local linkMax = tonumber(linkMaxDistance) or tonumber(self.params.LinkMaxDistance) or 0
    if linkMax > 0 and maxLen > 0 then
        local needed = math.ceil(maxLen / linkMax) + 1  -- +1 for the start/anchor link
        self.activeN = math.min(needed, self.n)
        print(string.format("[ChainController] StartExtension: MaxLength=%.3f LinkMaxDistance=%.4f needed=%d poolSize=%d activeN=%d",
            maxLen, linkMax, needed, self.n, self.activeN))
    else
        self.activeN = self.n
        print(string.format("[ChainController] StartExtension: MaxLength=%.3f LinkMaxDistance=%.4f invalid, using full pool activeN=%d",
            maxLen, linkMax, self.activeN))
    end

    if forward and type(forward) == "table" and (#forward >= 3) then
        local fx,fy,fz = forward[1], forward[2], forward[3]
        local nx,ny,nz = normalize(fx,fy,fz)
        if nx ~= 0 or ny ~= 0 or nz ~= 0 then self.lastForward = {nx,ny,nz} end
    end
end

function M:StopExtension()
    self.isExtending = false
end

function M:StartRetraction()
    if (self.chainLen or 0) <= 0 then return end
    self.isRetracting = true
    self.isExtending = false
    
    -- Unlock endpoint when retracting
    self.endPointLocked = false
end

-- Detect anchors (corner detection) and record them without mutating invMass here.
-- Anchor application to invMass happens each update so controller remains authoritative.
function M:ComputeAnchors(angleThresholdRad)
    angleThresholdRad = angleThresholdRad or math.rad(45)
    self.anchors = {}
    local aN = self.activeN
    if aN < 3 then return end
    for i = 2, aN - 1 do
        local a = self.positions[i-1]; local b = self.positions[i]; local c = self.positions[i+1]
        local v1x,v1y,v1z = b[1]-a[1], b[2]-a[2], b[3]-a[3]
        local v2x,v2y,v2z = c[1]-b[1], c[2]-b[2], c[3]-b[3]
        local l1 = vec_len(v1x,v1y,v1z)
        local l2 = vec_len(v2x,v2y,v2z)
        if l1 > 1e-6 and l2 > 1e-6 then
            local dotp = (v1x*v2x + v1y*v2y + v1z*v2z) / (l1 * l2)
            if dotp < -1 then dotp = -1 elseif dotp > 1 then dotp = 1 end
            local ang = math.acos(dotp)
            if ang >= angleThresholdRad then
                self.anchors[i] = true
            end
        end
    end
end

-- Perform raycast to detect collision and lock endpoint
function M:PerformRaycast(sx, sy, sz, maxDistance)
    if not Physics or not Physics.Raycast then
        return nil
    end
    
    local fx, fy, fz = self.lastForward[1] or 0, self.lastForward[2] or 0, self.lastForward[3] or 1
    
    -- Normalize direction
    local nx, ny, nz = normalize(fx, fy, fz)
    
    -- Perform raycast
    local hitDistance = Physics.Raycast(sx, sy, sz, nx, ny, nz, maxDistance)
    
    if hitDistance > 0 then
        -- Hit something! Calculate hit point
        local hitX = sx + nx * hitDistance
        local hitY = sy + ny * hitDistance
        local hitZ = sz + nz * hitDistance
        
        return {
            hit = true,
            distance = hitDistance,
            hitPoint = {hitX, hitY, hitZ}
        }
    end
    
    return nil
end

-- Controller update: authoritative chainLen -> produce startPos/endPos, segmentLen and per-link invMass.
-- Returns positions (for transform writing), startPos, endPos.
function M:Update(dt, settings)
    settings = settings or {}

    local chainSpeed = tonumber(settings.ChainSpeed) or tonumber(self.params.ChainSpeed) or 10
    local maxLenSetting = tonumber(settings.MaxLength) or tonumber(self.params.MaxLength) or 0
    local isElastic = (settings.IsElastic ~= nil) and settings.IsElastic or (self.params.IsElastic == true)
    local linkMax = tonumber(settings.LinkMaxDistance) or tonumber(self.params.LinkMaxDistance) or nil

    -- Use activeN for all per-link operations
    local aN = self.activeN

    -- 1) Update authoritative chainLen based on extending/retracting
    if self.isExtending then
        self.extensionTime = self.extensionTime + dt
        local desired = chainSpeed * self.extensionTime
        if maxLenSetting and maxLenSetting > 0 then desired = math.min(desired, maxLenSetting) end
        if not isElastic and linkMax and linkMax > 0 then
            local maxAllowed = linkMax * math.max(1, (aN - 1))
            if desired > maxAllowed then desired = maxAllowed end
        end
        self.chainLen = desired
    end

    if self.isRetracting then
        self.chainLen = math.max(0, (self.chainLen or 0) - chainSpeed * dt)
        if self.chainLen <= 0 then
            self.isRetracting = false
            self.chainLen = 0
            self.endPointLocked = false
        end
    end

    -- 2) Resolve start world position once (use settings.getStart if provided)
    local sx,sy,sz = 0,0,0
    if type(settings.getStart) == "function" then
        sx,sy,sz = settings.getStart()
    elseif settings.startOverride then
        sx,sy,sz = settings.startOverride[1] or 0, settings.startOverride[2] or 0, settings.startOverride[3] or 0
    else
        sx,sy,sz = self.startPos[1] or 0, self.startPos[2] or 0, self.startPos[3] or 0
    end
    self.startPos[1], self.startPos[2], self.startPos[3] = sx, sy, sz

    -- Single downward raycast from start point to get global ground floor for chain clamping.
    -- Only one raycast per frame regardless of link count (O(1)).
    if settings.GroundClamp and Physics and Physics.Raycast then
        local rayLen = 20.0
        local hitDist = Physics.Raycast(sx, sy, sz, 0, -1, 0, rayLen)
        if hitDist and hitDist > 0 then
            self._groundY = sy - hitDist + (settings.GroundClampOffset or 0.1)
        else
            self._groundY = nil
        end
    else
        self._groundY = nil
    end

    -- 3) Determine end world position with raycast collision detection
    local ex,ey,ez
    
    if settings.endOverride then
        ex,ey,ez = settings.endOverride[1] or 0, settings.endOverride[2] or 0, settings.endOverride[3] or 0
    elseif self.endPointLocked then
        ex, ey, ez = self.lockedEndPoint[1], self.lockedEndPoint[2], self.lockedEndPoint[3]
    else
        local fx,fy,fz = self.lastForward[1] or 0, self.lastForward[2] or 0, self.lastForward[3] or 1
        local theoreticalDistance = self.chainLen or 0
        
        if self.isExtending and theoreticalDistance > 0 then
            local raycastResult = self:PerformRaycast(sx, sy, sz, theoreticalDistance * 1.1)
            
            if raycastResult and raycastResult.hit then
                self.endPointLocked = true
                self.lockedEndPoint[1] = raycastResult.hitPoint[1]
                self.lockedEndPoint[2] = raycastResult.hitPoint[2]
                self.lockedEndPoint[3] = raycastResult.hitPoint[3]
                
                ex, ey, ez = raycastResult.hitPoint[1], raycastResult.hitPoint[2], raycastResult.hitPoint[3]
                
                self.chainLen = raycastResult.distance
                self.isExtending = false

                -- Recalculate activeN based on actual hit distance instead of MaxLength
                local linkMaxForSnap = tonumber(settings.LinkMaxDistance) or tonumber(self.params.LinkMaxDistance) or 0
                if linkMaxForSnap > 0 then
                    local needed = math.ceil(raycastResult.distance / linkMaxForSnap) + 1
                    local prevActiveN = self.activeN
                    self.activeN = math.min(needed, self.n)
                    print(string.format("[ChainController] Raycast HIT at distance %.3f, locked endpoint at (%.3f, %.3f, %.3f) | activeN recalculated: %d -> %d",
                        raycastResult.distance, ex, ey, ez, prevActiveN, self.activeN))
                else
                    print(string.format("[ChainController] Raycast HIT at distance %.3f, locked endpoint at (%.3f, %.3f, %.3f)",
                        raycastResult.distance, ex, ey, ez))
                end

            else
                ex = sx + (fx * theoreticalDistance)
                ey = sy + (fy * theoreticalDistance)
                ez = sz + (fz * theoreticalDistance)
            end
        else
            ex = sx + (fx * theoreticalDistance)
            ey = sy + (fy * theoreticalDistance)
            ez = sz + (fz * theoreticalDistance)
        end
    end
    
    self.endPos[1], self.endPos[2], self.endPos[3] = ex, ey, ez

    -- 4) Compute current physical distance and determine totalLen and segmentLen
    local curEndDist = vec_len(ex - sx, ey - sy, ez - sz)
    local totalLen = (self.chainLen and self.chainLen > 1e-8) and self.chainLen or math.max(curEndDist, 1e-6)
    if maxLenSetting and maxLenSetting > 0 then totalLen = math.min(totalLen, maxLenSetting) end

    local segmentLen = (aN > 1) and (totalLen / (aN - 1)) or 0
    if (not isElastic) and linkMax and linkMax > 0 and segmentLen > linkMax then
        segmentLen = linkMax
    end

    -- 5) Determine per-link kinematic state based on authoritative chainLen and anchors.
    -- Links beyond activeN are snapped to start and marked kinematic (pool links not in use).
    for i = 1, self.n do
        if i > aN then
            -- Pool link not in use: snap to start, mark kinematic
            self.positions[i][1], self.positions[i][2], self.positions[i][3] = sx, sy, sz
            self.prev[i][1], self.prev[i][2], self.prev[i][3] = sx, sy, sz
            self.invMass[i] = 0
        else
            local requiredDist = (i - 1) * segmentLen
            if (self.chainLen + 1e-9) < requiredDist then
                -- not deployed yet -> snap to start and mark kinematic
                self.positions[i][1], self.positions[i][2], self.positions[i][3] = sx, sy, sz
                self.prev[i][1], self.prev[i][2], self.prev[i][3] = sx, sy, sz
                self.invMass[i] = 0
            else
                -- deployed => dynamic unless explicitly anchored
                if self.anchors[i] then
                    self.invMass[i] = 0
                else
                    self.invMass[i] = 1
                end
            end
        end
    end

    -- Pin first link to start (ALWAYS kinematic)
    self.invMass[1] = 0

    -- Pin last active link to endpoint when extending OR when endpoint is locked
    if self.isExtending or self.endPointLocked then
        self.positions[aN][1], self.positions[aN][2], self.positions[aN][3] = ex, ey, ez
        self.prev[aN][1], self.prev[aN][2], self.prev[aN][3] = ex, ey, ez
        self.invMass[aN] = 0
    end

    -- 6) Re-apply anchors as kinematic overrides
    --TAKE NOTE THIS FUNCTION IS BUGGED
    --self:ComputeAnchors(settings.AnchorAngleThresholdRad or self.params.AnchorAngleThresholdRad or math.rad(45))
    for idx, _ in pairs(self.anchors) do
        if idx >= 1 and idx <= aN then
            self.invMass[idx] = 0
        end
    end

    -- 7) Build Verlet params and step physics
    local vparams = {
        n = aN,
        VerletGravity = settings.VerletGravity or self.params.VerletGravity,
        VerletDamping = settings.VerletDamping or self.params.VerletDamping,
        ConstraintIterations = settings.ConstraintIterations or self.params.ConstraintIterations,
        IsElastic = isElastic,
        LinkMaxDistance = linkMax,
        totalLen = totalLen,
        segmentLen = segmentLen,
        ClampSegment = linkMax,
        endPointLocked = self.endPointLocked,
        GroundClamp = settings.GroundClamp,
        GroundClampOffset = settings.GroundClampOffset,
        groundY = self._groundY,
        pinnedLast = self.endPointLocked or ((not self.isExtending) and ((self.chainLen or 0) + 1e-9 >= (aN - 1) * segmentLen) and (settings.PinEndWhenExtended or self.params.PinEndWhenExtended)),
        endPos = { ex, ey, ez },
        startPos = { sx, sy, sz }
    }
    
    -- Step Verlet physics (in-place modification of self.positions/self.prev/self.invMass)
    VerletAdapter.Step(self.VerletState, dt, vparams)

    -- 8) After physics, ensure locked endpoint and undeployed links remain kinematic
    if self.endPointLocked then
        self.positions[aN][1], self.positions[aN][2], self.positions[aN][3] = ex, ey, ez
        self.prev[aN][1], self.prev[aN][2], self.prev[aN][3] = ex, ey, ez
        self.invMass[aN] = 0
    elseif vparams.pinnedLast and vparams.endPos then
        self.positions[aN][1], self.positions[aN][2], self.positions[aN][3] = vparams.endPos[1], vparams.endPos[2], vparams.endPos[3]
        self.prev[aN][1], self.prev[aN][2], self.prev[aN][3] = vparams.endPos[1], vparams.endPos[2], vparams.endPos[3]
        self.invMass[aN] = 0
    end

    -- Ensure anchors are preserved post-physics
    for idx, _ in pairs(self.anchors) do
        if idx >= 1 and idx <= aN then
            self.invMass[idx] = 0
        end
    end

    -- Return positions for transform handler, and authoritative start/end positions
    return self.positions, { self.startPos[1], self.startPos[2], self.startPos[3] }, { self.endPos[1], self.endPos[2], self.endPos[3] }
end

function M:GetPublicState()
    return {
        ChainLength = self.chainLen,
        IsExtending = self.isExtending,
        IsRetracting = self.isRetracting,
        LinkCount = self.n,
        ActiveLinkCount = self.activeN,
        Anchors = self.anchors,
        EndPointLocked = self.endPointLocked,
        LockedEndPoint = self.endPointLocked and {self.lockedEndPoint[1], self.lockedEndPoint[2], self.lockedEndPoint[3]} or nil
    }
end

return M