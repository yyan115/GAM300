-- PlayerChain_rewrite.lua
-- PlayerChain_with_Verlet.lua
-- PlayerChain component adapted to use extension.verletGravity for lightweight physics-like behaviour.
-- Assumptions:
--  - Links are direct children (Link1..LinkN) of the chain object (one layer).
--  - Each link may have a Rigidbody; if present, the script will set it kinematic to avoid fighting the solver.
--  - World-space positioning is used throughout.

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local Verlet = require("extension.verletGravity")

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
        TriggerKey = "G",
        PlayerName = "Player",
        SimulatedHitDistance = 0.0,
        ForwardOverride = nil,
        EnableLogs = true,
        AutoStart = false,
        DumpEveryFrame = false,
        -- Verlet settings
        VerletGravity = 9.81,
        VerletDamping = 0.02,
        ConstraintIterations = 2
    },

    -- small helpers (kept same as previous world-space rewrite)
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

    -- world-read/write helpers (delegates to Verlet if needed)
    _read_world_pos = function(self, tr)
        if not tr then return 0,0,0 end
        local ok, a,b,c = pcall(function() return tr:GetPosition() end)
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

    _write_world_pos = function(self, tr, x, y, z)
        if not tr then return false end
        x,y,z = tonumber(x) or 0, tonumber(y) or 0, tonumber(z) or 0
        -- try SetPosition
        if type(tr) == "table" and type(tr.SetPosition) == "function" then
            local ok = pcall(function() tr:SetPosition(x,y,z) end)
            if ok then return true end
        end
        if type(Engine) == "table" and type(Engine.SetTransformPosition) == "function" then
            local ok2 = pcall(function() Engine.SetTransformPosition(tr, x, y, z) end)
            if ok2 then return true end
        end
        if type(tr) == "table" then
            pcall(function() tr.localPosition = { x = x, y = y, z = z }; tr.isDirty = true end)
            return true
        end
        return false
    end,

    Start = function(self)
        if type(Engine) ~= "table" then
            print("[PlayerChain] ERROR: Engine global missing. Aborting PlayerChain Start.")
            self._disabled_due_to_missing_engine = true
            return
        end

        self._runtime = self._runtime or {}
        local rt = self._runtime
        rt.childTransforms = {}
        rt.childProxies = {}

        if type(Input) ~= "table" then
            self._input_missing = true
            self:Log("WARNING: Input missing at Start; input disabled until available")
        end

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

        for i = 1, math.max(1, self.NumberOfLinks) do
            local name = "Link" .. tostring(i)
            local tr = Engine.FindTransformByName(name)
            if tr then
                table.insert(rt.childTransforms, tr)
                -- attempt to get and set Rigidbody kinematic if present
                pcall(function()
                    if tr.GetComponent then
                        local rb = tr:GetComponent("Rigidbody")
                        if rb and rb.isKinematic ~= nil then rb.isKinematic = true end
                    end
                end)
            else
                self:Log("warning - transform '" .. name .. "' not found")
            end
        end

        -- create Verlet proxies
        for i, tr in ipairs(rt.childTransforms) do
            local proxy = {}
            function proxy:GetComponent(compName) if compName == "Transform" then return tr end return nil end
            function proxy:GetPosition() return self._component_owner and self._component_owner:_read_world_pos(tr) or 0,0,0 end
            function proxy:Move(dx, dy, dz)
                local cx,cy,cz = proxy:GetPosition()
                proxy._component_owner:_write_world_pos(tr, cx + (dx or 0), cy + (dy or 0), cz + (dz or 0))
            end
            proxy._component_owner = self
            table.insert(rt.childProxies, proxy)
        end

        -- initialize verlet runtime using the Verlet extension
        Verlet.InitVerlet(self)

        local sx, sy, sz = self:_get_start_world()
        self.endPosition[1], self.endPosition[2], self.endPosition[3] = sx, sy, sz
        self.chainLength = 0.0

        self:Log("PlayerChain started. Links:", tostring(#rt.childProxies), "AutoStart=" .. tostring(self.AutoStart))
        if self.AutoStart then self:StartExtension() end
        if self.DumpEveryFrame then self:DumpLinkPositions() end
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
                    if self.currentState == COMPLETELY_LAX or self.currentState == LAX then self:StartExtension() end
                end
            end
        end

        if self.isExtending then self:ExtendChain(dt) end
        if self.isRetracting then self:RetractChain(dt) end

        -- Verlet step handles moving link transforms in world-space based on self.endPosition / self.chainLength
        Verlet.VerletStep(self, dt)

        self:CheckState()

        if self.DumpEveryFrame then self:DumpLinkPositions() end
    end,

    -- standard getters, StartExtension/ExtendChain/Retraction/StopExtension unchanged (use world-space helpers)
    GetChainState = function(self) return self.currentState end,
    GetChainLength = function(self) return self.chainLength end,
    GetEndPosition = function(self) return { self.endPosition[1], self.endPosition[2], self.endPosition[3] } end,

    StartExtension = function(self)
        self.isExtending = true
        self.isRetracting = false
        self.extensionTime = 0.0
        self.currentState = EXTENDING
        self.chainLength = 0.0
        local sx, sy, sz = self:_get_start_world()
        self.endPosition[1], self.endPosition[2], self.endPosition[3] = sx, sy, sz
        self.lastForward = self:GetForwardDirection()
        self:Log("Extension started")
    end,

    ExtendChain = function(self, dt)
        self.extensionTime = self.extensionTime + dt
        local desired = self.ChainSpeed * self.extensionTime
        if self.MaxLength > 0 and desired > self.MaxLength then desired = self.MaxLength end

        local forward = self.lastForward
        local sx, sy, sz = self:_get_start_world()

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
        local sx, sy, sz = self:_get_start_world()
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
            local sx, sy, sz = self:_get_start_world()
            self.endPosition[1], self.endPosition[2], self.endPosition[3] = sx, sy, sz
            self.currentState = COMPLETELY_LAX
            self:Log("Extension stopped - completely lax")
        else
            self.currentState = LAX
            self:Log("Extension stopped - lax at length " .. tostring(self.chainLength))
        end
    end,

    -- world start helper
    _get_start_world = function(self)
        local ok, a,b,c = pcall(function() return self:GetPosition() end)
        if ok then
            if type(a) == "table" then return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0 end
            if type(a) == "number" and type(b) == "number" and type(c) == "number" then return a,b,c end
        end
        return 0,0,0
    end,

    -- debug dump
    DumpLinkPositions = function(self)
        Verlet.DumpLinkPositions(self)
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