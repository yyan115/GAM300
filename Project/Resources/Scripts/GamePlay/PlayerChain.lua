-- PlayerChain_rewrite.lua
-- Clean, runnable PlayerChain component using runtime-only storage and TransformMixin proxies.

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

-- Chain States
local COMPLETELY_LAX = 0
local LAX = 1
local TAUT = 2
local EXTENDING = 3
local RETRACTING = 4

return Component {
    mixins = { TransformMixin },

    fields = {
        NumberOfLinks = 10,
        ChainSpeed = 10.0,
        MaxLength = 0.0,
        TriggerKey = "E",
        PlayerName = "Player",
        SimulatedHitDistance = 0.0,
        ForwardOverride = nil,
        EnableLogs = true,
        AutoStart = false,
        DumpEveryFrame = false -- enable to print positions every Update (can be noisy)
    },

    _unpack_pos = function(self, a, b, c)
        if type(a) == "table" then
            local x = a[1] or a.x or 0.0
            local y = a[2] or a.y or 0.0
            local z = a[3] or a.z or 0.0
            return x, y, z
        end
        local x = (type(a) == "number") and a or 0.0
        local y = (type(b) == "number") and b or 0.0
        local z = (type(c) == "number") and c or 0.0
        return x, y, z
    end,

    Log = function(self, ...)
        if not self.EnableLogs then return end
        local parts = {}
        for i, v in ipairs({...}) do parts[i] = tostring(v) end
        print("[PlayerChain] " .. table.concat(parts, " "))
    end,

    -- robust read of transform position
    _read_transform_position = function(self, tr)
        if not tr then return 0,0,0 end
        local ok, a, b, c = pcall(function() return tr:GetPosition() end)
        if ok then
            if type(a) == "table" then return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0 end
            if type(a) == "number" and type(b) == "number" and type(c) == "number" then return a,b,c end
        end
        local ok2, pos = pcall(function() return Engine.GetTransformPosition(tr) end)
        if ok2 and type(pos) == "table" then return pos[1] or pos.x or 0, pos[2] or pos.y or 0, pos[3] or pos.z or 0 end
        if type(tr) == "table" and tr.localPosition then
            local cur = tr.localPosition
            if type(cur) == "userdata" then
                local ok3, x,y,z = pcall(function() return cur.x, cur.y, cur.z end)
                if ok3 then return x or 0, y or 0, z or 0 end
            elseif type(cur) == "table" then
                return cur[1] or cur.x or 0, cur[2] or cur.y or 0, cur[3] or cur.z or 0
            end
        end
        return 0,0,0
    end,

    -- best-effort read of transform name
    _read_transform_name = function(self, tr)
        if not tr then return "<nil>" end
        local ok, name = pcall(function() return tr.name end)
        if ok and name then return tostring(name) end
        local ok2, n2 = pcall(function() return Engine.GetTransformName(tr) end)
        if ok2 and n2 then return tostring(n2) end
        return tostring(tr)
    end,

    _read_transform_parent_name = function(self, tr)
        if not tr then return "<nil>" end
        local ok, parent = pcall(function() return tr.parent end)
        if ok and parent then
            local ok2, pname = pcall(function() return parent.name end)
            if ok2 and pname then return tostring(pname) end
        end
        local ok3, p = pcall(function() return Engine.GetTransformParent(tr) end)
        if ok3 and p then
            local ok4, pname2 = pcall(function() return Engine.GetTransformName(p) end)
            if ok4 and pname2 then return tostring(pname2) end
            return tostring(p)
        end
        return "<no-parent-detected>"
    end,

    DumpLinkPositions = function(self)
        local rt = self._runtime or {}
        rt.childTransforms = rt.childTransforms or {}
        for i, tr in ipairs(rt.childTransforms) do
            local name = self:_read_transform_name(tr)
            local px, py, pz = self:_read_transform_position(tr)
            local parentName = self:_read_transform_parent_name(tr)
            self:Log(string.format("Link %d name=%s pos=(%.3f, %.3f, %.3f) parent=%s", i, name, px, py, pz, parentName))
        end
    end,

    Start = function(self)
        if type(Engine) ~= "table" then
            print("[PlayerChain] ERROR: Engine global missing. Aborting PlayerChain Start.")
            self._disabled_due_to_missing_engine = true
            return
        end

        -- runtime-only container to avoid serializer issues
        self._runtime = self._runtime or {}
        local rt = self._runtime
        rt.childTransforms = {}
        rt.childProxies = {}

        if type(Input) ~= "table" then
            self._input_missing = true
            self:Log("WARNING: Input missing at Start; input disabled until available")
        end

        -- runtime state
        self.currentState = COMPLETELY_LAX
        self.chainLength = 0.0
        self.endPosition = {0.0, 0.0, 0.0}
        self.isExtending = false
        self.isRetracting = false
        self.extensionTime = 0.0
        self.lastForward = {0.0, 0.0, 1.0}
        self.playerTransform = nil
        self.lastState = nil
        self._positionedOnce = false

        -- find link transforms Link1..LinkN
        for i = 1, math.max(1, self.NumberOfLinks) do
            local name = "Link" .. tostring(i)
            local tr = Engine.FindTransformByName(name)
            if tr then
                table.insert(rt.childTransforms, tr)
            else
                self:Log("warning - transform '" .. name .. "' not found")
            end
        end

        -- create proxies for each transform and apply TransformMixin (if available)
        for i, tr in ipairs(rt.childTransforms) do
            local proxy = {}
            function proxy:GetComponent(compName)
                if compName == "Transform" then return tr end
                return nil
            end

            if TransformMixin and type(TransformMixin.apply) == "function" then
                TransformMixin.apply(proxy)
            else
                function proxy:GetPosition()
                    return self._component_owner and self._component_owner:_read_transform_position(tr) or 0,0,0
                end
                function proxy:Move(dx, dy, dz)
                    pcall(function()
                        if type(tr) == "table" and tr.localPosition then
                            local cur = tr.localPosition
                            if type(cur) == "userdata" then
                                cur.x = (cur.x or 0) + (dx or 0)
                                cur.y = (cur.y or 0) + (dy or 0)
                                cur.z = (cur.z or 0) + (dz or 0)
                                tr.isDirty = true
                            elseif type(cur) == "table" then
                                if cur.x ~= nil then
                                    cur.x = (cur.x or 0) + (dx or 0)
                                    cur.y = (cur.y or 0) + (dy or 0)
                                    cur.z = (cur.z or 0) + (dz or 0)
                                else
                                    cur[1] = (cur[1] or 0) + (dx or 0)
                                    cur[2] = (cur[2] or 0) + (dy or 0)
                                    cur[3] = (cur[3] or 0) + (dz or 0)
                                end
                            else
                                local ok2, pos = pcall(function() return Engine.GetTransformPosition(tr) end)
                                if ok2 and type(pos) == "table" then
                                    local cx = pos[1] or pos.x or 0
                                    local cy = pos[2] or pos.y or 0
                                    local cz = pos[3] or pos.z or 0
                                    if type(Engine.SetTransformLocalPosition) == "function" then
                                        pcall(function() Engine.SetTransformLocalPosition(tr, cx + (dx or 0), cy + (dy or 0), cz + (dz or 0)) end)
                                    elseif type(Engine.SetTransformPosition) == "function" then
                                        pcall(function() Engine.SetTransformPosition(tr, cx + (dx or 0), cy + (dy or 0), cz + (dz or 0)) end)
                                    end
                                end
                            end
                        end
                    end)
                end
            end

            -- attach a backreference so fallback GetPosition can call helpers
            proxy._component_owner = self
            table.insert(rt.childProxies, proxy)
        end

        -- init end position
        local sx, sy, sz = self:_unpack_pos(self:GetPosition())
        self.endPosition[1], self.endPosition[2], self.endPosition[3] = sx, sy, sz
        self.chainLength = 0.0

        self:Log("PlayerChain started. Links:", tostring(#rt.childProxies), "AutoStart=" .. tostring(self.AutoStart))
        self:PositionBoxes()

        if self.AutoStart then
            self:Log("AutoStart enabled; starting extension")
            self:StartExtension()
        end

        -- dump initial positions once
        self:DumpLinkPositions()
    end,

    Update = function(self, dt)
        if self._disabled_due_to_missing_engine then return end
        self._runtime = self._runtime or {}
        local rt = self._runtime
        rt.childProxies = rt.childProxies or {}
        rt.childTransforms = rt.childTransforms or {}

        if self._input_missing and type(Input) == "table" then
            self._input_missing = false
            self:Log("Input became available")
        end

        if not self.playerTransform then
            self.playerTransform = Engine.FindTransformByName(self.PlayerName)
        end

        if not self._input_missing then
            local keyEnum = Input.Key and Input.Key[self.TriggerKey]
            if keyEnum and Input.GetKeyDown and Input.GetKeyDown(keyEnum) then
                if not self.isExtending and not self.isRetracting then
                    if self.currentState == COMPLETELY_LAX or self.currentState == LAX then
                        self:StartExtension()
                    end
                end
            end
        end

        if self.isExtending then self:ExtendChain(dt) end
        if self.isRetracting then self:RetractChain(dt) end

        self:PositionBoxes()
        self:CheckState()

        if self.DumpEveryFrame then self:DumpLinkPositions() end
    end,

    GetChainState = function(self) return self.currentState end,
    GetChainLength = function(self) return self.chainLength end,
    GetEndPosition = function(self) return { self.endPosition[1], self.endPosition[2], self.endPosition[3] } end,

    StartExtension = function(self)
        self.isExtending = true
        self.isRetracting = false
        self.extensionTime = 0.0
        self.currentState = EXTENDING
        self.chainLength = 0.0
        local sx, sy, sz = self:_unpack_pos(self:GetPosition())
        self.endPosition[1], self.endPosition[2], self.endPosition[3] = sx, sy, sz
        self.lastForward = self:GetForwardDirection()
        self:Log("Extension started")
    end,

    ExtendChain = function(self, dt)
        self.extensionTime = self.extensionTime + dt
        local desired = self.ChainSpeed * self.extensionTime
        if self.MaxLength > 0 and desired > self.MaxLength then desired = self.MaxLength end

        local forward = self.lastForward
        local sx, sy, sz = self:_unpack_pos(self:GetPosition())

        local newEndX = sx + forward[1] * desired
        local newEndY = sy + forward[2] * desired
        local newEndZ = sz + forward[3] * desired

        local hitFound, hitX, hitY, hitZ = false, nil, nil, nil
        if self.SimulatedHitDistance and self.SimulatedHitDistance > 0.0 then
            local hitDist = self.SimulatedHitDistance
            if self.MaxLength > 0 and hitDist > self.MaxLength then hitDist = self.MaxLength end
            if desired >= hitDist then
                hitFound = true
                hitX = sx + forward[1] * hitDist
                hitY = sy + forward[2] * hitDist
                hitZ = sz + forward[3] * hitDist
            end
        end

        if hitFound then
            self.endPosition[1], self.endPosition[2], self.endPosition[3] = hitX, hitY, hitZ
            local dx, dy, dz = hitX - sx, hitY - sy, hitZ - sz
            self.chainLength = math.sqrt(dx*dx + dy*dy + dz*dz)
            self:Log("Hit detected at distance " .. tostring(self.chainLength))
            self:StopExtension()
            return
        end

        self.endPosition[1], self.endPosition[2], self.endPosition[3] = newEndX, newEndY, newEndZ
        self.chainLength = desired

        if (self.MaxLength > 0 and self.chainLength >= self.MaxLength) or self.extensionTime > 10.0 then
            self:Log("Extension auto-stopping (length=" .. tostring(self.chainLength) .. ")")
            self:StopExtension()
        end
    end,

    StartRetraction = function(self)
        if self.chainLength <= 0 then return end
        self.isRetracting = true
        self.isExtending = false
        self.currentState = RETRACTING
        self:Log("Retraction started")
    end,

    RetractChain = function(self, dt)
        self.chainLength = math.max(0, self.chainLength - self.ChainSpeed * dt)
        local sx, sy, sz = self:_unpack_pos(self:GetPosition())
        local forward = self.lastForward or self:GetForwardDirection()
        local ex = sx + forward[1] * self.chainLength
        local ey = sy + forward[2] * self.chainLength
        local ez = sz + forward[3] * self.chainLength
        self.endPosition[1], self.endPosition[2], self.endPosition[3] = ex, ey, ez

        if self.chainLength <= 0 then
            self:Log("Retraction complete")
            self.isRetracting = false
            self.currentState = COMPLETELY_LAX
        end
    end,

    StopExtension = function(self)
        if not self.isExtending then return end
        self.isExtending = false
        if self.chainLength <= 0.0 then
            local sx, sy, sz = self:_unpack_pos(self:GetPosition())
            self.endPosition[1], self.endPosition[2], self.endPosition[3] = sx, sy, sz
            self.currentState = COMPLETELY_LAX
            self:Log("Extension stopped - completely lax")
        else
            self.currentState = LAX
            self:Log("Extension stopped - lax at length " .. tostring(self.chainLength))
        end
    end,

    -- PositionBoxes: only move links that are within the current chainLength reach
    PositionBoxes = function(self)
        self._runtime = self._runtime or {}
        local rt = self._runtime
        rt.childProxies = rt.childProxies or {}
        local linkCount = #rt.childProxies
        if linkCount == 0 then return end

        local sx, sy, sz = self:_unpack_pos(self:GetPosition())
        local ex, ey, ez = self.endPosition[1], self.endPosition[2], self.endPosition[3]

        local dx_e, dy_e, dz_e = ex - sx, ey - sy, ez - sz
        local curEndDist = math.sqrt(dx_e*dx_e + dy_e*dy_e + dz_e*dz_e)
        local dir = {0,0,1}
        if curEndDist > 1e-6 then dir = { dx_e/curEndDist, dy_e/curEndDist, dz_e/curEndDist } end

        -- total length used to compute segment spacing (prefer MaxLength if >0)
        local totalLen = (self.MaxLength and self.MaxLength > 0) and self.MaxLength or math.max(curEndDist, 1e-6)
        local segmentLen = (linkCount > 1) and (totalLen / (linkCount - 1)) or 0

        for i, proxy in ipairs(rt.childProxies) do
            if proxy and proxy.Move and proxy.GetPosition then
                local requiredDist = (i - 1) * segmentLen
                -- only place this link if the chainLength has reached this link's required distance
                if self.chainLength + 1e-6 >= requiredDist then
                    local placeDist = math.min(requiredDist, curEndDist)
                    local targetX = sx + dir[1] * placeDist
                    local targetY = sy + dir[2] * placeDist
                    local targetZ = sz + dir[3] * placeDist
                    local cx, cy, cz = proxy:GetPosition()
                    cx, cy, cz = cx or 0, cy or 0, cz or 0
                    local dx, dy, dz = targetX - cx, targetY - cy, targetZ - cz
                    if math.abs(dx) > 1e-6 or math.abs(dy) > 1e-6 or math.abs(dz) > 1e-6 then
                        proxy:Move(dx, dy, dz)
                    end
                end
            end
        end

        if not self._positionedOnce then
            self._positionedOnce = true
            self:Log("Positioned", tostring(linkCount), "links (seglen=" .. tostring(segmentLen) .. ")")
        end
    end,

    CheckState = function(self)
        if self.isExtending then
            self.currentState = EXTENDING
        elseif self.isRetracting then
            self.currentState = RETRACTING
        else
            if self.chainLength <= 0.0 then
                self.currentState = COMPLETELY_LAX
            else
                if not self.playerTransform then
                    self.playerTransform = Engine.FindTransformByName(self.PlayerName)
                    if not self.playerTransform then
                        self.currentState = LAX
                    end
                end
                if self.playerTransform then
                    local px, py, pz = self:_unpack_pos(Engine.GetTransformPosition(self.playerTransform))
                    local sx, sy, sz = self:_unpack_pos(self:GetPosition())
                    local dx, dy, dz = px - sx, py - sy, pz - sz
                    local playerDist = math.sqrt(dx*dx + dy*dy + dz*dz)
                    if playerDist > self.chainLength + 0.01 then
                        self.currentState = TAUT
                    else
                        if playerDist < math.max(0.01, self.chainLength * 0.25) then
                            self.currentState = COMPLETELY_LAX
                        else
                            self.currentState = LAX
                        end
                    end
                end
            end
        end

        if self.lastState ~= self.currentState then
            self.lastState = self.currentState
            local name = "UNKNOWN"
            if self.currentState == COMPLETELY_LAX then name = "COMPLETELY_LAX" end
            if self.currentState == LAX then name = "LAX" end
            if self.currentState == TAUT then name = "TAUT" end
            if self.currentState == EXTENDING then name = "EXTENDING" end
            if self.currentState == RETRACTING then name = "RETRACTING" end
            self:Log("State changed to", name, "(chainLength=" .. tostring(self.chainLength) .. ")")
        end
    end,

    GetForwardDirection = function(self)
        if self.ForwardOverride and type(self.ForwardOverride) == "table" and #self.ForwardOverride >= 3 then
            local fx,fy,fz = self.ForwardOverride[1], self.ForwardOverride[2], self.ForwardOverride[3]
            local mag = math.sqrt(fx*fx + fy*fy + fz*fz)
            if mag > 0.0001 then return { fx/mag, fy/mag, fz/mag } end
        end
        if self.GetTransformForward then
            local ok, r1, r2, r3 = pcall(function() return self:GetTransformForward() end)
            if ok then
                if type(r1) == "table" then
                    local fx, fy, fz = r1[1] or r1.x or 0.0, r1[2] or r1.y or 0.0, r1[3] or r1.z or 0.0
                    local mag = math.sqrt(fx*fx + fy*fy + fz*fz)
                    if mag > 0.0001 then return { fx/mag, fy/mag, fz/mag } end
                elseif type(r1) == "number" and type(r2) == "number" and type(r3) == "number" then
                    local mag = math.sqrt(r1*r1 + r2*r2 + r3*r3)
                    if mag > 0.0001 then return { r1/mag, r2/mag, r3/mag } end
                end
            end
        end
        return { 0.0, 0.0, 1.0 }
    end,

}
