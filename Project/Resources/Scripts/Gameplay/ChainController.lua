-- ChainController.lua
_G.CHAIN_DEBUG = _G.CHAIN_DEBUG ~= nil and _G.CHAIN_DEBUG or false
local function dbg(...) if _G.CHAIN_DEBUG then print(...) end end
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

    self.endPointLocked = false
    self.lockedEndPoint = {0,0,0}
    self.hookedTag = ""

    self._raycastSnapped = false
    self._lockedChainLen = 0.0
    self._flopping = false

    -- LOS anchor list: ordered {x,y,z} world positions where the chain bends
    -- around occluding geometry. Managed by UpdateLOSAnchors every frame.
    self.losAnchors = {}

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
    self.losAnchors = {}

    local maxLen = tonumber(maxLength) or tonumber(self.params.MaxLength) or 0
    local linkMax = tonumber(linkMaxDistance) or tonumber(self.params.LinkMaxDistance) or 0
    if linkMax > 0 and maxLen > 0 then
        local needed = math.ceil(maxLen / linkMax) + 1
        self.activeN = math.min(needed, self.n)
        dbg(string.format("[ChainController] StartExtension: MaxLength=%.3f LinkMaxDistance=%.4f needed=%d poolSize=%d activeN=%d",
            maxLen, linkMax, needed, self.n, self.activeN))
    else
        self.activeN = self.n
        dbg(string.format("[ChainController] StartExtension: MaxLength=%.3f LinkMaxDistance=%.4f invalid, using full pool activeN=%d",
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
    self.hookedTag = ""
    self.losAnchors = {}
    self._lockedChainLen = self.chainLen
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

-- ---------------------------------------------------------------------------
-- LOS anchor system
-- ---------------------------------------------------------------------------
-- Each anchor entry: { x, y, z, bodyId, nx, ny, nz }
--   x,y,z   world position (slides each frame along geometry)
--   bodyId  persistent physics body that caused this anchor (from RaycastFull)
--   nx,ny,nz hit normal at placement (used to offset anchor flush to surface)
--
-- DESIGN: Hybrid incremental — separate remove and add passes that operate on
-- different parts of the path, so they cannot conflict within a frame.
--
-- WHY INCREMENTAL (not rebuild-from-scratch):
--   Pure rebuild has no memory of which side of an obstacle the chain committed
--   to.  If the player moves to a position where startPos→endPos is now a clear
--   raycast, rebuild produces zero anchors — the chain snaps straight through
--   the pillar it wrapped around.  bodyId lets us catch this: if a body that
--   caused an anchor last frame no longer appears in the greedy walk this frame,
--   we shoot startPos→endPos to check whether that body is still topologically
--   between the two endpoints.  If it is, the chain is still wrapped — keep the
--   anchor even though the greedy walk missed it.
--
-- WHY NO OSCILLATION (unlike the original two-pass-on-one-list approach):
--   Remove and add passes operate on different segments of the path.
--   Remove checks existing anchor chords (prev→next).
--   Add extends only the frontier from the last surviving anchor toward endPos.
--   They share no mutable state within the same frame, so neither pass
--   invalidates the other's inputs.
--
-- ANCHOR SLIDING (hitNormal):
--   Anchor position = hitPoint - hitNormal * SURFACE_OFFSET, placing it flush
--   against the surface.  Each frame the anchor is re-derived from a fresh
--   raycast, so it naturally slides along geometry as player moves vertically.
-- ---------------------------------------------------------------------------
local LOS_NUDGE          = 0.06   -- metres offset along normal from surface (flush placement)
local LOS_MIN_DIST       = 0.03   -- skip raycasts on segments shorter than this
local LOS_CLEAR_SLACK    = 0.06   -- hit within this of the far endpoint counts as "clear"
local LOS_MAX_ANCHORS    = 16     -- hard cap against degenerate geometry
local LOS_CLEAR_FRAMES   = 2      -- consecutive clear frames before an anchor is dissolved
                                   -- prevents one-frame oscillation when player is close to occluder

-- ---------------------------------------------------------------------------
-- RaycastFull unpacking
-- ---------------------------------------------------------------------------
-- C++ returns std::tuple<bool,float,float,float,float,float,float,float,uint32_t>.
-- Depending on the Lua binding this arrives as EITHER:
--   (a) 9 separate return values  — pcall gives ok, hit, dist, px...bodyId
--   (b) a single Lua table        — pcall gives ok, r (where r[1]=hit, r[2]=dist...)
-- We detect which case we're in by checking the type of the second pcall result.
-- ---------------------------------------------------------------------------
local function los_raycast(ox,oy,oz, dx,dy,dz, maxDist)
    if Physics.RaycastFull then
        local ok, a, b, c, d, e, f, g, h, k =
            pcall(function()
                return Physics.RaycastFull(ox,oy,oz, dx,dy,dz, maxDist)
            end)
        if ok then
            local hit, dist, px, py, pz, nx, ny, nz, bodyId
            if type(a) == "table" then
                -- Binding returned a single table (case b)
                hit = a[1]; dist = a[2]
                px = a[3]; py = a[4]; pz = a[5]
                nx = a[6]; ny = a[7]; nz = a[8]
                bodyId = a[9]
            else
                -- Binding returned multiple values (case a)
                hit = a;  dist = b
                px = c;   py = d;   pz = e
                nx = f;   ny = g;   nz = h
                bodyId = k
            end
            if hit and dist and dist > 0 then
                return true, dist, px,py,pz, nx,ny,nz, bodyId
            end
        end
        return false
    end
    -- Fallback: plain Raycast — synthesise hitPoint, no normal/bodyId
    local dist = Physics.Raycast(ox,oy,oz, dx,dy,dz, maxDist)
    if dist and dist > 0 then
        return true, dist, ox+dx*dist, oy+dy*dist, oz+dz*dist, 0,1,0, nil
    end
    return false
end

-- Place a single anchor from a RaycastFull result.
-- Position is offset LOS_NUDGE along the surface normal so it sits flush
-- against the geometry edge rather than floating in free space before the wall.
local function make_anchor(ox,oy,oz, dirX,dirY,dirZ, dist, px,py,pz, nx,ny,nz, bodyId)
    local ax, ay, az
    nx = nx or 0; ny = ny or 0; nz = nz or 0
    local nlen = math.sqrt(nx*nx + ny*ny + nz*nz)
    if nlen > 0.01 then
        nx, ny, nz = nx/nlen, ny/nlen, nz/nlen
        ax = px + nx * LOS_NUDGE
        ay = py + ny * LOS_NUDGE
        az = pz + nz * LOS_NUDGE
    else
        -- No valid normal: nudge back along ray direction
        ax = ox + dirX * (dist - LOS_NUDGE)
        ay = oy + dirY * (dist - LOS_NUDGE)
        az = oz + dirZ * (dist - LOS_NUDGE)
        nx, ny, nz = -dirX, -dirY, -dirZ
    end
    return { ax, ay, az, bodyId = bodyId, nx = nx, ny = ny, nz = nz, clearFrames = 0 }
end

function M:UpdateLOSAnchors(sx, sy, sz, ex, ey, ez)
    if not Physics then self.losAnchors = {} return end

    -- -------------------------------------------------------------------------
    -- REMOVE PASS
    -- For each anchor, check two conditions for dissolution:
    --   (a) chord prev→next is geometrically clear
    --   (b) the chord prev→next does NOT hit this anchor's own bodyId
    --       (wrapping guard — if removing this anchor would make the chain
    --       clip through the body it wraps, keep it)
    -- To prevent flicker when the player is very close to an occluder, we
    -- require the anchor to be clear for LOS_CLEAR_FRAMES consecutive frames
    -- before actually removing it.  clearFrames is reset to 0 on any blocked
    -- frame so a single obstruction re-locks the anchor immediately.
    --
    -- SLIDING: when an anchor survives, we re-derive its world position by
    -- shooting from its predecessor toward the anchor's OWN stored position
    -- (not the chord direction).  This guarantees we always re-hit the same
    -- surface patch regardless of where next_ has moved, so the anchor slides
    -- naturally along geometry as the player moves up/down or sideways.
    -- -------------------------------------------------------------------------

    -- Topology unwrap test: shoot directly from startPos (player) to endPos.
    -- If that ray hits anchor.bodyId, the obstacle is still topologically between
    -- the two endpoints — the chain is still wrapped, keep the anchor.
    -- If the ray does NOT hit anchor.bodyId, the player and endpoint are on the
    -- same side of the obstacle → the wrapping has resolved → allow removal.
    -- This is the correct test: it answers "are player and endpoint on the same
    -- side?" rather than "does the local chord clip the body?" (the local chord
    -- from prev→next always passes through the pillar when the player is far away,
    -- which is what caused the stickiness).
    local _directSideCache = {}
    local function isStillTopologicallyWrapped(anchor)
        if anchor.bodyId == nil then return false end
        if _directSideCache[anchor.bodyId] ~= nil then
            return _directSideCache[anchor.bodyId]
        end
        local dx, dy, dz = ex-sx, ey-sy, ez-sz
        local dist = math.sqrt(dx*dx+dy*dy+dz*dz)
        if dist < LOS_MIN_DIST then
            _directSideCache[anchor.bodyId] = false
            return false
        end
        local ndx, ndy, ndz = dx/dist, dy/dist, dz/dist
        local hit, _, _,_,_, _,_,_, hitBodyId =
            los_raycast(sx, sy, sz, ndx, ndy, ndz, dist)
        local wrapped = hit and (hitBodyId == anchor.bodyId)
        _directSideCache[anchor.bodyId] = wrapped
        return wrapped
    end

    local i = 1
    while i <= #self.losAnchors do
        local anchor = self.losAnchors[i]
        local prev   = (i == 1) and {sx, sy, sz} or self.losAnchors[i-1]
        local next_  = (i == #self.losAnchors) and {ex, ey, ez} or self.losAnchors[i+1]
        local dx, dy, dz = next_[1]-prev[1], next_[2]-prev[2], next_[3]-prev[3]
        local chordDist = math.sqrt(dx*dx+dy*dy+dz*dz)

        -- Evaluate whether this anchor is currently unnecessary
        local shouldDissolve = false
        if chordDist < LOS_MIN_DIST then
            -- Collapsed — dissolve immediately regardless of clearFrames
            shouldDissolve = true
            anchor.clearFrames = (anchor.clearFrames or 0) + 1
        else
            local ndx,ndy,ndz = dx/chordDist, dy/chordDist, dz/chordDist
            local hit, hitDist = los_raycast(prev[1],prev[2],prev[3], ndx,ndy,ndz, chordDist)
            local chordClear = (not hit) or (hitDist and hitDist >= chordDist - LOS_CLEAR_SLACK)

            if chordClear and not isStillTopologicallyWrapped(anchor) then
                anchor.clearFrames = (anchor.clearFrames or 0) + 1
                if anchor.clearFrames >= LOS_CLEAR_FRAMES then
                    shouldDissolve = true
                    dbg(string.format("[ChainController][LOS] Remove anchor %d after %d clear frames", i, anchor.clearFrames))
                end
            else
                anchor.clearFrames = 0  -- blocked this frame — reset debounce
            end
        end

        if shouldDissolve then
            table.remove(self.losAnchors, i)
            -- i stays the same; next entry slid down
        else
            -- SLIDING REFRESH: re-derive world position by shooting from predecessor
            -- toward this anchor's own stored position.  Using the anchor's position
            -- (not the chord direction) ensures we always re-hit the same surface
            -- patch even after the player has moved significantly sideways.
            --
            -- IMPORTANT: skip refresh when clearFrames > 0.  If the anchor is already
            -- accumulating toward dissolution, re-hitting the surface would re-lock it
            -- and prevent it from ever being removed (the stickiness bug).
            if (anchor.clearFrames or 0) > 0 then
                i = i + 1
                goto continue_anchor
            end
            local toAx = anchor[1] - prev[1]
            local toAy = anchor[2] - prev[2]
            local toAz = anchor[3] - prev[3]
            local toADist = math.sqrt(toAx*toAx + toAy*toAy + toAz*toAz)
            if toADist > LOS_MIN_DIST then
                local tdx, tdy, tdz = toAx/toADist, toAy/toADist, toAz/toADist
                -- Cast slightly beyond stored position to account for normal offset
                local hit2, dist2, px,py,pz, nx2,ny2,nz2, bodyId2 =
                    los_raycast(prev[1],prev[2],prev[3], tdx,tdy,tdz, toADist + 0.3)
                if hit2 then
                    local refreshed = make_anchor(
                        prev[1],prev[2],prev[3], tdx,tdy,tdz,
                        dist2, px,py,pz, nx2,ny2,nz2, bodyId2)
                    -- Validate: the segment refreshed→next_ must not be blocked by
                    -- the same body.  If it is, the refresh landed on the wrong face
                    -- (e.g. side face when the anchor should be on the front) and
                    -- accepting it would make the path cut through the obstacle.
                    local refreshOk = true
                    local rnx = next_[1] - refreshed[1]
                    local rny = next_[2] - refreshed[2]
                    local rnz = next_[3] - refreshed[3]
                    local rnDist = math.sqrt(rnx*rnx + rny*rny + rnz*rnz)
                    if rnDist >= LOS_MIN_DIST then
                        local rndx, rndy, rndz = rnx/rnDist, rny/rnDist, rnz/rnDist
                        local vhit, _, _,_,_, _,_,_, vBodyId =
                            los_raycast(refreshed[1],refreshed[2],refreshed[3], rndx,rndy,rndz, rnDist)
                        if vhit and vBodyId == bodyId2 then
                            refreshOk = false  -- wrong face: segment to next still cuts the same body
                        end
                    end
                    if refreshOk then
                        anchor[1] = refreshed[1]
                        anchor[2] = refreshed[2]
                        anchor[3] = refreshed[3]
                        anchor.nx = refreshed.nx
                        anchor.ny = refreshed.ny
                        anchor.nz = refreshed.nz
                        -- Update bodyId so wrap cache stays valid for the new body
                        anchor.bodyId = bodyId2
                    end
                end
            end
            i = i + 1
            ::continue_anchor::
        end
    end

    -- -------------------------------------------------------------------------
    -- ADD PASS
    -- Walk from the last surviving anchor toward endPos, adding new anchors
    -- wherever the path is blocked.
    -- GUARD: skip any hit body that the topology test says is NOT between the
    -- two endpoints — this prevents re-adding anchors that were just dissolved.
    -- -------------------------------------------------------------------------
    if #self.losAnchors >= LOS_MAX_ANCHORS then return end

    local fromX, fromY, fromZ
    if #self.losAnchors > 0 then
        local last = self.losAnchors[#self.losAnchors]
        fromX, fromY, fromZ = last[1], last[2], last[3]
    else
        fromX, fromY, fromZ = sx, sy, sz
    end

    for _ = 1, LOS_MAX_ANCHORS - #self.losAnchors do
        local dx, dy, dz = ex-fromX, ey-fromY, ez-fromZ
        local dist = math.sqrt(dx*dx+dy*dy+dz*dz)
        if dist < LOS_MIN_DIST then break end

        local ndx,ndy,ndz = dx/dist, dy/dist, dz/dist
        local hit, hitDist, px,py,pz, nx,ny,nz, bodyId =
            los_raycast(fromX,fromY,fromZ, ndx,ndy,ndz, dist)

        local clear = (not hit) or (hitDist and hitDist >= dist - LOS_CLEAR_SLACK)
        if clear then break end

        -- Topology guard: if the direct startPos→endPos ray doesn't hit this body,
        -- the obstacle is not topologically between the endpoints — skip it.
        -- This prevents re-adding a body that the remove pass just dissolved.
        if bodyId ~= nil and not _directSideCache[bodyId] then
            -- run topology check for this bodyId and cache it
            local tdx, tdy, tdz = ex-sx, ey-sy, ez-sz
            local tdist = math.sqrt(tdx*tdx+tdy*tdy+tdz*tdz)
            if tdist >= LOS_MIN_DIST then
                local tndx, tndy, tndz = tdx/tdist, tdy/tdist, tdz/tdist
                local thit, _, _,_,_, _,_,_, tBodyId =
                    los_raycast(sx, sy, sz, tndx, tndy, tndz, tdist)
                _directSideCache[bodyId] = thit and (tBodyId == bodyId)
            else
                _directSideCache[bodyId] = false
            end
        end
        if bodyId ~= nil and not _directSideCache[bodyId] then
            -- Body is not between endpoints — nudge past it and keep walking
            fromX = fromX + ndx * (hitDist + LOS_NUDGE * 2)
            fromY = fromY + ndy * (hitDist + LOS_NUDGE * 2)
            fromZ = fromZ + ndz * (hitDist + LOS_NUDGE * 2)
            -- don't add anchor for this body
        else
            local anchor = make_anchor(fromX,fromY,fromZ, ndx,ndy,ndz, hitDist, px,py,pz, nx,ny,nz, bodyId)
            dbg(string.format("[ChainController][LOS] Add anchor %d bodyId=%s pos=(%.3f,%.3f,%.3f)",
                #self.losAnchors+1, tostring(bodyId), anchor[1], anchor[2], anchor[3]))
            table.insert(self.losAnchors, anchor)
            fromX = anchor[1] + ndx * LOS_NUDGE
            fromY = anchor[2] + ndy * LOS_NUDGE
            fromZ = anchor[3] + ndz * LOS_NUDGE
        end

        if #self.losAnchors >= LOS_MAX_ANCHORS then break end
    end

    -- -------------------------------------------------------------------------
    -- VALIDATION PASS
    -- Walk every segment of the complete path (start→a[1]→...→a[n]→end).
    -- Guards:
    --   (a) Skip bodies not topologically between endpoints (same guard as add pass).
    --   (b) Degenerate loop detection: if a validation insert lands at the same
    --       position as the previous insert, the anchor is stuck against a concave
    --       surface and adding more copies won't help — break immediately.
    -- -------------------------------------------------------------------------
    local validationPasses = 0
    local maxValidation = LOS_MAX_ANCHORS
    local validationChanged = true
    local lastInsertX, lastInsertY, lastInsertZ = nil, nil, nil
    while validationChanged and validationPasses < maxValidation and #self.losAnchors < LOS_MAX_ANCHORS do
        validationChanged = false
        validationPasses = validationPasses + 1

        local path = {{sx, sy, sz}}
        for _, a in ipairs(self.losAnchors) do table.insert(path, a) end
        table.insert(path, {ex, ey, ez})

        for seg = 1, #path - 1 do
            local ax2, ay2, az2 = path[seg][1], path[seg][2], path[seg][3]
            local bx2, by2, bz2 = path[seg+1][1], path[seg+1][2], path[seg+1][3]
            local vdx, vdy, vdz = bx2-ax2, by2-ay2, bz2-az2
            local vdist = math.sqrt(vdx*vdx + vdy*vdy + vdz*vdz)
            if vdist >= LOS_MIN_DIST then
                local vndx, vndy, vndz = vdx/vdist, vdy/vdist, vdz/vdist
                local vhit, vhitDist, vpx,vpy,vpz, vnx,vny,vnz, vBodyId =
                    los_raycast(ax2,ay2,az2, vndx,vndy,vndz, vdist)
                local vClear = (not vhit) or (vhitDist and vhitDist >= vdist - LOS_CLEAR_SLACK)
                if not vClear then
                    -- Topology guard: skip bodies not between endpoints
                    if vBodyId ~= nil and not _directSideCache[vBodyId] then
                        local tdx2, tdy2, tdz2 = ex-sx, ey-sy, ez-sz
                        local tdist2 = math.sqrt(tdx2*tdx2+tdy2*tdy2+tdz2*tdz2)
                        if tdist2 >= LOS_MIN_DIST then
                            local tndx2, tndy2, tndz2 = tdx2/tdist2, tdy2/tdist2, tdz2/tdist2
                            local thit2, _, _,_,_, _,_,_, tBodyId2 =
                                los_raycast(sx, sy, sz, tndx2, tndy2, tndz2, tdist2)
                            _directSideCache[vBodyId] = thit2 and (tBodyId2 == vBodyId)
                        else
                            _directSideCache[vBodyId] = false
                        end
                    end
                    if vBodyId ~= nil and not _directSideCache[vBodyId] then
                        -- Not topologically between endpoints — skip this segment
                    else
                        local newAnchor = make_anchor(ax2,ay2,az2, vndx,vndy,vndz,
                            vhitDist, vpx,vpy,vpz, vnx,vny,vnz, vBodyId)

                        -- Degenerate loop guard: if new anchor is at the same position
                        -- as the last insert, we're stuck against concave geometry — stop.
                        if lastInsertX and
                           math.abs(newAnchor[1]-lastInsertX) < LOS_NUDGE and
                           math.abs(newAnchor[2]-lastInsertY) < LOS_NUDGE and
                           math.abs(newAnchor[3]-lastInsertZ) < LOS_NUDGE then
                            dbg("[ChainController][LOS] Validation degenerate loop detected, stopping")
                            validationChanged = false
                            break
                        end

                        dbg(string.format("[ChainController][LOS] Validation insert at seg=%d bodyId=%s pos=(%.3f,%.3f,%.3f)",
                            seg, tostring(vBodyId), newAnchor[1], newAnchor[2], newAnchor[3]))
                        table.insert(self.losAnchors, seg, newAnchor)
                        lastInsertX, lastInsertY, lastInsertZ = newAnchor[1], newAnchor[2], newAnchor[3]
                        validationChanged = true
                        break
                    end
                end
            end
            if #self.losAnchors >= LOS_MAX_ANCHORS then break end
        end
    end
end

-- Distribute activeN links evenly along the piecewise path:
--   startPos → losAnchors[1] → losAnchors[2] → ... → endPos
-- All positions are set kinematically (prev = pos, so no velocity).
-- Pool links beyond activeN are parked at startPos.
function M:DistributeLinksAlongPath(sx, sy, sz, ex, ey, ez, activeN)
    local path = {{sx, sy, sz}}
    for _, a in ipairs(self.losAnchors) do table.insert(path, a) end
    table.insert(path, {ex, ey, ez})

    -- Precompute per-segment lengths and total
    local segLens = {}
    local totalLen = 0
    for i = 2, #path do
        local dx = path[i][1] - path[i-1][1]
        local dy = path[i][2] - path[i-1][2]
        local dz = path[i][3] - path[i-1][3]
        local l  = math.sqrt(dx*dx + dy*dy + dz*dz)
        segLens[i - 1] = l
        totalLen = totalLen + l
    end

    -- Place each active link
    for i = 1, activeN do
        local t           = (activeN > 1) and ((i - 1) / (activeN - 1)) or 0
        local targetDist  = t * totalLen
        local cumLen      = 0
        local placed      = false

        for seg = 1, #segLens do
            local segEnd = cumLen + segLens[seg]
            if targetDist <= segEnd + 1e-9 then
                local localT = (segLens[seg] > 1e-9) and ((targetDist - cumLen) / segLens[seg]) or 0
                local from = path[seg]; local to = path[seg + 1]
                local px = from[1] + (to[1] - from[1]) * localT
                local py = from[2] + (to[2] - from[2]) * localT
                local pz = from[3] + (to[3] - from[3]) * localT
                self.positions[i][1] = px; self.positions[i][2] = py; self.positions[i][3] = pz
                self.prev[i][1]      = px; self.prev[i][2]      = py; self.prev[i][3]      = pz
                placed = true
                break
            end
            cumLen = segEnd
        end

        if not placed then
            self.positions[i][1] = ex; self.positions[i][2] = ey; self.positions[i][3] = ez
            self.prev[i][1]      = ex; self.prev[i][2]      = ey; self.prev[i][3]      = ez
        end
    end

    -- Park pool links at startPos (keeps them hidden at origin of chain)
    for i = activeN + 1, self.n do
        self.positions[i][1] = sx; self.positions[i][2] = sy; self.positions[i][3] = sz
        self.prev[i][1]      = sx; self.prev[i][2]      = sy; self.prev[i][3]      = sz
    end
end

-- ---------------------------------------------------------------------------

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
            self.losAnchors = {}
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

    -- Movement constraint
    local constraintActive = (self.endPointLocked or self._raycastSnapped) and not self.isRetracting and not self._flopping
    if constraintActive then
        local slack    = math.max(0.5, tonumber(settings.ChainSlackDistance) or 0.5)
        local dragTag  = settings.DragTag or ""
        local ex0 = self.lockedEndPoint[1]
        local ey0 = self.lockedEndPoint[2]
        local ez0 = self.lockedEndPoint[3]
        local playerDist = vec_len(sx - ex0, sy - ey0, sz - ez0)
        local chainLength = self.chainLen or 0
        local isDragType = (dragTag ~= "" and self.hookedTag == dragTag)
        local hardLimit = chainLength + slack

        dbg(string.format("[ChainController][CONSTRAINT] locked=%s snapped=%s playerDist=%.3f chainLen=%.3f slack=%.3f hardLimit=%.3f hookedTag='%s'",
            tostring(self.endPointLocked), tostring(self._raycastSnapped),
            playerDist, chainLength, slack, hardLimit, tostring(self.hookedTag)))

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
            if playerDist > chainLength then
                ratio = math.max(0, math.min(1, (playerDist - chainLength) / slack))
            end
            local shouldFlop = self._isTaut and (playerDist > hardLimit + 1e-4) and (self.hookedTag == nil or self.hookedTag == "")
            if shouldFlop then
                dbg("[ChainController][CONSTRAINT] TAUT + EXCEEDED -> flopping (untagged)")
                self.endPointLocked = false
                self._raycastSnapped = false
                self._flopping = true
                self.hookedTag = ""
                self.constraintResult = { ratio = 0, exceeded = true, drag = false }
            else
                self.constraintResult = { ratio = ratio, exceeded = false, drag = false,
                endX = ex0, endY = ey0, endZ = ez0 }
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
        ex,ey,ez = settings.endOverride[1] or 0, settings.endOverride[2] or 0, settings.endOverride[3] or 0

    elseif self.endPointLocked and not self.isRetracting then
        ex, ey, ez = self.lockedEndPoint[1], self.lockedEndPoint[2], self.lockedEndPoint[3]

    elseif self.isRetracting then
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
            ex = sx; ey = sy; ez = sz
        end

    elseif self._raycastSnapped then
        ex, ey, ez = self.lockedEndPoint[1], self.lockedEndPoint[2], self.lockedEndPoint[3]

    elseif self.isExtending then
        local theoreticalDistance = self.chainLen or 0
        local raycastResult = self:PerformRaycast(sx, sy, sz, theoreticalDistance * 1.1)

        if raycastResult and raycastResult.hit then
            self.chainLen = raycastResult.distance
            self.isExtending = false
            self._raycastSnapped = true

            local linkMaxForSnap = tonumber(settings.LinkMaxDistance) or tonumber(self.params.LinkMaxDistance) or 0
            if linkMaxForSnap > 0 then
                local needed = math.ceil(raycastResult.distance / linkMaxForSnap) + 1
                local prevActiveN = self.activeN
                self.activeN = math.min(needed, self.n)
                aN = self.activeN
                dbg(string.format("[ChainController] Raycast HIT at distance %.3f, snapped to (%.3f,%.3f,%.3f) | activeN: %d -> %d",
                    raycastResult.distance,
                    raycastResult.hitPoint[1], raycastResult.hitPoint[2], raycastResult.hitPoint[3],
                    prevActiveN, self.activeN))
            end

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
        ex = self.positions[aN][1]
        ey = self.positions[aN][2]
        ez = self.positions[aN][3]

    else
        local theoreticalDistance = self.chainLen or 0
        ex = sx + (fx * theoreticalDistance)
        ey = sy + (fy * theoreticalDistance)
        ez = sz + (fz * theoreticalDistance)
    end

    self.endPos[1], self.endPos[2], self.endPos[3] = ex, ey, ez

    -- =========================================================================
    -- LOS ANCHOR MODE
    -- When UseLOSAnchors is true and the chain has length, bypass Verlet entirely.
    -- Positions are computed geometrically: straight lines between startPos,
    -- any LOS anchors, and endPos.  Anchors are created/destroyed automatically
    -- by raycasting each segment every frame.
    -- =========================================================================
    if settings.UseLOSAnchors and (self.chainLen or 0) > 1e-4 and not self._flopping
       and (self.endPointLocked or self._raycastSnapped) then
        -- Update the anchor list (cleanup stale + add new where LOS breaks)
        self:UpdateLOSAnchors(sx, sy, sz, ex, ey, ez)

        -- Distribute all active links along the piecewise straight path
        self:DistributeLinksAlongPath(sx, sy, sz, ex, ey, ez, aN)

        -- All links kinematic — no physics for this mode
        for i = 1, self.n do self.invMass[i] = 0 end

        -- Still compute isTaut (constraint system reads it)
        do
            local arcLen = 0
            for i = 2, aN do
                local dx = self.positions[i][1] - self.positions[i-1][1]
                local dy = self.positions[i][2] - self.positions[i-1][2]
                local dz = self.positions[i][3] - self.positions[i-1][3]
                arcLen = arcLen + vec_len(dx, dy, dz)
            end
            self._isTaut = (arcLen >= (self.chainLen or 0) * 0.98)
            self._arcLen = arcLen
        end

        return self.positions, { sx, sy, sz }, { ex, ey, ez }
    end
    -- =========================================================================

    -- 4) Physical distance and segment length
    local curEndDist = vec_len(ex - sx, ey - sy, ez - sz)
    local totalLen
    if self._flopping then
        totalLen = math.max(curEndDist, 1e-6)
    else
        totalLen = (self.chainLen and self.chainLen > 1e-8) and self.chainLen or math.max(curEndDist, 1e-6)
        if maxLenSetting and maxLenSetting > 0 then totalLen = math.min(totalLen, maxLenSetting) end
    end

    local segmentLen
    if self._flopping then
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
            if not self._flopping and (self.chainLen + 1e-9) < requiredDist then
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

    self.invMass[1] = 0

    if (self.isExtending or self._raycastSnapped or self.endPointLocked) and not self._flopping then
        self.positions[aN][1], self.positions[aN][2], self.positions[aN][3] = ex, ey, ez
        self.prev[aN][1], self.prev[aN][2], self.prev[aN][3] = ex, ey, ez
        self.invMass[aN] = 0
    end

    for idx, _ in pairs(self.anchors) do
        if idx >= 1 and idx <= aN then
            self.invMass[idx] = 0
        end
    end

    -- 6) Verlet physics step
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

    -- 7) Post-physics: enforce locked/snapped endpoint and undeployed links
    if (self.endPointLocked or self._raycastSnapped) and not self._flopping then
        self.positions[aN][1], self.positions[aN][2], self.positions[aN][3] = ex, ey, ez
        self.prev[aN][1], self.prev[aN][2], self.prev[aN][3] = ex, ey, ez
        self.invMass[aN] = 0
    elseif vparams.pinnedLast and vparams.endPos then
        self.positions[aN][1], self.positions[aN][2], self.positions[aN][3] = vparams.endPos[1], vparams.endPos[2], vparams.endPos[3]
        self.prev[aN][1], self.prev[aN][2], self.prev[aN][3] = vparams.endPos[1], vparams.endPos[2], vparams.endPos[3]
        self.invMass[aN] = 0
    end

    for idx, _ in pairs(self.anchors) do
        if idx >= 1 and idx <= aN then
            self.invMass[idx] = 0
        end
    end

    -- Compute isTaut
    do
        local arcLen = 0
        for i = 2, aN do
            local dx = self.positions[i][1] - self.positions[i-1][1]
            local dy = self.positions[i][2] - self.positions[i-1][2]
            local dz = self.positions[i][3] - self.positions[i-1][3]
            arcLen = arcLen + vec_len(dx, dy, dz)
        end
        self._isTaut = (arcLen >= (self.chainLen or 0) * 0.98)
        self._arcLen = arcLen
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
        IsTaut = self._isTaut,
        LOSAnchorCount = #self.losAnchors,
        LockedEndPoint = (self.endPointLocked or self._raycastSnapped) and
                         {self.lockedEndPoint[1], self.lockedEndPoint[2], self.lockedEndPoint[3]} or nil
    }
end

return M