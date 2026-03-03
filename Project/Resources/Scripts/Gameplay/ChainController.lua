-- ChainController.lua
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

    self.positions = {}
    self.prev = {}
    self.invMass = {}
    for i = 1, self.n do
        self.positions[i] = {0,0,0}
        self.prev[i] = {0,0,0}
        self.invMass[i] = 1
    end

    self.activeN = self.n
    self.anchors = {}
    self.chainLen = 0.0
    self.extensionTime = 0.0
    self.isExtending = false
    self.isRetracting = false
    self.lastForward = {0,0,1}
    self.startPos = {0,0,0}
    self.endPos = {0,0,0}

    self.endPointLocked = false     -- true only after OnTriggerEnter fires via chain.endpoint_hit_entity
    self.lockedEndPoint = {0,0,0}   -- updated every frame by ChainEndpointController while hooked
    self.hookedTag = ""             -- tag of the hooked entity root, set at hit time

    self._raycastSnapped = false    -- true after raycast hit, before trigger fires
    self._lockedChainLen = 0.0      -- chainLen at moment retraction begins
    self._flopping = false          -- true when max distance reached with no hit — end link is free

    self.VerletState = VerletAdapter.Init{ positions = self.positions, prev = self.prev, invMass = self.invMass }
    return setmetatable(self, { __index = M })
end

function M:SetStartPos(x,y,z)
    self.startPos = {x or 0, y or 0, z or 0}
end

function M:SetEndPos(x,y,z)
    self.endPos = {x or 0, y or 0, z or 0}
end

function M:StartExtension(forward, maxLength, linkMaxDistance)
    self.isExtending = true
    self.isRetracting = false
    self.extensionTime = 0
    self.chainLen = 0
    self._lockedChainLen = 0
    self._raycastSnapped = false
    self._flopping = false

    self.endPointLocked = false
    self.lockedEndPoint = {0,0,0}
    self.hookedTag = ""

    local maxLen = tonumber(maxLength) or tonumber(self.params.MaxLength) or 0
    local linkMax = tonumber(linkMaxDistance) or tonumber(self.params.LinkMaxDistance) or 0
    if linkMax > 0 and maxLen > 0 then
        local needed = math.ceil(maxLen / linkMax) + 1
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
    self._raycastSnapped = false
    self._flopping = false
    -- Snapshot chainLen at moment retraction begins so we can interpolate
    -- from lockedEndPoint back to startPos correctly
    self._lockedChainLen = self.chainLen
    -- Do NOT clear endPointLocked here — Update clears it when chainLen hits 0
end

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

function M:PerformRaycast(sx, sy, sz, maxDistance)
    if not Physics or not Physics.Raycast then return nil end
    local fx, fy, fz = self.lastForward[1] or 0, self.lastForward[2] or 0, self.lastForward[3] or 1
    local nx, ny, nz = normalize(fx, fy, fz)
    local hitDistance = Physics.Raycast(sx, sy, sz, nx, ny, nz, maxDistance)
    if hitDistance > 0 then
        return {
            hit = true,
            distance = hitDistance,
            hitPoint = {
                sx + nx * hitDistance,
                sy + ny * hitDistance,
                sz + nz * hitDistance
            }
        }
    end
    return nil
end

function M:Update(dt, settings)
    settings = settings or {}

    local chainSpeed    = tonumber(settings.ChainSpeed)      or tonumber(self.params.ChainSpeed)      or 10
    local maxLenSetting = tonumber(settings.MaxLength)       or tonumber(self.params.MaxLength)       or 0
    local isElastic     = (settings.IsElastic ~= nil) and settings.IsElastic or (self.params.IsElastic == true)
    local linkMax       = tonumber(settings.LinkMaxDistance) or tonumber(self.params.LinkMaxDistance) or nil

    local aN = self.activeN

    -- 1) Update chainLen
    if self.isExtending then
        self.extensionTime = self.extensionTime + dt
        local desired = chainSpeed * self.extensionTime
        if maxLenSetting and maxLenSetting > 0 then desired = math.min(desired, maxLenSetting) end
        if not isElastic and linkMax and linkMax > 0 then
            local maxAllowed = linkMax * math.max(1, (aN - 1))
            if desired > maxAllowed then desired = maxAllowed end
        end
        self.chainLen = desired
        -- Max distance reached with no raycast hit — release end link to flop freely
        if maxLenSetting and maxLenSetting > 0 and desired >= maxLenSetting and not self._raycastSnapped and not self.endPointLocked then
            self.isExtending = false
            self._flopping = true
        end
    end

    if self.isRetracting then
        self.chainLen = math.max(0, (self.chainLen or 0) - chainSpeed * dt)
        if self.chainLen <= 0 then
            self.isRetracting = false
            self.chainLen = 0
            self.endPointLocked = false
            self._raycastSnapped = false
        end
    end

    -- 2) Resolve start world position
    local sx,sy,sz = 0,0,0
    if type(settings.getStart) == "function" then
        sx,sy,sz = settings.getStart()
    elseif settings.startOverride then
        sx,sy,sz = settings.startOverride[1] or 0, settings.startOverride[2] or 0, settings.startOverride[3] or 0
    else
        sx,sy,sz = self.startPos[1] or 0, self.startPos[2] or 0, self.startPos[3] or 0
    end
    self.startPos[1], self.startPos[2], self.startPos[3] = sx, sy, sz

    -- Movement constraint: active when hooked (endPointLocked) OR snapped to ground (raycastSnapped).
    -- Stores result on self.constraintResult for ChainBootstrap to publish.
    local constraintActive = (self.endPointLocked or self._raycastSnapped) and not self.isRetracting and not self._flopping
    if constraintActive then
        local slack    = tonumber(settings.ChainSlackDistance) or 0
        local dragTag  = settings.DragTag or ""
        local ex0 = self.lockedEndPoint[1]
        local ey0 = self.lockedEndPoint[2]
        local ez0 = self.lockedEndPoint[3]
        local playerDist = vec_len(sx - ex0, sy - ey0, sz - ez0)
        local chainLength = self.chainLen or 0
        local isDragType = (dragTag ~= "" and self.hookedTag == dragTag)
        local hardLimit = chainLength + slack

        print(string.format("[ChainController][CONSTRAINT] locked=%s snapped=%s playerDist=%.3f chainLen=%.3f slack=%.3f hardLimit=%.3f",
            tostring(self.endPointLocked), tostring(self._raycastSnapped),
            playerDist, chainLength, slack, hardLimit))

        if isDragType then
            if playerDist > chainLength + 1e-4 then
                local dx = sx - ex0
                local dy = sy - ey0
                local dz = sz - ez0
                local dist = vec_len(dx, dy, dz)
                if dist > 1e-6 then
                    local nx, ny, nz = dx/dist, dy/dist, dz/dist
                    self.constraintResult = {
                        ratio = 0, exceeded = false, drag = true,
                        targetX = ex0 + nx * chainLength,
                        targetY = ey0 + ny * chainLength,
                        targetZ = ez0 + nz * chainLength,
                    }
                end
            else
                self.constraintResult = { ratio = 0, exceeded = false, drag = false }
            end
        else
            local ratio = 0
            if slack > 1e-6 then
                ratio = math.max(0, math.min(1, (playerDist - chainLength) / slack))
            end
            if playerDist > hardLimit + 1e-4 then
                print("[ChainController][CONSTRAINT] EXCEEDED -> flopping")
                self.endPointLocked = false
                self._raycastSnapped = false
                self._flopping = true
                self.hookedTag = ""
                self.constraintResult = { ratio = 0, exceeded = true, drag = false }
            else
                self.constraintResult = { ratio = ratio, exceeded = false, drag = false }
            end
        end
    else
        self.constraintResult = { ratio = 0, exceeded = false, drag = false }
    end

    -- Ground clamp (single downward ray, O(1))
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

    -- 3) Determine end world position
    local ex,ey,ez
    local fx,fy,fz = self.lastForward[1] or 0, self.lastForward[2] or 0, self.lastForward[3] or 1

    if settings.endOverride then
        -- Explicit override always wins
        ex,ey,ez = settings.endOverride[1] or 0, settings.endOverride[2] or 0, settings.endOverride[3] or 0

    elseif self.endPointLocked and not self.isRetracting then
        -- Trigger has fired and endpoint is parented to enemy.
        -- lockedEndPoint is updated every frame by ChainEndpointController
        -- reading its own parented world position back from the engine.
        ex, ey, ez = self.lockedEndPoint[1], self.lockedEndPoint[2], self.lockedEndPoint[3]

    elseif self.isRetracting then
        -- Retract from lockedEndPoint toward startPos along the actual vector
        -- between them, proportional to remaining chainLen
        local lx = self.lockedEndPoint[1]
        local ly = self.lockedEndPoint[2]
        local lz = self.lockedEndPoint[3]
        local dx = lx - sx
        local dy = ly - sy
        local dz = lz - sz
        local fullDist = vec_len(dx, dy, dz)
        if fullDist > 1e-6 then
            local t = self.chainLen / (self._lockedChainLen > 0 and self._lockedChainLen or fullDist)
            if t < 0 then t = 0 end
            if t > 1 then t = 1 end
            ex = sx + dx * t
            ey = sy + dy * t
            ez = sz + dz * t
        else
            ex = sx
            ey = sy
            ez = sz
        end

    elseif self._raycastSnapped then
        -- Raycast hit, but trigger hasn't fired yet.
        -- Hold endpoint exactly at the snapped world position — do not
        -- recompute from player position so it doesn't drift with the player.
        ex, ey, ez = self.lockedEndPoint[1], self.lockedEndPoint[2], self.lockedEndPoint[3]

    elseif self.isExtending then
        -- Actively extending: check for raycast hit
        local theoreticalDistance = self.chainLen or 0
        local raycastResult = self:PerformRaycast(sx, sy, sz, theoreticalDistance * 1.1)

        if raycastResult and raycastResult.hit then
            -- Raycast hit: stop extension at hit distance, snap endpoint there.
            -- Do NOT set endPointLocked — only OnTriggerEnter does that.
            -- _raycastSnapped holds the endpoint in place until the trigger fires.
            self.chainLen = raycastResult.distance
            self.isExtending = false
            self._raycastSnapped = true

            -- Recalculate activeN based on actual hit distance
            local linkMaxForSnap = tonumber(settings.LinkMaxDistance) or tonumber(self.params.LinkMaxDistance) or 0
            if linkMaxForSnap > 0 then
                local needed = math.ceil(raycastResult.distance / linkMaxForSnap) + 1
                local prevActiveN = self.activeN
                self.activeN = math.min(needed, self.n)
                aN = self.activeN
                print(string.format("[ChainController] Raycast HIT at distance %.3f, snapped to (%.3f,%.3f,%.3f) | activeN: %d -> %d",
                    raycastResult.distance,
                    raycastResult.hitPoint[1], raycastResult.hitPoint[2], raycastResult.hitPoint[3],
                    prevActiveN, self.activeN))
            else
                print(string.format("[ChainController] Raycast HIT at distance %.3f, snapped to (%.3f,%.3f,%.3f)",
                    raycastResult.distance,
                    raycastResult.hitPoint[1], raycastResult.hitPoint[2], raycastResult.hitPoint[3]))
            end

            -- Store snap position in lockedEndPoint so _raycastSnapped branch
            -- and retraction both have a stable world position to work from
            self.lockedEndPoint[1] = raycastResult.hitPoint[1]
            self.lockedEndPoint[2] = raycastResult.hitPoint[2]
            self.lockedEndPoint[3] = raycastResult.hitPoint[3]

            ex, ey, ez = raycastResult.hitPoint[1], raycastResult.hitPoint[2], raycastResult.hitPoint[3]
        else
            ex = sx + (fx * theoreticalDistance)
            ey = sy + (fy * theoreticalDistance)
            ez = sz + (fz * theoreticalDistance)
        end

    elseif self._flopping then
        -- End link is free — physics owns its position. Read it back so endPos
        -- and the endpoint object stay in sync with where the last link actually is.
        ex = self.positions[aN][1]
        ey = self.positions[aN][2]
        ez = self.positions[aN][3]

    else
        -- Idle / fully retracted
        local theoreticalDistance = self.chainLen or 0
        ex = sx + (fx * theoreticalDistance)
        ey = sy + (fy * theoreticalDistance)
        ez = sz + (fz * theoreticalDistance)
    end

    self.endPos[1], self.endPos[2], self.endPos[3] = ex, ey, ez

    -- 4) Physical distance and segment length
    local curEndDist = vec_len(ex - sx, ey - sy, ez - sz)
    local totalLen
    if self._flopping then
        -- Physics owns the end — measure actual distance so constraints don't fight gravity
        totalLen = math.max(curEndDist, 1e-6)
    else
        totalLen = (self.chainLen and self.chainLen > 1e-8) and self.chainLen or math.max(curEndDist, 1e-6)
        if maxLenSetting and maxLenSetting > 0 then totalLen = math.min(totalLen, maxLenSetting) end
    end

    local segmentLen
    if self._flopping then
        -- Freeze segment length at the natural rest length from when extension ended.
        -- Using curEndDist here would compress all links into a spring — wrong.
        local restLen = (maxLenSetting and maxLenSetting > 0) and maxLenSetting or math.max(curEndDist, 1e-6)
        segmentLen = (aN > 1) and (restLen / (aN - 1)) or 0
        if (not isElastic) and linkMax and linkMax > 0 and segmentLen > linkMax then
            segmentLen = linkMax
        end
    else
        segmentLen = (aN > 1) and (totalLen / (aN - 1)) or 0
        if (not isElastic) and linkMax and linkMax > 0 and segmentLen > linkMax then
            segmentLen = linkMax
        end
    end

    -- 5) Per-link kinematic state
    for i = 1, self.n do
        if i > aN then
            self.positions[i][1], self.positions[i][2], self.positions[i][3] = sx, sy, sz
            self.prev[i][1], self.prev[i][2], self.prev[i][3] = sx, sy, sz
            self.invMass[i] = 0
        else
            local requiredDist = (i - 1) * segmentLen
            if (self.chainLen + 1e-9) < requiredDist then
                self.positions[i][1], self.positions[i][2], self.positions[i][3] = sx, sy, sz
                self.prev[i][1], self.prev[i][2], self.prev[i][3] = sx, sy, sz
                self.invMass[i] = 0
            else
                if self.anchors[i] then
                    self.invMass[i] = 0
                else
                    self.invMass[i] = 1
                end
            end
        end
    end

    -- Pin first link to start (always kinematic)
    self.invMass[1] = 0

    -- Pin last active link to endpoint when extending, snapped, or locked — NOT when flopping
    if (self.isExtending or self._raycastSnapped or self.endPointLocked) and not self._flopping then
        self.positions[aN][1], self.positions[aN][2], self.positions[aN][3] = ex, ey, ez
        self.prev[aN][1], self.prev[aN][2], self.prev[aN][3] = ex, ey, ez
        self.invMass[aN] = 0
    end

    -- 6) Re-apply anchors
    for idx, _ in pairs(self.anchors) do
        if idx >= 1 and idx <= aN then
            self.invMass[idx] = 0
        end
    end

    -- 7) Verlet physics step
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
        endPointLocked = self.endPointLocked or self._raycastSnapped,
        GroundClamp = settings.GroundClamp,
        GroundClampOffset = settings.GroundClampOffset,
        groundY = self._groundY,
        pinnedLast = (not self._flopping) and (self.endPointLocked or self._raycastSnapped or
                     ((not self.isExtending) and
                      ((self.chainLen or 0) + 1e-9 >= (aN - 1) * segmentLen) and
                      (settings.PinEndWhenExtended or self.params.PinEndWhenExtended))),
        endPos  = { ex, ey, ez },
        startPos = { sx, sy, sz }
    }

    VerletAdapter.Step(self.VerletState, dt, vparams)

    if settings.WallClamp then
        vparams.WallClamp = true
        vparams.WallClampInterval = settings.WallClampInterval or 10
        vparams.WallClampRadius = settings.WallClampRadius or 0
        VerletAdapter.ApplyWallClamp(self.VerletState, vparams)
    end

    -- 8) Post-physics: enforce locked/snapped endpoint and undeployed links
    if (self.endPointLocked or self._raycastSnapped) and not self._flopping then
        self.positions[aN][1], self.positions[aN][2], self.positions[aN][3] = ex, ey, ez
        self.prev[aN][1], self.prev[aN][2], self.prev[aN][3] = ex, ey, ez
        self.invMass[aN] = 0
    elseif vparams.pinnedLast and vparams.endPos then
        self.positions[aN][1], self.positions[aN][2], self.positions[aN][3] = vparams.endPos[1], vparams.endPos[2], vparams.endPos[3]
        self.prev[aN][1], self.prev[aN][2], self.prev[aN][3] = vparams.endPos[1], vparams.endPos[2], vparams.endPos[3]
        self.invMass[aN] = 0
    end

    -- Preserve anchors post-physics
    for idx, _ in pairs(self.anchors) do
        if idx >= 1 and idx <= aN then
            self.invMass[idx] = 0
        end
    end

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
        RaycastSnapped = self._raycastSnapped,
        Flopping = self._flopping,
        LockedEndPoint = (self.endPointLocked or self._raycastSnapped) and
                         {self.lockedEndPoint[1], self.lockedEndPoint[2], self.lockedEndPoint[3]} or nil
    }
end

return M