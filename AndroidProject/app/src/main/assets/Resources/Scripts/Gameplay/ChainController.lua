-- ChainController.lua
_G.CHAIN_DEBUG = _G.CHAIN_DEBUG ~= nil and _G.CHAIN_DEBUG or false
local function dbg(...) if _G.CHAIN_DEBUG then print(...) end end
local VerletAdapter = require("extension.verletAdapter")
local M = {}

local function vec_len(x,y,z) return math.sqrt((x or 0)*(x or 0)+(y or 0)*(y or 0)+(z or 0)*(z or 0)) end
local function normalize(x,y,z) local L=vec_len(x,y,z) if L<1e-9 then return 0,0,0 end return x/L,y/L,z/L end
local function set3(v, x, y, z)
    v[1], v[2], v[3] = x or 0, y or 0, z or 0
    return v
end

local function reset_constraint(result)
    result.ratio, result.exceeded, result.drag = 0, false, false
    result.endX, result.endY, result.endZ = nil, nil, nil
    result.targetX, result.targetY, result.targetZ = nil, nil, nil
    result.pullTargetX, result.pullTargetY, result.pullTargetZ = nil, nil, nil
    result.effectiveDist, result.chainLength = nil, nil
    return result
end

function M.New(params)
    local self = setmetatable({}, {__index=M})
    self.params   = params or {}
    self.n        = math.max(1, tonumber(self.params.NumberOfLinks) or 1)
    self.positions, self.prev, self.invMass = {}, {}, {}
    for i = 1, self.n do
        self.positions[i]={0,0,0}; self.prev[i]={0,0,0}; self.invMass[i]=1
    end
    self.activeN        = self.n
    self.anchors        = {}
    self.chainLen       = 0.0
    self.extensionTime  = 0.0
    self.isExtending    = false
    self.isRetracting   = false
    self.lastForward    = {0,0,1}
    self.startPos       = {0,0,0}
    self.endPos         = {0,0,0}
    self.endPointLocked = false
    self.lockedEndPoint = {0,0,0}
    self.hookedTag      = ""
    self._raycastSnapped         = false
    self._lockedChainLen         = 0.0
    self._flopping               = false
    self._justEnteredFlopFromExt  = false
    self._justEnteredRaycastSnap  = false
    self.losAnchors       = {}
    self.worldTarget      = nil   -- {x,y,z} aim point; when set, lastForward is recomputed each frame
    self.constraintResult = {ratio=0, exceeded=false, drag=false}
    self._throwableTension = {}
    self._verletParams = {startPos={0,0,0}, endPos={0,0,0}}
    self._returnStart = {0,0,0}
    self._returnEnd = {0,0,0}
    self._publicState = {}
    self._publicLockedEnd = {0,0,0}
    self._fullyInactive = false
    self.VerletState = VerletAdapter.Init{positions=self.positions, prev=self.prev, invMass=self.invMass}
    return self
end

function M:SetStartPos(x,y,z) set3(self.startPos, x,y,z) end
function M:SetEndPos(x,y,z)   set3(self.endPos, x,y,z) end

function M:StartExtension(forward, maxLength, linkMaxDistance, worldTarget)
    self.isExtending, self.isRetracting = true, false
    self.extensionTime, self.chainLen   = 0, 0
    self._lockedChainLen, self._raycastSnapped, self._flopping = 0, false, false
    self._justEnteredFlopFromExt = false
    self._justEnteredRaycastSnap = false
    self.endPointLocked = false
    set3(self.lockedEndPoint, 0,0,0)
    self.hookedTag      = ""
    self.losAnchors     = {}
    self.worldTarget    = worldTarget  -- {x,y,z} or nil
    local maxLen  = tonumber(maxLength)       or tonumber(self.params.MaxLength)       or 0
    local linkMax = tonumber(linkMaxDistance) or tonumber(self.params.LinkMaxDistance) or 0
    if linkMax > 0 and maxLen > 0 then
        self.activeN = math.min(math.ceil(maxLen/linkMax)+1, self.n)
    else
        self.activeN = self.n
    end
    -- Idle mode only tracks link 1. Re-seed the pool when a throw starts so
    -- every Verlet particle begins at the current hand position.
    for i = 1, self.activeN do
        set3(self.positions[i], self.startPos[1],self.startPos[2],self.startPos[3])
        set3(self.prev[i], self.startPos[1],self.startPos[2],self.startPos[3])
    end
    if forward and type(forward)=="table" and #forward>=3 then
        local nx,ny,nz = normalize(forward[1],forward[2],forward[3])
        if nx~=0 or ny~=0 or nz~=0 then set3(self.lastForward, nx,ny,nz) end
    end
end

function M:StopExtension() self.isExtending = false; self.worldTarget = nil end

function M:StartRetraction()
    if (self.chainLen or 0) <= 0 then return end
    self.isRetracting, self.isExtending = true, false
    self._raycastSnapped, self._flopping = false, false
    self._justEnteredFlopFromExt = false
    self._justEnteredRaycastSnap = false
    self.hookedTag       = ""
    self.losAnchors      = {}
    self._lockedChainLen = self.chainLen
    self.worldTarget     = nil
end

function M:ContinueExtension(forward, maxLength, linkMaxDistance)
    local maxLen  = tonumber(maxLength)       or tonumber(self.params.MaxLength)       or 0
    local linkMax = tonumber(linkMaxDistance) or tonumber(self.params.LinkMaxDistance) or 0
    local chainSpeed = tonumber(self.params.ChainSpeed) or 10
    self.extensionTime = (self.chainLen or 0) / math.max(chainSpeed, 1e-6)
    self.endPointLocked  = false
    self._raycastSnapped = false
    self._flopping       = false
    self._justEnteredFlopFromExt = false
    self._justEnteredRaycastSnap = false
    self.hookedTag       = ""
    self.losAnchors      = {}
    self._lockedChainLen = 0
    if linkMax > 0 and maxLen > 0 then
        self.activeN = math.min(math.ceil(maxLen / linkMax) + 1, self.n)
    else
        self.activeN = self.n
    end
    if forward and type(forward) == "table" and #forward >= 3 then
        local nx,ny,nz = normalize(forward[1], forward[2], forward[3])
        if nx ~= 0 or ny ~= 0 or nz ~= 0 then set3(self.lastForward, nx,ny,nz) end
    end
    self.isExtending  = true
    self.isRetracting = false
end

function M:ComputeAnchors(angleThresholdRad)
    angleThresholdRad = angleThresholdRad or math.rad(45)
    self.anchors = {}
    local aN = self.activeN
    if aN < 3 then return end
    for i = 2, aN-1 do
        local a,b,c = self.positions[i-1], self.positions[i], self.positions[i+1]
        local v1x,v1y,v1z = b[1]-a[1],b[2]-a[2],b[3]-a[3]
        local v2x,v2y,v2z = c[1]-b[1],c[2]-b[2],c[3]-b[3]
        local l1,l2 = vec_len(v1x,v1y,v1z), vec_len(v2x,v2y,v2z)
        if l1>1e-6 and l2>1e-6 then
            local dotp = math.max(-1, math.min(1, (v1x*v2x+v1y*v2y+v1z*v2z)/(l1*l2)))
            if math.acos(dotp) >= angleThresholdRad then self.anchors[i] = true end
        end
    end
end

function M:PerformRaycast(sx,sy,sz,maxDistance)
    if not Physics or not Physics.Raycast then return nil end
    local nx,ny,nz = normalize(self.lastForward[1] or 0, self.lastForward[2] or 0, self.lastForward[3] or 1)
    local hitDistance = Physics.Raycast(sx,sy,sz, nx,ny,nz, maxDistance)
    if hitDistance > 0 then
        return {hit=true, distance=hitDistance,
                hitPoint={sx+nx*hitDistance, sy+ny*hitDistance, sz+nz*hitDistance}}
    end
    return nil
end

function M:Tug(strength)
    -- Temporarily shorten the target length to make the chain look taut.
    -- The solver will naturally return it to normal next frame.
    if self.chainLen then
        self.chainLen = math.max(0.1, self.chainLen - (strength or 0.3))
    end
end
-- ---------------------------------------------------------------------------
-- LOS anchor system
-- ---------------------------------------------------------------------------
local LOS_NUDGE        = 0.06
local LOS_MIN_DIST     = 0.03
local LOS_CLEAR_SLACK  = 0.06
local LOS_MAX_ANCHORS  = 16
local LOS_CLEAR_FRAMES = 2

local function los_raycast(ox,oy,oz, dx,dy,dz, maxDist)
    if Physics.RaycastFull then
        local ok, a,b,c,d,e,f,g,h,k = pcall(function()
            return Physics.RaycastFull(ox,oy,oz, dx,dy,dz, maxDist)
        end)
        if ok then
            local hit,dist,px,py,pz,nx,ny,nz,bodyId
            if type(a)=="table" then
                hit,dist,px,py,pz,nx,ny,nz,bodyId = a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9]
            else
                hit,dist,px,py,pz,nx,ny,nz,bodyId = a,b,c,d,e,f,g,h,k
            end
            if hit and dist and dist>0 then return true,dist,px,py,pz,nx,ny,nz,bodyId end
        end
        return false
    end
    local dist = Physics.Raycast(ox,oy,oz, dx,dy,dz, maxDist)
    if dist and dist>0 then return true,dist, ox+dx*dist,oy+dy*dist,oz+dz*dist, 0,1,0,nil end
    return false
end

local function make_anchor(ox,oy,oz, dirX,dirY,dirZ, dist, px,py,pz, nx,ny,nz, bodyId)
    nx,ny,nz = nx or 0, ny or 0, nz or 0
    local nlen = math.sqrt(nx*nx+ny*ny+nz*nz)
    local ax,ay,az
    if nlen > 0.01 then
        nx,ny,nz = nx/nlen,ny/nlen,nz/nlen
        ax,ay,az = px+nx*LOS_NUDGE, py+ny*LOS_NUDGE, pz+nz*LOS_NUDGE
    else
        nx,ny,nz = -dirX,-dirY,-dirZ
        ax,ay,az = ox+dirX*(dist-LOS_NUDGE), oy+dirY*(dist-LOS_NUDGE), oz+dirZ*(dist-LOS_NUDGE)
    end
    return {ax,ay,az, bodyId=bodyId, nx=nx,ny=ny,nz=nz, clearFrames=0}
end

function M:UpdateLOSAnchors(sx,sy,sz,ex,ey,ez)
    if not Physics then self.losAnchors={} return end

    local _sideCache = {}
    local function isWrapped(anchor)
        if anchor.bodyId==nil then return false end
        if _sideCache[anchor.bodyId]~=nil then return _sideCache[anchor.bodyId] end
        local dx,dy,dz = ex-sx,ey-sy,ez-sz
        local dist = math.sqrt(dx*dx+dy*dy+dz*dz)
        if dist < LOS_MIN_DIST then _sideCache[anchor.bodyId]=false; return false end
        local hit,_,_,_,_,_,_,_,hitBodyId = los_raycast(sx,sy,sz, dx/dist,dy/dist,dz/dist, dist)
        local wrapped = hit and (hitBodyId==anchor.bodyId)
        _sideCache[anchor.bodyId] = wrapped
        return wrapped
    end

    local dissolvedThisFrame = {}

    -- REMOVE PASS
    local i = 1
    while i <= #self.losAnchors do
        local anchor = self.losAnchors[i]
        local prev  = (i==1) and {sx,sy,sz} or self.losAnchors[i-1]
        local next_ = (i==#self.losAnchors) and {ex,ey,ez} or self.losAnchors[i+1]
        local dx,dy,dz = next_[1]-prev[1], next_[2]-prev[2], next_[3]-prev[3]
        local chordDist = math.sqrt(dx*dx+dy*dy+dz*dz)
        local shouldDissolve = false

        if chordDist < LOS_MIN_DIST then
            anchor.clearFrames = (anchor.clearFrames or 0)+1
            shouldDissolve = true
        else
            local hit,hitDist = los_raycast(prev[1],prev[2],prev[3], dx/chordDist,dy/chordDist,dz/chordDist, chordDist)
            if ((not hit) or (hitDist and hitDist>=chordDist-LOS_CLEAR_SLACK)) and not isWrapped(anchor) then
                anchor.clearFrames = (anchor.clearFrames or 0)+1
                if anchor.clearFrames >= LOS_CLEAR_FRAMES then
                    shouldDissolve = true
                    dbg(string.format("[LOS] Remove anchor %d after %d clear frames",i,anchor.clearFrames))
                end
            else
                anchor.clearFrames = 0
            end
        end

        if shouldDissolve then
            if anchor.bodyId then dissolvedThisFrame[anchor.bodyId]=true end
            table.remove(self.losAnchors, i)
        else
            if (anchor.clearFrames or 0) == 0 then
                local toAx,toAy,toAz = anchor[1]-prev[1], anchor[2]-prev[2], anchor[3]-prev[3]
                local toADist = math.sqrt(toAx*toAx+toAy*toAy+toAz*toAz)
                if toADist > LOS_MIN_DIST then
                    local tdx,tdy,tdz = toAx/toADist, toAy/toADist, toAz/toADist
                    local hit2,dist2,px,py,pz,nx2,ny2,nz2,bodyId2 =
                        los_raycast(prev[1],prev[2],prev[3], tdx,tdy,tdz, toADist+0.3)
                    if hit2 then
                        local ref = make_anchor(prev[1],prev[2],prev[3], tdx,tdy,tdz,
                                                dist2,px,py,pz, nx2,ny2,nz2, bodyId2)
                        local rnx,rny,rnz = next_[1]-ref[1], next_[2]-ref[2], next_[3]-ref[3]
                        local rnDist = math.sqrt(rnx*rnx+rny*rny+rnz*rnz)
                        local refreshOk = true
                        if rnDist >= LOS_MIN_DIST then
                            local vhit,_,_,_,_,_,_,_,vBodyId =
                                los_raycast(ref[1],ref[2],ref[3], rnx/rnDist,rny/rnDist,rnz/rnDist, rnDist)
                            if vhit and vBodyId==bodyId2 then refreshOk=false end
                        end
                        if refreshOk then
                            anchor[1],anchor[2],anchor[3] = ref[1],ref[2],ref[3]
                            anchor.nx,anchor.ny,anchor.nz = ref.nx,ref.ny,ref.nz
                            anchor.bodyId = bodyId2
                        end
                    end
                end
            end
            i = i+1
        end
    end

    -- ADD PASS
    if #self.losAnchors >= LOS_MAX_ANCHORS then return end
    local fromX,fromY,fromZ
    if #self.losAnchors > 0 then
        fromX,fromY,fromZ = self.losAnchors[#self.losAnchors][1],
                            self.losAnchors[#self.losAnchors][2],
                            self.losAnchors[#self.losAnchors][3]
    else
        fromX,fromY,fromZ = sx,sy,sz
    end
    for _ = 1, LOS_MAX_ANCHORS-#self.losAnchors do
        local dx,dy,dz = ex-fromX,ey-fromY,ez-fromZ
        local dist = math.sqrt(dx*dx+dy*dy+dz*dz)
        if dist < LOS_MIN_DIST then break end
        local ndx,ndy,ndz = dx/dist,dy/dist,dz/dist
        local hit,hitDist,px,py,pz,nx,ny,nz,bodyId = los_raycast(fromX,fromY,fromZ, ndx,ndy,ndz, dist)
        if (not hit) or (hitDist and hitDist>=dist-LOS_CLEAR_SLACK) then break end
        if bodyId and dissolvedThisFrame[bodyId] then break end
        local anchor = make_anchor(fromX,fromY,fromZ, ndx,ndy,ndz, hitDist,px,py,pz,nx,ny,nz,bodyId)
        dbg(string.format("[LOS] Add anchor %d bodyId=%s pos=(%.3f,%.3f,%.3f)",
            #self.losAnchors+1, tostring(bodyId), anchor[1],anchor[2],anchor[3]))
        table.insert(self.losAnchors, anchor)
        fromX,fromY,fromZ = anchor[1]+ndx*LOS_NUDGE, anchor[2]+ndy*LOS_NUDGE, anchor[3]+ndz*LOS_NUDGE
        if #self.losAnchors >= LOS_MAX_ANCHORS then break end
    end

    -- VALIDATION PASS
    local passes, changed = 0, true
    while changed and passes<LOS_MAX_ANCHORS and #self.losAnchors<LOS_MAX_ANCHORS do
        changed=false; passes=passes+1
        local path = {{sx,sy,sz}}
        for _,a in ipairs(self.losAnchors) do table.insert(path,a) end
        table.insert(path,{ex,ey,ez})
        for seg = 1, #path-1 do
            local ax2,ay2,az2 = path[seg][1],path[seg][2],path[seg][3]
            local bx2,by2,bz2 = path[seg+1][1],path[seg+1][2],path[seg+1][3]
            local vdx,vdy,vdz = bx2-ax2,by2-ay2,bz2-az2
            local vdist = math.sqrt(vdx*vdx+vdy*vdy+vdz*vdz)
            if vdist >= LOS_MIN_DIST then
                local vndx,vndy,vndz = vdx/vdist,vdy/vdist,vdz/vdist
                local vhit,vhitDist,vpx,vpy,vpz,vnx,vny,vnz,vBodyId =
                    los_raycast(ax2,ay2,az2, vndx,vndy,vndz, vdist)
                if vhit and not (vhitDist and vhitDist>=vdist-LOS_CLEAR_SLACK) then
                    if not (vBodyId and dissolvedThisFrame[vBodyId]) then
                        local newA = make_anchor(ax2,ay2,az2, vndx,vndy,vndz,
                            vhitDist,vpx,vpy,vpz, vnx,vny,vnz, vBodyId)
                        dbg(string.format("[LOS] Validation insert seg=%d bodyId=%s pos=(%.3f,%.3f,%.3f)",
                            seg, tostring(vBodyId), newA[1],newA[2],newA[3]))
                        table.insert(self.losAnchors, seg, newA)
                        changed=true; break
                    end
                end
            end
            if #self.losAnchors >= LOS_MAX_ANCHORS then break end
        end
    end
end

function M:DistributeLinksAlongPath(sx,sy,sz,ex,ey,ez,activeN)
    local path = {{sx,sy,sz}}
    for _,a in ipairs(self.losAnchors) do table.insert(path,a) end
    table.insert(path,{ex,ey,ez})
    local segLens, totalLen = {}, 0
    for i = 2,#path do
        local dx,dy,dz = path[i][1]-path[i-1][1], path[i][2]-path[i-1][2], path[i][3]-path[i-1][3]
        local l = math.sqrt(dx*dx+dy*dy+dz*dz)
        segLens[i-1]=l; totalLen=totalLen+l
    end
    for i = 1, activeN do
        local targetDist = ((activeN>1) and ((i-1)/(activeN-1)) or 0)*totalLen
        local cumLen, placed = 0, false
        for seg = 1,#segLens do
            local segEnd = cumLen+segLens[seg]
            if targetDist <= segEnd+1e-9 then
                local localT = (segLens[seg]>1e-9) and ((targetDist-cumLen)/segLens[seg]) or 0
                local from,to = path[seg],path[seg+1]
                set3(self.positions[i],
                    from[1]+(to[1]-from[1])*localT,
                    from[2]+(to[2]-from[2])*localT,
                    from[3]+(to[3]-from[3])*localT)
                set3(self.prev[i], self.positions[i][1],self.positions[i][2],self.positions[i][3])
                placed=true; break
            end
            cumLen=segEnd
        end
        if not placed then set3(self.positions[i], ex,ey,ez); set3(self.prev[i], ex,ey,ez) end
    end
    for i = activeN+1, self.n do set3(self.positions[i], sx,sy,sz); set3(self.prev[i], sx,sy,sz) end
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
        if maxLenSetting>0 then desired=math.min(desired,maxLenSetting) end
        if not isElastic and linkMax and linkMax>0 then desired=math.min(desired,linkMax*math.max(1,aN-1)) end
        self.chainLen = desired
        if maxLenSetting>0 and desired>=maxLenSetting and not self._raycastSnapped and not self.endPointLocked then
            self.isExtending=false; self._flopping=true
            self._justEnteredFlopFromExt = true
        end
    end
    if self.isRetracting then
        self.chainLen = math.max(0,(self.chainLen or 0)-chainSpeed*dt)
        if self.chainLen <= 0 then
            self.isRetracting,self.chainLen = false,0
            self.endPointLocked,self._raycastSnapped = false,false
            self.losAnchors = {}
        end
    end

    -- 2) Resolve start position
    local sx,sy,sz = 0,0,0
    if type(settings.getStart)=="function" then
        sx,sy,sz = settings.getStart()
    elseif settings.startOverride then
        sx,sy,sz = settings.startOverride[1] or 0, settings.startOverride[2] or 0, settings.startOverride[3] or 0
    else
        sx,sy,sz = self.startPos[1] or 0, self.startPos[2] or 0, self.startPos[3] or 0
    end
    self.startPos[1],self.startPos[2],self.startPos[3] = sx,sy,sz

    local fullyInactive = not self.isExtending and not self.isRetracting
        and not self.endPointLocked and not self._raycastSnapped and not self._flopping
        and (self.chainLen or 0) <= 1e-4 and not settings.SpinActive
    if fullyInactive then
        if not self._fullyInactive then
            for i = 1, self.n do
                set3(self.positions[i], sx,sy,sz)
                set3(self.prev[i], sx,sy,sz)
                self.invMass[i] = 0
            end
            self.VerletState.accumulator = 0
        else
            set3(self.positions[1], sx,sy,sz)
            set3(self.prev[1], sx,sy,sz)
        end
        self._fullyInactive = true
        self.invMass[1] = 0
        set3(self.endPos, sx,sy,sz)
        reset_constraint(self.constraintResult)
        self.throwableTension = nil
        self._groundY, self._isTaut, self._arcLen = nil, false, 0
        return self.positions, set3(self._returnStart, sx,sy,sz), set3(self._returnEnd, sx,sy,sz)
    end

    if self._fullyInactive then
        -- Spin/other visible modes may begin without StartExtension.
        for i = 1, self.activeN do
            set3(self.positions[i], sx,sy,sz)
            set3(self.prev[i], sx,sy,sz)
        end
    end
    self._fullyInactive = false

    -- Movement constraint
    local constraintResult = reset_constraint(self.constraintResult)
    local constraintActive = (self.endPointLocked or self._raycastSnapped) and not self.isRetracting and not self._flopping
    if constraintActive then
        local slack       = math.max(0.5, tonumber(settings.ChainSlackDistance) or 0.5)
        local dragTag     = settings.DragTag or ""
        local ex0,ey0,ez0 = self.lockedEndPoint[1],self.lockedEndPoint[2],self.lockedEndPoint[3]
        local chainLength = self.chainLen or 0
        local isDragType  = (dragTag~="" and self.hookedTag==dragTag)
        local hardLimit   = chainLength + slack
        local usingArcLen   = settings.UseLOSAnchors and self._arcLen and (#self.losAnchors>0)
        local effectiveDist = usingArcLen and self._arcLen or vec_len(sx-ex0,sy-ey0,sz-ez0)
        local tensionX,tensionY,tensionZ
        if #self.losAnchors>0 then
            tensionX,tensionY,tensionZ = self.losAnchors[1][1],self.losAnchors[1][2],self.losAnchors[1][3]
        else
            tensionX,tensionY,tensionZ = ex0,ey0,ez0
        end
        if _G.CHAIN_DEBUG then
            dbg(string.format("[CONSTRAINT] effectiveDist=%.3f arcLen=%s chainLen=%.3f hardLimit=%.3f usingArcLen=%s",
                effectiveDist,tostring(self._arcLen),chainLength,hardLimit,tostring(usingArcLen)))
        end
        -- Pull target for throwable: the nearest chain point on the player side of the endpoint.
        -- = last LOS anchor (closest to the hooked obj so it follows the chain path around corners),
        -- or player/startPos if no anchors (straight line pull).
        local pullTX, pullTY, pullTZ
        if #self.losAnchors > 0 then
            local la = self.losAnchors[#self.losAnchors]
            pullTX, pullTY, pullTZ = la[1], la[2], la[3]
        else
            pullTX, pullTY, pullTZ = sx, sy, sz
        end

        if isDragType then
            if effectiveDist > chainLength+1e-4 then
                local dx,dy,dz = sx-ex0,sy-ey0,sz-ez0
                local dist = vec_len(dx,dy,dz)
                if dist>1e-6 then
                    constraintResult.drag = true
                    constraintResult.targetX = ex0+(dx/dist)*chainLength
                    constraintResult.targetY = ey0+(dy/dist)*chainLength
                    constraintResult.targetZ = ez0+(dz/dist)*chainLength
                    constraintResult.pullTargetX = pullTX
                    constraintResult.pullTargetY = pullTY
                    constraintResult.pullTargetZ = pullTZ
                end
            else
                constraintResult.effectiveDist = effectiveDist
                constraintResult.chainLength = chainLength
                constraintResult.pullTargetX = pullTX
                constraintResult.pullTargetY = pullTY
                constraintResult.pullTargetZ = pullTZ
            end
        else
            local ratio = (effectiveDist>chainLength)
                and math.max(0,math.min(1,(effectiveDist-chainLength)/slack)) or 0
            if self._isTaut and effectiveDist>hardLimit+1e-4 and (self.hookedTag==nil or self.hookedTag=="") then
                dbg("[CONSTRAINT] TAUT + EXCEEDED -> flopping")
                self.endPointLocked,self._raycastSnapped,self._flopping = false,false,true
                self.hookedTag=""
                constraintResult.exceeded = true
                constraintResult.effectiveDist = effectiveDist
                constraintResult.chainLength = chainLength
            else
                constraintResult.ratio = ratio
                constraintResult.endX = tensionX
                constraintResult.endY = tensionY
                constraintResult.endZ = tensionZ
                constraintResult.effectiveDist = effectiveDist
                constraintResult.chainLength = chainLength
                constraintResult.pullTargetX = pullTX
                constraintResult.pullTargetY = pullTY
                constraintResult.pullTargetZ = pullTZ
            end
        end
    end

    -- Throwable tension: computed separately so it fires during retraction too.
    -- constraintActive blocks on isRetracting, but we need effectiveDist/chainLength
    -- the whole time the chain is pulling the throwable in.
    local throwableTag = settings.ThrowableTag or "Throwable"
    if self.endPointLocked and self.hookedTag == throwableTag then
        local ex0,ey0,ez0 = self.lockedEndPoint[1],self.lockedEndPoint[2],self.lockedEndPoint[3]
        local tEffDist = vec_len(sx-ex0,sy-ey0,sz-ez0)
        if settings.UseLOSAnchors and self._arcLen and #self.losAnchors>0 then
            tEffDist = self._arcLen
        end
        local tChainLen = self.chainLen or 0
        local tPullTX, tPullTY, tPullTZ
        if #self.losAnchors > 0 then
            local la = self.losAnchors[#self.losAnchors]
            tPullTX, tPullTY, tPullTZ = la[1], la[2], la[3]
        else
            tPullTX, tPullTY, tPullTZ = sx, sy, sz
        end
        local tension = self._throwableTension
        tension.effectiveDist = tEffDist
        tension.chainLength = tChainLen
        tension.pullTargetX = tPullTX
        tension.pullTargetY = tPullTY
        tension.pullTargetZ = tPullTZ
        self.throwableTension = tension
    else
        self.throwableTension = nil
    end

    -- Ground clamp
    if settings.GroundClamp and Physics and Physics.Raycast then
        local hitDist = Physics.Raycast(sx,sy,sz, 0,-1,0, 20.0)
        self._groundY = (hitDist and hitDist>0) and (sy-hitDist+(settings.GroundClampOffset or 0.1)) or nil
    else
        self._groundY = nil
    end

    -- Recompute lastForward toward world target each frame so the chain
    -- tracks the crosshair hit point even as the hand bone moves (e.g. player
    -- rotation during the throw animation).
    if self.worldTarget and self.isExtending then
        local wt = self.worldTarget
        local dx = wt.x - sx
        local dy = wt.y - sy
        local dz = wt.z - sz
        local len = vec_len(dx, dy, dz)
        if len > 0.001 then
            set3(self.lastForward, dx/len, dy/len, dz/len)
        end
    end

    -- 3) Determine end position
    local ex,ey,ez
    local fx,fy,fz = self.lastForward[1] or 0, self.lastForward[2] or 0, self.lastForward[3] or 1
    if settings.endOverride then
        ex,ey,ez = settings.endOverride[1] or 0,settings.endOverride[2] or 0,settings.endOverride[3] or 0
    elseif self.endPointLocked and not self.isRetracting then
        ex,ey,ez = self.lockedEndPoint[1],self.lockedEndPoint[2],self.lockedEndPoint[3]
    elseif self.isRetracting then
        local lx,ly,lz = self.lockedEndPoint[1],self.lockedEndPoint[2],self.lockedEndPoint[3]
        local dx,dy,dz = lx-sx,ly-sy,lz-sz
        local fullDist = vec_len(dx,dy,dz)
        if fullDist>1e-6 then
            local t = math.max(0,math.min(1,self.chainLen/(self._lockedChainLen>0 and self._lockedChainLen or fullDist)))
            ex,ey,ez = sx+dx*t,sy+dy*t,sz+dz*t
        else
            ex,ey,ez = sx,sy,sz
        end
    elseif self._raycastSnapped then
        ex,ey,ez = self.lockedEndPoint[1],self.lockedEndPoint[2],self.lockedEndPoint[3]
    elseif self.isExtending then
        local theorDist = self.chainLen or 0
        local rc = self:PerformRaycast(sx,sy,sz, theorDist*1.1)
        if rc and rc.hit then
            -- FIX: trim chainLen to actual hit distance so visual length matches geometry
            self.chainLen = rc.distance
            self.isExtending = false
            self._raycastSnapped = true
            self._justEnteredRaycastSnap = true
            local lmfs = tonumber(settings.LinkMaxDistance) or tonumber(self.params.LinkMaxDistance) or 0
            local snapMult = tonumber(settings.RaycastSnapLinkMultiplier) or 0.8
            if lmfs > 0 then
                self.activeN = math.min(math.ceil(rc.distance / lmfs * snapMult), self.n)
                aN = self.activeN
            end
            self.lockedEndPoint[1],self.lockedEndPoint[2],self.lockedEndPoint[3] =
                rc.hitPoint[1],rc.hitPoint[2],rc.hitPoint[3]
            ex,ey,ez = rc.hitPoint[1],rc.hitPoint[2],rc.hitPoint[3]
        else
            ex,ey,ez = sx+fx*theorDist,sy+fy*theorDist,sz+fz*theorDist
        end
    elseif self._flopping then
        ex,ey,ez = self.positions[aN][1],self.positions[aN][2],self.positions[aN][3]
    else
        local d = self.chainLen or 0
        ex,ey,ez = sx+fx*d,sy+fy*d,sz+fz*d
    end
    self.endPos[1],self.endPos[2],self.endPos[3] = ex,ey,ez

    -- One-frame redistribution on wall snap
    if self._raycastSnapped and self._justEnteredRaycastSnap then
        self._justEnteredRaycastSnap = false
        for i = 1, aN do
            local t = (aN > 1) and ((i-1)/(aN-1)) or 0
            set3(self.positions[i], sx + (ex-sx)*t, sy + (ey-sy)*t, sz + (ez-sz)*t)
            set3(self.prev[i], self.positions[i][1], self.positions[i][2], self.positions[i][3])
        end
    end
    -- =========================================================================
    if settings.UseLOSAnchors and (self.chainLen or 0)>1e-4 and not self._flopping
       and (self.endPointLocked or self._raycastSnapped) then

        self:UpdateLOSAnchors(sx,sy,sz,ex,ey,ez)

        local path = {{sx,sy,sz}}
        for _,a in ipairs(self.losAnchors) do table.insert(path,a) end
        table.insert(path,{ex,ey,ez})
        local totalPathLen,cumLens = 0,{0}
        for i = 2,#path do
            local dx,dy,dz = path[i][1]-path[i-1][1],path[i][2]-path[i-1][2],path[i][3]-path[i-1][3]
            totalPathLen = totalPathLen+math.sqrt(dx*dx+dy*dy+dz*dz)
            cumLens[i] = totalPathLen
        end

        for i = aN+1,self.n do set3(self.positions[i], sx,sy,sz); set3(self.prev[i], sx,sy,sz); self.invMass[i]=0 end
        for i = 1,aN do self.invMass[i]=(self.anchors[i]) and 0 or 1 end
        self.invMass[1]=0; self.invMass[aN]=0

        local curEndDist = vec_len(ex-sx,ey-sy,ez-sz)
        local totalLen   = (self.chainLen and self.chainLen>1e-8) and self.chainLen or math.max(curEndDist,1e-6)
        local segmentLen = (aN>1) and (totalLen/(aN-1)) or 0
        if linkMax and linkMax>0 and segmentLen>linkMax then segmentLen=linkMax end

        local vparams = self._verletParams
        vparams.n = aN
        vparams.VerletGravity = settings.VerletGravity or self.params.VerletGravity
        vparams.VerletDamping = settings.VerletDamping or self.params.VerletDamping
        vparams.ConstraintIterations = settings.ConstraintIterations or self.params.ConstraintIterations
        vparams.IsElastic = isElastic
        vparams.LinkMaxDistance = linkMax
        vparams.totalLen = totalLen
        vparams.segmentLen = segmentLen
        vparams.ClampSegment = linkMax
        vparams.endPointLocked = nil
        vparams.GroundClamp = settings.GroundClamp
        vparams.GroundClampOffset = nil
        vparams.groundY = self._groundY
        vparams.pinnedLast = true
        set3(vparams.endPos, ex,ey,ez)
        set3(vparams.startPos, sx,sy,sz)
        vparams.FixedDt = settings.FixedDt or self.params.FixedDt
        vparams.SubSteps = settings.SubSteps or self.params.SubSteps
        vparams.MaxSubSteps = settings.MaxSubSteps or self.params.MaxSubSteps
        vparams.WallClamp = false
        vparams.WallClampInterval = nil
        vparams.WallClampRadius = nil
        VerletAdapter.Step(self.VerletState, dt, vparams)

        local isTautForProjection = (totalPathLen >= (self.chainLen or 0) * 0.97)

        if totalPathLen > 1e-9 and isTautForProjection then
            for i = 1,aN do
                local targetDist = ((aN>1) and ((i-1)/(aN-1)) or 0)*totalPathLen
                local tx,tz = sx,sz
                for seg = 1,#cumLens-1 do
                    if targetDist <= cumLens[seg+1]+1e-9 then
                        local segLen = cumLens[seg+1]-cumLens[seg]
                        local localT = (segLen>1e-9) and ((targetDist-cumLens[seg])/segLen) or 0
                        tx = path[seg][1]+(path[seg+1][1]-path[seg][1])*localT
                        tz = path[seg][3]+(path[seg+1][3]-path[seg][3])*localT
                        break
                    end
                end
                self.positions[i][1]=tx; self.positions[i][3]=tz
                self.prev[i][1]=tx;      self.prev[i][3]=tz
            end
        end

        set3(self.positions[1], sx,sy,sz); set3(self.prev[1], sx,sy,sz)
        set3(self.positions[aN], ex,ey,ez); set3(self.prev[aN], ex,ey,ez)

        local arcLen = 0
        for i = 2,aN do
            arcLen=arcLen+vec_len(self.positions[i][1]-self.positions[i-1][1],
                                  self.positions[i][2]-self.positions[i-1][2],
                                  self.positions[i][3]-self.positions[i-1][3])
        end
        self._isTaut=(arcLen>=(self.chainLen or 0)*0.98); self._arcLen=arcLen
        return self.positions, set3(self._returnStart, sx,sy,sz), set3(self._returnEnd, ex,ey,ez)
    end
    -- =========================================================================

    -- 4) Physical distance and segment length
    local curEndDist = vec_len(ex-sx,ey-sy,ez-sz)
    local totalLen
    if self._flopping then
        totalLen = math.max(curEndDist,1e-6)
    else
        totalLen = (self.chainLen and self.chainLen>1e-8) and self.chainLen or math.max(curEndDist,1e-6)
        if maxLenSetting>0 then totalLen=math.min(totalLen,maxLenSetting) end
    end
    local restLen = self._flopping
        and math.max((self.chainLen or 0), 1e-6)
        or ((maxLenSetting>0) and maxLenSetting or math.max(curEndDist,1e-6))
    local segmentLen = (aN>1) and ((self._flopping and restLen or totalLen)/(aN-1)) or 0
    if (not isElastic) and linkMax and linkMax>0 and segmentLen>linkMax then segmentLen=linkMax end

    if self._flopping and self._justEnteredFlopFromExt then
        self._justEnteredFlopFromExt = false
        for i = 1, aN do
            local t = (aN > 1) and ((i-1)/(aN-1)) or 0
            set3(self.positions[i], sx + (ex-sx)*t, sy + (ey-sy)*t, sz + (ez-sz)*t)
            set3(self.prev[i], self.positions[i][1], self.positions[i][2], self.positions[i][3])
        end
    end
    for i = 1,self.n do
        if i > aN then
            set3(self.positions[i], sx,sy,sz); set3(self.prev[i], sx,sy,sz); self.invMass[i]=0
        else
            local reqDist = (i-1)*segmentLen
            if not self._flopping and (self.chainLen+1e-9)<reqDist then
                set3(self.positions[i], sx,sy,sz); set3(self.prev[i], sx,sy,sz); self.invMass[i]=0
            else
                self.invMass[i] = self.anchors[i] and 0 or 1
            end
        end
    end
    self.invMass[1]=0
    if (self.isExtending or self._raycastSnapped or self.endPointLocked) and not self._flopping then
        set3(self.positions[aN], ex,ey,ez); set3(self.prev[aN], ex,ey,ez); self.invMass[aN]=0
    end
    for idx,_ in pairs(self.anchors) do if idx>=1 and idx<=aN then self.invMass[idx]=0 end end

    -- 6) Verlet physics step
    local vparams = self._verletParams
    vparams.n = aN
    vparams.VerletGravity = settings.VerletGravity or self.params.VerletGravity
    vparams.VerletDamping = settings.VerletDamping or self.params.VerletDamping
    vparams.ConstraintIterations = settings.ConstraintIterations or self.params.ConstraintIterations
    vparams.IsElastic = isElastic
    vparams.LinkMaxDistance = linkMax
    vparams.totalLen = totalLen
    vparams.segmentLen = segmentLen
    vparams.ClampSegment = (not self._flopping) and linkMax or nil
    vparams.endPointLocked = self.endPointLocked or self._raycastSnapped
    vparams.GroundClamp = settings.GroundClamp
    vparams.GroundClampOffset = settings.GroundClampOffset
    vparams.groundY = self._groundY
    vparams.pinnedLast = (not self._flopping) and (self.endPointLocked or self._raycastSnapped or
        ((not self.isExtending) and ((self.chainLen or 0)+1e-9>=(aN-1)*segmentLen) and
         (settings.PinEndWhenExtended or self.params.PinEndWhenExtended)))
    set3(vparams.endPos, ex,ey,ez)
    set3(vparams.startPos, sx,sy,sz)
    vparams.FixedDt = settings.FixedDt or self.params.FixedDt
    vparams.SubSteps = settings.SubSteps or self.params.SubSteps
    vparams.MaxSubSteps = settings.MaxSubSteps or self.params.MaxSubSteps
    vparams.WallClamp = false
    vparams.WallClampInterval = nil
    vparams.WallClampRadius = nil
    VerletAdapter.Step(self.VerletState, dt, vparams)
    if settings.WallClamp then
        vparams.WallClamp=true
        vparams.WallClampInterval=settings.WallClampInterval or 10
        vparams.WallClampRadius=settings.WallClampRadius or 0
        VerletAdapter.ApplyWallClamp(self.VerletState, vparams)
    end

    -- 7) Post-physics: enforce endpoints
    if (self.endPointLocked or self._raycastSnapped) and not self._flopping then
        set3(self.positions[aN], ex,ey,ez); set3(self.prev[aN], ex,ey,ez); self.invMass[aN]=0
    elseif vparams.pinnedLast and vparams.endPos then
        set3(self.positions[aN], vparams.endPos[1],vparams.endPos[2],vparams.endPos[3])
        set3(self.prev[aN], vparams.endPos[1],vparams.endPos[2],vparams.endPos[3])
        self.invMass[aN]=0
    end
    for idx,_ in pairs(self.anchors) do if idx>=1 and idx<=aN then self.invMass[idx]=0 end end

    local arcLen = 0
    for i = 2,aN do
        arcLen=arcLen+vec_len(self.positions[i][1]-self.positions[i-1][1],
                              self.positions[i][2]-self.positions[i-1][2],
                              self.positions[i][3]-self.positions[i-1][3])
    end
    self._isTaut=(arcLen>=(self.chainLen or 0)*0.98); self._arcLen=arcLen

    return self.positions,
        set3(self._returnStart, self.startPos[1],self.startPos[2],self.startPos[3]),
        set3(self._returnEnd, self.endPos[1],self.endPos[2],self.endPos[3])
end

function M:GetPublicState()
    local state = self._publicState
    state.ChainLength = self.chainLen
    state.IsExtending = self.isExtending
    state.IsRetracting = self.isRetracting
    state.LinkCount = self.n
    state.ActiveLinkCount = self.activeN
    state.Anchors = self.anchors
    state.EndPointLocked = self.endPointLocked
    state.RaycastSnapped = self._raycastSnapped
    state.Flopping = self._flopping
    state.IsTaut = self._isTaut
    state.LOSAnchorCount = #self.losAnchors
    if self.endPointLocked or self._raycastSnapped then
        state.LockedEndPoint = set3(self._publicLockedEnd,
            self.lockedEndPoint[1],self.lockedEndPoint[2],self.lockedEndPoint[3])
    else
        state.LockedEndPoint = nil
    end
    return state
end

return M
