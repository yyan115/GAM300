-- ChainController.lua
_G.CHAIN_DEBUG = _G.CHAIN_DEBUG ~= nil and _G.CHAIN_DEBUG or false
local function dbg(...) if _G.CHAIN_DEBUG then print(...) end end
local VerletAdapter = require("extension.verletAdapter")
local M = {}

local function vec_len(x,y,z) return math.sqrt((x or 0)*(x or 0)+(y or 0)*(y or 0)+(z or 0)*(z or 0)) end
local function normalize(x,y,z) local L=vec_len(x,y,z) if L<1e-9 then return 0,0,0 end return x/L,y/L,z/L end

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
    self._raycastSnapped  = false
    self._lockedChainLen  = 0.0
    self._flopping        = false
    self.losAnchors       = {}
    self.VerletState = VerletAdapter.Init{positions=self.positions, prev=self.prev, invMass=self.invMass}
    return self
end

function M:SetStartPos(x,y,z) self.startPos = {x or 0,y or 0,z or 0} end
function M:SetEndPos(x,y,z)   self.endPos   = {x or 0,y or 0,z or 0} end

function M:StartExtension(forward, maxLength, linkMaxDistance)
    self.isExtending, self.isRetracting = true, false
    self.extensionTime, self.chainLen   = 0, 0
    self._lockedChainLen, self._raycastSnapped, self._flopping = 0, false, false
    self.endPointLocked = false
    self.lockedEndPoint = {0,0,0}
    self.hookedTag      = ""
    self.losAnchors     = {}
    local maxLen  = tonumber(maxLength)       or tonumber(self.params.MaxLength)       or 0
    local linkMax = tonumber(linkMaxDistance) or tonumber(self.params.LinkMaxDistance) or 0
    if linkMax > 0 and maxLen > 0 then
        self.activeN = math.min(math.ceil(maxLen/linkMax)+1, self.n)
    else
        self.activeN = self.n
    end
    if forward and type(forward)=="table" and #forward>=3 then
        local nx,ny,nz = normalize(forward[1],forward[2],forward[3])
        if nx~=0 or ny~=0 or nz~=0 then self.lastForward={nx,ny,nz} end
    end
end

function M:StopExtension() self.isExtending = false end

function M:StartRetraction()
    if (self.chainLen or 0) <= 0 then return end
    self.isRetracting, self.isExtending = true, false
    self._raycastSnapped, self._flopping = false, false
    self.hookedTag       = ""
    self.losAnchors      = {}
    self._lockedChainLen = self.chainLen
end

-- ContinueExtension: resume extending from the current chain length and active link count.
-- Call this when the chain is already out (locked/snapped) and the player holds the button
-- again to extend further. Links are added proportionally to chainLen via LinkMaxDistance,
-- exactly mirroring how normal StartExtension + Update activates them one by one.
function M:ContinueExtension(forward, maxLength, linkMaxDistance)
    local maxLen  = tonumber(maxLength)       or tonumber(self.params.MaxLength)       or 0
    local linkMax = tonumber(linkMaxDistance) or tonumber(self.params.LinkMaxDistance) or 0

    -- Back-solve extensionTime so chainLen is continuous from its current value.
    -- Update() does: chainLen = chainSpeed * extensionTime, accumulating dt each frame.
    -- Setting extensionTime here means the very next Update tick adds only dt worth of length.
    local chainSpeed = tonumber(self.params.ChainSpeed) or 10
    self.extensionTime = (self.chainLen or 0) / math.max(chainSpeed, 1e-6)

    -- Unlock endpoint so Verlet is free to push the tip outward again.
    self.endPointLocked  = false
    self._raycastSnapped = false
    self._flopping       = false
    self.hookedTag       = ""
    self.losAnchors      = {}
    self._lockedChainLen = 0

    -- Set activeN to the FULL ceiling of links that maxLength could ever need.
    -- This is identical to what StartExtension does. Update's per-link kinematic
    -- gate (reqDist = (i-1)*segmentLen compared against chainLen) then naturally
    -- activates new links one by one as chainLen grows, proportional to distance.
    -- Links already placed keep their positions because chainLen doesn't reset to 0.
    if linkMax > 0 and maxLen > 0 then
        self.activeN = math.min(math.ceil(maxLen / linkMax) + 1, self.n)
    else
        self.activeN = self.n
    end

    -- Update aim direction if provided.
    if forward and type(forward) == "table" and #forward >= 3 then
        local nx,ny,nz = normalize(forward[1], forward[2], forward[3])
        if nx ~= 0 or ny ~= 0 or nz ~= 0 then self.lastForward = {nx,ny,nz} end
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

    -- Per-frame topology test cache: is player still on opposite side of anchor's body?
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
            -- Sliding refresh: re-derive position by shooting toward anchor's stored pos
            -- (skip if clearFrames>0 — re-locking a dissolving anchor causes stickiness)
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
                        -- Validate refreshed→next_ doesn't re-cut same body (wrong face guard)
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

    -- VALIDATION PASS: catch any newly-blocked segments across the full path
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
                self.positions[i] = {from[1]+(to[1]-from[1])*localT, from[2]+(to[2]-from[2])*localT, from[3]+(to[3]-from[3])*localT}
                self.prev[i] = {self.positions[i][1],self.positions[i][2],self.positions[i][3]}
                placed=true; break
            end
            cumLen=segEnd
        end
        if not placed then self.positions[i]={ex,ey,ez}; self.prev[i]={ex,ey,ez} end
    end
    for i = activeN+1, self.n do self.positions[i]={sx,sy,sz}; self.prev[i]={sx,sy,sz} end
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

    -- Movement constraint
    local constraintActive = (self.endPointLocked or self._raycastSnapped) and not self.isRetracting and not self._flopping
    if constraintActive then
        local slack       = math.max(0.5, tonumber(settings.ChainSlackDistance) or 0.5)
        local dragTag     = settings.DragTag or ""
        local ex0,ey0,ez0 = self.lockedEndPoint[1],self.lockedEndPoint[2],self.lockedEndPoint[3]
        local chainLength = self.chainLen or 0
        local isDragType  = (dragTag~="" and self.hookedTag==dragTag)
        local hardLimit   = chainLength + slack
        -- Use arc length when LOS anchors active; straight-line player→endpoint otherwise
        local usingArcLen   = settings.UseLOSAnchors and self._arcLen and (#self.losAnchors>0)
        local effectiveDist = usingArcLen and self._arcLen or vec_len(sx-ex0,sy-ey0,sz-ez0)
        -- Tension direction: toward first anchor, or locked endpoint if no anchors
        local tensionX,tensionY,tensionZ
        if #self.losAnchors>0 then
            tensionX,tensionY,tensionZ = self.losAnchors[1][1],self.losAnchors[1][2],self.losAnchors[1][3]
        else
            tensionX,tensionY,tensionZ = ex0,ey0,ez0
        end
        dbg(string.format("[CONSTRAINT] effectiveDist=%.3f arcLen=%s chainLen=%.3f hardLimit=%.3f usingArcLen=%s",
            effectiveDist,tostring(self._arcLen),chainLength,hardLimit,tostring(usingArcLen)))
        if isDragType then
            if effectiveDist > chainLength+1e-4 then
                local dx,dy,dz = sx-ex0,sy-ey0,sz-ez0
                local dist = vec_len(dx,dy,dz)
                if dist>1e-6 then
                    self.constraintResult = {ratio=0,exceeded=false,drag=true,
                        targetX=ex0+(dx/dist)*chainLength,
                        targetY=ey0+(dy/dist)*chainLength,
                        targetZ=ez0+(dz/dist)*chainLength}
                end
            else
                self.constraintResult = {ratio=0,exceeded=false,drag=false}
            end
        else
            local ratio = (effectiveDist>chainLength)
                and math.max(0,math.min(1,(effectiveDist-chainLength)/slack)) or 0
            if self._isTaut and effectiveDist>hardLimit+1e-4 and (self.hookedTag==nil or self.hookedTag=="") then
                dbg("[CONSTRAINT] TAUT + EXCEEDED -> flopping")
                self.endPointLocked,self._raycastSnapped,self._flopping = false,false,true
                self.hookedTag=""
                self.constraintResult = {ratio=0,exceeded=true,drag=false}
            else
                self.constraintResult = {ratio=ratio,exceeded=false,drag=false,
                    endX=tensionX,endY=tensionY,endZ=tensionZ}
            end
        end
    else
        self.constraintResult = {ratio=0,exceeded=false,drag=false}
    end

    -- Ground clamp
    if settings.GroundClamp and Physics and Physics.Raycast then
        local hitDist = Physics.Raycast(sx,sy,sz, 0,-1,0, 20.0)
        self._groundY = (hitDist and hitDist>0) and (sy-hitDist+(settings.GroundClampOffset or 0.1)) or nil
    else
        self._groundY = nil
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
            self.chainLen=rc.distance; self.isExtending=false; self._raycastSnapped=true
            local lmfs = tonumber(settings.LinkMaxDistance) or tonumber(self.params.LinkMaxDistance) or 0
            if lmfs>0 then self.activeN=math.min(math.ceil(rc.distance/lmfs)+1,self.n); aN=self.activeN end
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

    -- =========================================================================
    -- LOS ANCHOR MODE
    -- =========================================================================
    if settings.UseLOSAnchors and (self.chainLen or 0)>1e-4 and not self._flopping
       and (self.endPointLocked or self._raycastSnapped) then

        self:UpdateLOSAnchors(sx,sy,sz,ex,ey,ez)

        -- Build XZ path for post-physics correction
        local path = {{sx,sy,sz}}
        for _,a in ipairs(self.losAnchors) do table.insert(path,a) end
        table.insert(path,{ex,ey,ez})
        local totalPathLen,cumLens = 0,{0}
        for i = 2,#path do
            local dx,dy,dz = path[i][1]-path[i-1][1],path[i][2]-path[i-1][2],path[i][3]-path[i-1][3]
            totalPathLen = totalPathLen+math.sqrt(dx*dx+dy*dy+dz*dz)
            cumLens[i] = totalPathLen
        end

        -- Park pool links; make active links dynamic (start+end hard-pinned)
        for i = aN+1,self.n do self.positions[i]={sx,sy,sz}; self.prev[i]={sx,sy,sz}; self.invMass[i]=0 end
        for i = 1,aN do self.invMass[i]=(self.anchors[i]) and 0 or 1 end
        self.invMass[1]=0; self.invMass[aN]=0

        local curEndDist = vec_len(ex-sx,ey-sy,ez-sz)
        local totalLen   = (self.chainLen and self.chainLen>1e-8) and self.chainLen or math.max(curEndDist,1e-6)
        local segmentLen = (aN>1) and (totalLen/(aN-1)) or 0
        if linkMax and linkMax>0 and segmentLen>linkMax then segmentLen=linkMax end

        VerletAdapter.Step(self.VerletState, dt, {
            n=aN, VerletGravity=settings.VerletGravity or self.params.VerletGravity,
            VerletDamping=settings.VerletDamping or self.params.VerletDamping,
            ConstraintIterations=settings.ConstraintIterations or self.params.ConstraintIterations,
            IsElastic=isElastic, LinkMaxDistance=linkMax,
            totalLen=totalLen, segmentLen=segmentLen, ClampSegment=linkMax,
            GroundClamp=settings.GroundClamp, groundY=self._groundY,
            pinnedLast=true, endPos={ex,ey,ez}, startPos={sx,sy,sz},
        })

        -- FIX: Post-physics XZ path projection only when chain is taut.
        -- When the chain is lax (slack), the path is shorter than the chain length,
        -- so projecting all links onto it collapses them into a V shape.
        -- Instead, trust Verlet's positions when there is meaningful slack.
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

        -- Re-pin start and end (including Y)
        self.positions[1]={sx,sy,sz}; self.prev[1]={sx,sy,sz}
        self.positions[aN]={ex,ey,ez}; self.prev[aN]={ex,ey,ez}

        local arcLen = 0
        for i = 2,aN do
            arcLen=arcLen+vec_len(self.positions[i][1]-self.positions[i-1][1],
                                  self.positions[i][2]-self.positions[i-1][2],
                                  self.positions[i][3]-self.positions[i-1][3])
        end
        self._isTaut=(arcLen>=(self.chainLen or 0)*0.98); self._arcLen=arcLen
        return self.positions, {sx,sy,sz}, {ex,ey,ez}
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
    local restLen = (maxLenSetting>0) and maxLenSetting or math.max(curEndDist,1e-6)
    local segmentLen = (aN>1) and ((self._flopping and restLen or totalLen)/(aN-1)) or 0
    if (not isElastic) and linkMax and linkMax>0 and segmentLen>linkMax then segmentLen=linkMax end

    -- 5) Per-link kinematic state
    for i = 1,self.n do
        if i > aN then
            self.positions[i]={sx,sy,sz}; self.prev[i]={sx,sy,sz}; self.invMass[i]=0
        else
            local reqDist = (i-1)*segmentLen
            if not self._flopping and (self.chainLen+1e-9)<reqDist then
                self.positions[i]={sx,sy,sz}; self.prev[i]={sx,sy,sz}; self.invMass[i]=0
            else
                self.invMass[i] = self.anchors[i] and 0 or 1
            end
        end
    end
    self.invMass[1]=0
    if (self.isExtending or self._raycastSnapped or self.endPointLocked) and not self._flopping then
        self.positions[aN]={ex,ey,ez}; self.prev[aN]={ex,ey,ez}; self.invMass[aN]=0
    end
    for idx,_ in pairs(self.anchors) do if idx>=1 and idx<=aN then self.invMass[idx]=0 end end

    -- 6) Verlet physics step
    local vparams = {
        n=aN, VerletGravity=settings.VerletGravity or self.params.VerletGravity,
        VerletDamping=settings.VerletDamping or self.params.VerletDamping,
        ConstraintIterations=settings.ConstraintIterations or self.params.ConstraintIterations,
        IsElastic=isElastic, LinkMaxDistance=linkMax,
        totalLen=totalLen, segmentLen=segmentLen, ClampSegment=linkMax,
        endPointLocked=self.endPointLocked or self._raycastSnapped,
        GroundClamp=settings.GroundClamp, GroundClampOffset=settings.GroundClampOffset,
        groundY=self._groundY,
        pinnedLast=(not self._flopping) and (self.endPointLocked or self._raycastSnapped or
            ((not self.isExtending) and ((self.chainLen or 0)+1e-9>=(aN-1)*segmentLen) and
             (settings.PinEndWhenExtended or self.params.PinEndWhenExtended))),
        endPos={ex,ey,ez}, startPos={sx,sy,sz}
    }
    VerletAdapter.Step(self.VerletState, dt, vparams)
    if settings.WallClamp then
        vparams.WallClamp=true
        vparams.WallClampInterval=settings.WallClampInterval or 10
        vparams.WallClampRadius=settings.WallClampRadius or 0
        VerletAdapter.ApplyWallClamp(self.VerletState, vparams)
    end

    -- 7) Post-physics: enforce endpoints
    if (self.endPointLocked or self._raycastSnapped) and not self._flopping then
        self.positions[aN]={ex,ey,ez}; self.prev[aN]={ex,ey,ez}; self.invMass[aN]=0
    elseif vparams.pinnedLast and vparams.endPos then
        self.positions[aN]={vparams.endPos[1],vparams.endPos[2],vparams.endPos[3]}
        self.prev[aN]={vparams.endPos[1],vparams.endPos[2],vparams.endPos[3]}
        self.invMass[aN]=0
    end
    for idx,_ in pairs(self.anchors) do if idx>=1 and idx<=aN then self.invMass[idx]=0 end end

    -- Compute isTaut
    local arcLen = 0
    for i = 2,aN do
        arcLen=arcLen+vec_len(self.positions[i][1]-self.positions[i-1][1],
                              self.positions[i][2]-self.positions[i-1][2],
                              self.positions[i][3]-self.positions[i-1][3])
    end
    self._isTaut=(arcLen>=(self.chainLen or 0)*0.98); self._arcLen=arcLen

    return self.positions, {self.startPos[1],self.startPos[2],self.startPos[3]}, {self.endPos[1],self.endPos[2],self.endPos[3]}
end

function M:GetPublicState()
    return {
        ChainLength     = self.chainLen,
        IsExtending     = self.isExtending,
        IsRetracting    = self.isRetracting,
        LinkCount       = self.n,
        ActiveLinkCount = self.activeN,
        Anchors         = self.anchors,
        EndPointLocked  = self.endPointLocked,
        RaycastSnapped  = self._raycastSnapped,
        Flopping        = self._flopping,
        IsTaut          = self._isTaut,
        LOSAnchorCount  = #self.losAnchors,
        LockedEndPoint  = (self.endPointLocked or self._raycastSnapped) and
                          {self.lockedEndPoint[1],self.lockedEndPoint[2],self.lockedEndPoint[3]} or nil
    }
end

return M