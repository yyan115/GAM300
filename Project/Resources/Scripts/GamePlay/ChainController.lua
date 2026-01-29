-- ChainController.lua
-- Owns positions[], prevPositions[], invMass[], anchors and runtime chain state
local VerletAdapter = require("extension.VerletAdapter")
local M = {}

function M.New(params)
    local self = {}
    self.params = params or {}
    self.n = tonumber(self.params.NumberOfLinks) or 1
    -- allocate arrays once
    self.positions = {}
    self.prev = {}
    self.invMass = {}
    for i = 1, self.n do
        self.positions[i] = {0,0,0}
        self.prev[i] = {0,0,0}
        self.invMass[i] = 1
    end
    self.anchors = {}           -- map index->true for kinematic anchors (besides start/end)
    self.chainLen = 0.0         -- authoritative chain visible to editor
    self.extensionTime = 0.0
    self.isExtending = false
    self.isRetracting = false
    self.lastForward = {0,0,1}
    self.VerletState = VerletAdapter.Init{ positions = self.positions, prev = self.prev, invMass = self.invMass }
    return setmetatable(self, { __index = M })
end

function M:SetStartPos(x,y,z)
    self.startPos = {x,y,z}
end
function M:SetEndPos(x,y,z)
    self.endPos = {x,y,z}
end

function M:StartExtension()
    self.isExtending = true
    self.isRetracting = false
    self.extensionTime = 0
    self.chainLen = 0
end
function M:StopExtension()
    self.isExtending = false
end
function M:StartRetraction()
    if self.chainLen <= 0 then return end
    self.isRetracting = true
    self.isExtending = false
end

-- compute anchors based on geometry: detect large angle changes and create anchor there
function M:ComputeAnchors(angleThresholdRad)
    angleThresholdRad = angleThresholdRad or math.rad(45)
    self.anchors = {}
    local n = self.n
    if n < 3 then return end
    for i = 2, n-1 do
        local a = self.positions[i-1]; local b = self.positions[i]; local c = self.positions[i+1]
        local v1x,v1y,v1z = b[1]-a[1], b[2]-a[2], b[3]-a[3]
        local v2x,v2y,v2z = c[1]-b[1], c[2]-b[2], c[3]-b[3]
        local l1 = math.sqrt(v1x*v1x + v1y*v1y + v1z*v1z)
        local l2 = math.sqrt(v2x*v2x + v2y*v2y + v2z*v2z)
        if l1 > 1e-6 and l2 > 1e-6 then
            local dotp = (v1x*v2x + v1y*v2y + v1z*v2z) / (l1*l2)
            if dotp < -1 then dotp = -1 elseif dotp > 1 then dotp = 1 end
            local ang = math.acos(dotp)
            if ang >= angleThresholdRad then
                self.anchors[i] = true
                -- treat anchors as kinematic for Verlet
                self.invMass[i] = 0
            else
                if self.invMass[i] == 0 and not self.anchors[i] then
                    self.invMass[i] = 1
                end
            end
        end
    end
end


function M:Update(dt, settings)
    settings = settings or {}
    -- update desired chain length when extending/retracting
    if self.isExtending then
        self.extensionTime = self.extensionTime + dt
        local desired = (settings.ChainSpeed or 10) * self.extensionTime
        if settings.MaxLength and settings.MaxLength > 0 then desired = math.min(desired, settings.MaxLength) end
        -- clamp when inelastic
        if not settings.IsElastic then
            local maxAllowed = (settings.LinkMaxDistance or 0.025) * math.max(1, self.n - 1)
            if desired > maxAllowed then desired = maxAllowed end
        end
        self.chainLen = desired
    end
    if self.isRetracting then
        self.chainLen = math.max(0, self.chainLen - (settings.ChainSpeed or 10) * dt)
        if self.chainLen <= 0 then
            self.isRetracting = false
        end
    end

    -- compute start world and end world positions via callbacks if provided in settings (component)
    local sx,sy,sz = 0,0,0
    if type(settings.getStart) == "function" then
        sx,sy,sz = settings.getStart()
    elseif settings.startOverride then
        sx,sy,sz = settings.startOverride[1], settings.startOverride[2], settings.startOverride[3]
    end
    local sx, sy, sz = 0, 0, 0
    if type(settings.getStart) == "function" then
        sx, sy, sz = settings.getStart()
    elseif settings.startOverride then
        sx, sy, sz = settings.startOverride[1], settings.startOverride[2], settings.startOverride[3]
    end

    local ex, ey, ez
    if settings.endOverride then
        ex, ey, ez = settings.endOverride[1], settings.endOverride[2], settings.endOverride[3]
    elseif self.endPos then
        ex, ey, ez = self.endPos[1], self.endPos[2], self.endPos[3]
    else
        ex, ey, ez = sx, sy, sz
    end

    self.startPos = { sx, sy, sz }
    self.endPos = self.endPos or { ex, ey, ez }

    -- compute totalLen and segmentLen for VerletAdapter
    local curEndDist = math.sqrt((self.endPos[1]-sx)^2 + (self.endPos[2]-sy)^2 + (self.endPos[3]-sz)^2)
    local totalLen = (settings.MaxLength and settings.MaxLength > 0) and settings.MaxLength or math.max(curEndDist, 1e-6)
    local segmentLen = (self.n > 1) and totalLen / (self.n - 1) or 0

    -- set per-link kinematic flags based on chainLen and segmentLen
    for i = 1, self.n do
        local requiredDist = (i - 1) * segmentLen
        if self.chainLen + 1e-6 < requiredDist then
            self.invMass[i] = 0
            self.positions[i][1], self.positions[i][2], self.positions[i][3] = sx, sy, sz
            self.prev[i][1], self.prev[i][2], self.prev[i][3] = sx, sy, sz
        else
            if i ~= 1 and (self.invMass[i] == 0) and (not self.anchors[i]) then
                self.invMass[i] = 1
            end
        end
    end

    -- compute anchors by angle heuristic (corner detection)
    self:ComputeAnchors(settings.AnchorAngleThresholdRad or math.rad(45))

    -- Prepare params for VerletAdapter
    local vparams = {
        VerletGravity = settings.VerletGravity,
        VerletDamping = settings.VerletDamping,
        ConstraintIterations = settings.ConstraintIterations,
        IsElastic = settings.IsElastic,
        LinkMaxDistance = settings.LinkMaxDistance,
        totalLen = totalLen,
        segmentLen = segmentLen,
        ClampSegment = settings.LinkMaxDistance,
        pinnedLast = (not self.isExtending) and (self.chainLen + 1e-6 >= (self.n - 1) * segmentLen) and settings.PinEndWhenExtended,
        endPos = self.endPos,
        startPos = self.startPos
    }

    -- Step Verlet
    VerletAdapter.Step(self.VerletState, dt, vparams)

    -- After verlet, provide positions for transform handler and rotation computation
    return self.positions, self.startPos, self.endPos
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
