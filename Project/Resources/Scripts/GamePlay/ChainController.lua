-- ChainController.lua (refactored, authoritative chain length, clear responsibilities)
-- Owns logical chain state (chainLen, extending/retracting), anchors, and prepares pure-physics params.
local VerletAdapter = require("extension.VerletAdapter")
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

    self.anchors = {}           -- map index->true; ComputeAnchors only marks anchors (does not mutate invMass)
    self.chainLen = 0.0         -- authoritative logical chain length (meters)
    self.extensionTime = 0.0
    self.isExtending = false
    self.isRetracting = false
    self.lastForward = {0,0,1}  -- use component to set a different forward if desired (StartExtension(forward))
    self.startPos = {0,0,0}
    self.endPos = {0,0,0}

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

-- Accept optional forward vector when starting extension to preserve component-level forward
function M:StartExtension(forward)
    self.isExtending = true
    self.isRetracting = false
    self.extensionTime = 0
    self.chainLen = 0
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
end

-- Detect anchors (corner detection) and record them without mutating invMass here.
-- Anchor application to invMass happens each update so controller remains authoritative.
function M:ComputeAnchors(angleThresholdRad)
    angleThresholdRad = angleThresholdRad or math.rad(45)
    self.anchors = {}
    if self.n < 3 then return end
    for i = 2, self.n - 1 do
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

-- Controller update: authoritative chainLen -> produce startPos/endPos, segmentLen and per-link invMass.
-- Returns positions (for transform writing), startPos, endPos.
function M:Update(dt, settings)
    settings = settings or {}

    local chainSpeed = tonumber(settings.ChainSpeed) or tonumber(self.params.ChainSpeed) or 10
    local maxLenSetting = tonumber(settings.MaxLength) or tonumber(self.params.MaxLength) or 0
    local isElastic = (settings.IsElastic ~= nil) and settings.IsElastic or (self.params.IsElastic == true)
    local linkMax = tonumber(settings.LinkMaxDistance) or tonumber(self.params.LinkMaxDistance) or nil

    -- 1) Update authoritative chainLen based on extending/retracting
    if self.isExtending then
        self.extensionTime = self.extensionTime + dt
        local desired = chainSpeed * self.extensionTime
        if maxLenSetting and maxLenSetting > 0 then desired = math.min(desired, maxLenSetting) end
        if not isElastic and linkMax and linkMax > 0 then
            local maxAllowed = linkMax * math.max(1, (self.n - 1))
            if desired > maxAllowed then desired = maxAllowed end
        end
        self.chainLen = desired
    end

    if self.isRetracting then
        self.chainLen = math.max(0, (self.chainLen or 0) - chainSpeed * dt)
        if self.chainLen <= 0 then
            self.isRetracting = false
            self.chainLen = 0
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

    -- 3) Determine end world position:
    -- If an explicit endOverride passed, use it. Else derive from authoritative chainLen + lastForward.
    local ex,ey,ez
    if settings.endOverride then
        ex,ey,ez = settings.endOverride[1] or 0, settings.endOverride[2] or 0, settings.endOverride[3] or 0
    --elseif self.endPos and (self.endPos[1] ~= nil) and (self.endPos[2] ~= nil) and (self.endPos[3] ~= nil) and (not self.isExtending) then
        -- keep previously set endPos when not actively extending (useful for pinned hit points)
    --    ex,ey,ez = self.endPos[1], self.endPos[2], self.endPos[3]
    else
        -- derive end from start + forward * chainLen
        local fx,fy,fz = self.lastForward[1] or 0, self.lastForward[2] or 0, self.lastForward[3] or 1
        ex = sx + (fx * (self.chainLen or 0))
        ey = sy + (fy * (self.chainLen or 0))
        ez = sz + (fz * (self.chainLen or 0))
    end
    self.endPos[1], self.endPos[2], self.endPos[3] = ex, ey, ez

    -- 4) Compute current physical distance and determine a "totalLen" and segmentLen that physics will try to satisfy.
    local curEndDist = vec_len(ex - sx, ey - sy, ez - sz)
    -- Prefer authoritative chainLen when > 0; fallback to current end distance
    local totalLen = (self.chainLen and self.chainLen > 1e-8) and self.chainLen or math.max(curEndDist, 1e-6)
    -- However, respect MaxLength if configured (physics shouldn't expand beyond this)
    if maxLenSetting and maxLenSetting > 0 then totalLen = math.min(totalLen, maxLenSetting) end

    local segmentLen = (self.n > 1) and (totalLen / (self.n - 1)) or 0
    if (not isElastic) and linkMax and linkMax > 0 and segmentLen > linkMax then
        segmentLen = linkMax
    end

    -- 5) Determine per-link kinematic state based on authoritative chainLen and anchors.
    for i = 1, self.n do
        local requiredDist = (i - 1) * segmentLen
        if (self.chainLen + 1e-9) < requiredDist then
            -- not deployed -> snap to start and mark kinematic (invMass=0)
            self.positions[i][1], self.positions[i][2], self.positions[i][3] = sx, sy, sz
            self.prev[i][1], self.prev[i][2], self.prev[i][3] = sx, sy, sz
            self.invMass[i] = 0
        else
            -- deployed => dynamic unless explicitly anchored by corner detection
            if self.anchors[i] then
                self.invMass[i] = 0
            else
                self.invMass[i] = 1
            end
        end
    end

    -- NEW: During extension, pin the last link to the endpoint so it extends in the right direction
    if self.isExtending then
        self.positions[self.n][1], self.positions[self.n][2], self.positions[self.n][3] = ex, ey, ez
        self.prev[self.n][1], self.prev[self.n][2], self.prev[self.n][3] = ex, ey, ez
        self.invMass[self.n] = 0
    end

    -- 6) Compute anchors from current geometry; anchors only set marker table (ComputeAnchors won't mutate invMass)
    --TAKE NOTE THIS FUNCTION IS BUGGED
    --self:ComputeAnchors(settings.AnchorAngleThresholdRad or self.params.AnchorAngleThresholdRad or math.rad(45))

    -- Re-apply anchors as kinematic overrides (anchors should always be kinematic)
    for idx, _ in pairs(self.anchors) do
        if idx >= 1 and idx <= self.n then
            self.invMass[idx] = 0
        end
    end

    -- 7) Build Verlet params and step physics
    local vparams = {
        VerletGravity = settings.VerletGravity or self.params.VerletGravity,
        VerletDamping = settings.VerletDamping or self.params.VerletDamping,
        ConstraintIterations = settings.ConstraintIterations or self.params.ConstraintIterations,
        IsElastic = isElastic,
        LinkMaxDistance = linkMax,
        totalLen = totalLen,
        segmentLen = segmentLen,
        ClampSegment = linkMax,
        pinnedLast = (not self.isExtending) and ((self.chainLen or 0) + 1e-9 >= (self.n - 1) * segmentLen) and (settings.PinEndWhenExtended or self.params.PinEndWhenExtended),
        endPos = { ex, ey, ez },
        startPos = { sx, sy, sz }
    }

    -- Step Verlet physics (in-place modification of self.positions/self.prev/self.invMass)
    VerletAdapter.Step(self.VerletState, dt, vparams)

    -- 8) After physics, ensure anchors and undeployed links remain kinematic (defensive)
    if vparams.pinnedLast and vparams.endPos then
        local li = self.n
        self.positions[li][1], self.positions[li][2], self.positions[li][3] = vparams.endPos[1], vparams.endPos[2], vparams.endPos[3]
        self.prev[li][1], self.prev[li][2], self.prev[li][3] = vparams.endPos[1], vparams.endPos[2], vparams.endPos[3]
        self.invMass[li] = 0
    end

    -- ensure anchors are preserved
    for idx, _ in pairs(self.anchors) do
        if idx >= 1 and idx <= self.n then
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
        Anchors = self.anchors
    }
end

return M
