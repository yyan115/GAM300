-- PlayerChain_rewrite.lua
-- PlayerChain component with Verlet physics integration
-- Optimized with minimal logging and exposed state for external scripts

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
        TriggerKey = "E",
        PlayerName = "Player",
        SimulatedHitDistance = 0.0,
        ForwardOverride = nil,
        EnableLogs = false,  -- Default off for performance
        AutoStart = false,
        DumpEveryFrame = false,
        -- Verlet settings
        VerletGravity = 9.81,           -- Gravity magnitude (set to 0 to disable bouncing)
        VerletDamping = 0.02,           -- Damping (0-1, higher = more damping)
        ConstraintIterations = 2,       -- Distance constraint iterations (higher = stiffer)
        EnableVerletPhysics = true,     -- Toggle Verlet simulation on/off

        -- New: Elasticity control
        IsElastic = true,               -- If false, chain cannot stretch beyond LinkMaxDistance per segment
        LinkMaxDistance = 0.15,          -- Maximum allowed distance between adjacent links when IsElastic == false

        -- Read-only status fields (for editor display) - these are authoritative for external scripts
        m_CurrentState = "COMPLETELY_LAX",
        m_CurrentLength = 0.0,
        m_IsExtending = false,
        m_IsRetracting = false,
        m_LinkCount = 0
    },

    -------------------------------------------------------------------------
    -- Lightweight helpers
    -------------------------------------------------------------------------
    _unpack_pos = function(self, a, b, c)
        if type(a) == "table" then
            return a[1] or a.x or 0.0, a[2] or a.y or 0.0, a[3] or a.z or 0.0
        end
        return (type(a) == "number") and a or 0.0,
               (type(b) == "number") and b or 0.0,
               (type(c) == "number") and c or 0.0
    end,

    Log = function(self, ...)
        if not self.EnableLogs then return end
        local parts = {}
        for i, v in ipairs({...}) do parts[i] = tostring(v) end
        print("[PlayerChain] " .. table.concat(parts, " "))
    end,

    _read_transform_position = function(self, tr)
        if not tr then return 0,0,0 end
        local ok, a, b, c = pcall(function() return tr:GetPosition() end)
        if ok then
            if type(a) == "table" then
                return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
            end
            if type(a) == "number" and type(b) == "number" and type(c) == "number" then
                return a, b, c
            end
        end
        if type(tr.localPosition) == "table" or type(tr.localPosition) == "userdata" then
            local pos = tr.localPosition
            return pos.x or pos[1] or 0, pos.y or pos[2] or 0, pos.z or pos[3] or 0
        end
        return 0,0,0
    end,

    _read_world_pos = function(self, tr)
        return self:_read_transform_position(tr)
    end,

    _write_world_pos = function(self, tr, x, y, z)
        if not tr then return false end

        local rt = self._runtime
        if rt and rt.childTransforms and rt.childProxies then
            for i = 1, #rt.childTransforms do
                if rt.childTransforms[i] == tr then
                    local proxy = rt.childProxies[i]
                    if proxy and proxy.SetPosition then
                        local ok = pcall(function() proxy:SetPosition(x, y, z) end)
                        if ok then return true end
                    end
                    break
                end
            end
        end

        if type(tr.localPosition) ~= "nil" then
            local pos = tr.localPosition
            if type(pos) == "userdata" then
                pcall(function()
                    pos.x, pos.y, pos.z = x, y, z
                    tr.isDirty = true
                end)
                return true
            elseif type(pos) == "table" then
                pcall(function()
                    pos.x, pos.y, pos.z = x, y, z
                    tr.isDirty = true
                end)
                return true
            end
        end
        return false
    end,

    -------------------------------------------------------------------------
    -- Backwards compatibility: sync authoritative m_ fields with legacy aliases
    -------------------------------------------------------------------------
    _sync_to_aliases = function(self)
        -- copy authoritative m_ fields into legacy names used by existing modules
        self.chainLength     = self.m_CurrentLength or 0.0
        self.isExtending     = (self.m_IsExtending == true)
        self.isRetracting    = (self.m_IsRetracting == true)
    end,

    _sync_from_aliases = function(self)
        -- after physics/legacy code runs, copy any changes back into editor-facing m_ fields
        if self.chainLength ~= nil then
            self.m_CurrentLength = self.chainLength
        else
            self.m_CurrentLength = self.m_CurrentLength or 0.0
        end
        self.m_IsExtending   = (self.isExtending == true)
        self.m_IsRetracting  = (self.isRetracting == true)
    end,

    -------------------------------------------------------------------------
    -- Lifecycle
    -------------------------------------------------------------------------
    Start = function(self)
        if type(Engine) ~= "table" then
            print("[PlayerChain] ERROR: Engine global missing.")
            self._disabled_due_to_missing_engine = true
            return
        end

        self._runtime = self._runtime or {}
        local rt = self._runtime
        rt.childTransforms = {}
        rt.childProxies = {}

        if type(Input) ~= "table" then
            self._input_missing = true
        end

        -- Authoritative state uses m_ fields (avoid redundant copies)
        self.currentState = COMPLETELY_LAX                         -- numeric internal state for fast branching
        self.m_CurrentState = "COMPLETELY_LAX"
        self.m_CurrentLength = 0.0
        self.m_IsExtending = false
        self.m_IsRetracting = false
        self.m_LinkCount = 0

        -- Internal helpers (not authoritative)
        self.extensionTime = 0.0
        self.lastForward = {0.0, 0.0, 1.0}
        self.playerTransform = nil
        self.lastState = nil
        self.endPosition = {0.0, 0.0, 0.0}

        -- Find link transforms
        for i = 1, math.max(1, self.NumberOfLinks) do
            local name = "Link" .. tostring(i)
            local tr = Engine.FindTransformByName(name)
            if tr then
                table.insert(rt.childTransforms, tr)
                pcall(function()
                    if tr.GetComponent then
                        local rb = tr:GetComponent("Rigidbody")
                        if rb and rb.isKinematic ~= nil then
                            rb.isKinematic = true
                        end
                    end
                end)
            end
        end

        -- Create proxies with TransformMixin
        for i, tr in ipairs(rt.childTransforms) do
            local proxy = {}
            function proxy:GetComponent(compName)
                if compName == "Transform" then return tr end
                return nil
            end

            if TransformMixin and type(TransformMixin.apply) == "function" then
                TransformMixin.apply(proxy)
            else
                -- Fallback implementation
                function proxy:GetPosition()
                    return self._component_owner and self._component_owner:_read_transform_position(tr) or 0,0,0
                end
                function proxy:SetPosition(x, y, z)
                    if type(tr.localPosition) ~= "nil" then
                        local pos = tr.localPosition
                        if type(pos) == "userdata" or type(pos) == "table" then
                            pos.x, pos.y, pos.z = x, y, z
                            tr.isDirty = true
                        end
                    end
                end
            end

            proxy._component_owner = self
            table.insert(rt.childProxies, proxy)
        end

        -- Initialize Verlet (module expected to exist)
        Verlet.InitVerlet(self)

        local sx, sy, sz = self:_unpack_pos(self:GetPosition())
        self.endPosition[1], self.endPosition[2], self.endPosition[3] = sx, sy, sz
        self.m_CurrentLength = 0.0

        if self.EnableLogs then
            self:Log("Started with", #rt.childProxies, "links, AutoStart:", self.AutoStart)
        end

        -- Initialize editor display fields
        self.m_LinkCount = #rt.childProxies
        self.m_CurrentState = "COMPLETELY_LAX"
        self.m_CurrentLength = 0.0
        self.m_IsExtending = false
        self.m_IsRetracting = false

        -- Sync to legacy aliases so Verlet/legacy code sees initial values
        self:_sync_to_aliases()

        if self.AutoStart then
            self:StartExtension()
        end
    end,

    Update = function(self, dt)
        if self._disabled_due_to_missing_engine then return end

        local rt = self._runtime or {}
        rt.childProxies = rt.childProxies or {}

        -- Check for input availability
        if self._input_missing and type(Input) == "table" then
            self._input_missing = false
        end

        -- Cache player transform
        if not self.playerTransform then
            self.playerTransform = Engine.FindTransformByName(self.PlayerName)
        end

        -- Handle input
        if not self._input_missing then
            local keyEnum = Input.Key and Input.Key[self.TriggerKey]
            if keyEnum and Input.GetKeyDown and Input.GetKeyDown(keyEnum) then
                if not self.m_IsExtending and not self.m_IsRetracting then
                    if self.currentState == COMPLETELY_LAX or self.currentState == LAX then
                        self:StartExtension()
                    end
                end
            end
        end

        -- Update chain mechanics (m_ fields are authoritative)
        if self.m_IsExtending then
            self:ExtendChain(dt)
        end
        if self.m_IsRetracting then
            self:RetractChain(dt)
        end

        -- Ensure legacy aliases are synced before physics/legacy modules run
        self:_sync_to_aliases()

        -- Physics simulation
        if self.EnableVerletPhysics then
            Verlet.VerletStep(self, dt)
        else
            -- Fallback: simple linear positioning without physics
            self:PositionLinksSimple()
        end

        -- Update Link Rotations
        self:UpdateLinkRotations()

        -- Copy back anything physics/legacy code changed
        self:_sync_from_aliases()

        -- State management
        self:CheckState()

        -- Update editor display fields (keeps m_ authoritative)
        self:UpdateEditorFields()

        if self.DumpEveryFrame then
            self:DumpLinkPositions()
        end
    end,

    -------------------------------------------------------------------------
    -- PUBLIC API - Exposed for external scripts
    -------------------------------------------------------------------------
    GetChainState = function(self)
        return self.currentState
    end,

    GetChainStateString = function(self)
        return self.m_CurrentState or "UNKNOWN"
    end,

    GetChainLength = function(self)
        return self.m_CurrentLength
    end,

    GetEndPosition = function(self)
        return {
            x = self.endPosition[1],
            y = self.endPosition[2],
            z = self.endPosition[3]
        }
    end,

    GetLinkPositions = function(self)
        local positions = {}
        local rt = self._runtime or {}
        if rt.childProxies then
            for i, proxy in ipairs(rt.childProxies) do
                if proxy.GetPosition then
                    local x, y, z = proxy:GetPosition()
                    positions[i] = {x = x, y = y, z = z}
                end
            end
        end
        return positions
    end,

    GetLinkPosition = function(self, index)
        local rt = self._runtime or {}
        if rt.childProxies and rt.childProxies[index] then
            local proxy = rt.childProxies[index]
            if proxy.GetPosition then
                local x, y, z = proxy:GetPosition()
                return {x = x, y = y, z = z}
            end
        end
        return nil
    end,

    IsExtending = function(self)
        return self.m_IsExtending == true
    end,

    IsRetracting = function(self)
        return self.m_IsRetracting == true
    end,

    GetLinkCount = function(self)
        return self.m_LinkCount or 0
    end,

    -------------------------------------------------------------------------
    -- CHAIN MECHANICS
    -------------------------------------------------------------------------
    StartExtension = function(self)
        self.m_IsExtending = true
        self.m_IsRetracting = false
        self.extensionTime = 0.0
        self.currentState = EXTENDING
        self.m_CurrentState = "EXTENDING"
        self.m_CurrentLength = 0.0
        local sx, sy, sz = self:_unpack_pos(self:GetPosition())
        self.endPosition[1], self.endPosition[2], self.endPosition[3] = sx, sy, sz
        self.lastForward = self:GetForwardDirection()

        -- Keep aliases in sync for any immediate legacy calls
        self:_sync_to_aliases()

        if self.EnableLogs then
            self:Log("Extension started")
        end
    end,

    ExtendChain = function(self, dt)
        self.extensionTime = self.extensionTime + dt
        local desired = self.ChainSpeed * self.extensionTime
        if self.MaxLength > 0 and desired > self.MaxLength then
            desired = self.MaxLength
        end

        -- If non-elastic, clamp desired length to maximum allowed by LinkMaxDistance
        local rt = self._runtime or {}
        local linkCount = (rt.childProxies and #rt.childProxies) or math.max(1, self.NumberOfLinks)
        if not self.IsElastic then
            local maxAllowed = self.LinkMaxDistance * (math.max(1, linkCount) - 1)
            if desired > maxAllowed then desired = maxAllowed end
        end

        local forward = self.lastForward
        local sx, sy, sz = self:_unpack_pos(self:GetPosition())

        local newEndX = sx + forward[1] * desired
        local newEndY = sy + forward[2] * desired
        local newEndZ = sz + forward[3] * desired

        -- Check for simulated hit
        if self.SimulatedHitDistance and self.SimulatedHitDistance > 0.0 then
            local hitDist = self.SimulatedHitDistance
            if self.MaxLength > 0 and hitDist > self.MaxLength then
                hitDist = self.MaxLength
            end
            if desired >= hitDist then
                local hitX = sx + forward[1] * hitDist
                local hitY = sy + forward[2] * hitDist
                local hitZ = sz + forward[3] * hitDist
                self.endPosition[1], self.endPosition[2], self.endPosition[3] = hitX, hitY, hitZ
                local dx, dy, dz = hitX - sx, hitY - sy, hitZ - sz
                self.m_CurrentLength = math.sqrt(dx*dx + dy*dy + dz*dz)
                if self.EnableLogs then
                    self:Log("Hit at distance", self.m_CurrentLength)
                end
                self:StopExtension()
                return
            end
        end

        self.endPosition[1], self.endPosition[2], self.endPosition[3] = newEndX, newEndY, newEndZ
        self.m_CurrentLength = desired

        if (self.MaxLength > 0 and self.m_CurrentLength >= self.MaxLength) or self.extensionTime > 10.0 then
            if self.EnableLogs then
                self:Log("Extension stopped at length", self.m_CurrentLength)
            end
            self:StopExtension()
        end
    end,

    StartRetraction = function(self)
        if (self.m_CurrentLength or 0) <= 0 then return end
        self.m_IsRetracting = true
        self.m_IsExtending = false
        self.currentState = RETRACTING
        self.m_CurrentState = "RETRACTING"

        -- Keep aliases in sync for legacy code
        self:_sync_to_aliases()

        if self.EnableLogs then
            self:Log("Retraction started")
        end
    end,

    RetractChain = function(self, dt)
        self.m_CurrentLength = math.max(0, (self.m_CurrentLength or 0) - self.ChainSpeed * dt)
        local sx, sy, sz = self:_unpack_pos(self:GetPosition())
        local forward = self.lastForward or self:GetForwardDirection()
        local ex = sx + forward[1] * self.m_CurrentLength
        local ey = sy + forward[2] * self.m_CurrentLength
        local ez = sz + forward[3] * self.m_CurrentLength
        self.endPosition[1], self.endPosition[2], self.endPosition[3] = ex, ey, ez

        if self.m_CurrentLength <= 0 then
            if self.EnableLogs then
                self:Log("Retraction complete")
            end
            self.m_IsRetracting = false
            self.currentState = COMPLETELY_LAX
            self.m_CurrentState = "COMPLETELY_LAX"
        end
    end,

    StopExtension = function(self)
        -- Accept either legacy alias or authoritative m_ field
        if not self.m_IsExtending and not self.isExtending then return end

        -- Clear both authoritative and alias flags
        self.m_IsExtending = false
        self.isExtending = false

        if (self.m_CurrentLength or 0) <= 0.0 then
            local sx, sy, sz = self:_unpack_pos(self:GetPosition())
            self.endPosition[1], self.endPosition[2], self.endPosition[3] = sx, sy, sz
            self.currentState = COMPLETELY_LAX
            self.m_CurrentState = "COMPLETELY_LAX"
            self.chainLength = 0.0
            self.m_CurrentLength = 0.0
        else
            self.currentState = LAX
            self.m_CurrentState = "LAX"
        end

        -- Keep aliases in sync for legacy modules
        self:_sync_to_aliases()
    end,

    -- Replace UpdateChainRotations with this parallel-transport-based version
    UpdateLinkRotations = function(self)
        local rt = self._runtime or {}
        local proxies = rt.childProxies or {}
        local transforms = rt.childTransforms or {}
        local n = #transforms
        if n <= 0 then return end

        local EPS = 1e-8

        -- math helpers
        local function len(x,y,z) return math.sqrt((x or 0)*(x or 0) + (y or 0)*(y or 0) + (z or 0)*(z or 0)) end
        local function normalize(x,y,z)
            local L = len(x,y,z)
            if L < EPS then return 0.0, 0.0, 0.0 end
            return x / L, y / L, z / L
        end
        local function dot(ax,ay,az, bx,by,bz) return (ax or 0)*(bx or 0) + (ay or 0)*(by or 0) + (az or 0)*(bz or 0) end
        local function cross(ax,ay,az, bx,by,bz) return (ay or 0)*(bz or 0) - (az or 0)*(by or 0), (az or 0)*(bx or 0) - (ax or 0)*(bz or 0), (ax or 0)*(by or 0) - (ay or 0)*(bx or 0) end

        -- quat helpers
        local function quat_from_axis_angle(ax,ay,az, angle)
            local nx,ny,nz = normalize(ax,ay,az)
            if nx == 0 and ny == 0 and nz == 0 then return 1.0,0.0,0.0,0.0 end
            local h = angle * 0.5
            local s = math.sin(h)
            return math.cos(h), nx * s, ny * s, nz * s
        end
        local function rotate_vec_by_quat(qw,qx,qy,qz, vx,vy,vz)
            -- optimized quaternion-vector multiplication: v' = q * (0,v) * q^-1
            local tx = 2 * ( qy * vz - qz * vy )
            local ty = 2 * ( qz * vx - qx * vz )
            local tz = 2 * ( qx * vy - qy * vx )
            local vpx = vx + qw * tx + ( qy * tz - qz * ty )
            local vpy = vy + qw * ty + ( qz * tx - qx * tz )
            local vpz = vz + qw * tz + ( qx * ty - qy * tx )
            return vpx, vpy, vpz
        end

        -- matrix->quat converter (expects columns: col0=RIGHT, col1=UP, col2=FORWARD)
        local function mat3_to_quat(m00,m10,m20, m01,m11,m21, m02,m12,m22)
            local tr = m00 + m11 + m22
            local qw,qx,qy,qz
            if tr > 0 then
                local S = math.sqrt(tr + 1.0) * 2.0
                qw = 0.25 * S
                qx = (m21 - m12) / S
                qy = (m02 - m20) / S
                qz = (m10 - m01) / S
            else
                if (m00 > m11) and (m00 > m22) then
                    local S = math.sqrt(1.0 + m00 - m11 - m22) * 2.0
                    qw = (m21 - m12) / S
                    qx = 0.25 * S
                    qy = (m01 + m10) / S
                    qz = (m02 + m20) / S
                elseif m11 > m22 then
                    local S = math.sqrt(1.0 + m11 - m00 - m22) * 2.0
                    qw = (m02 - m20) / S
                    qx = (m01 + m10) / S
                    qy = 0.25 * S
                    qz = (m12 + m21) / S
                else
                    local S = math.sqrt(1.0 + m22 - m00 - m11) * 2.0
                    qw = (m10 - m01) / S
                    qx = (m02 + m20) / S
                    qy = (m12 + m21) / S
                    qz = 0.25 * S
                end
            end
            return qw or 1, qx or 0, qy or 0, qz or 0
        end

        -- robust position reader: prefer runtime (Verlet) positions, then component-level reader if provided, then transform/proxy
        local function read_position(i)
            -- prefer runtime positions if present (Verlet stores world positions in rt.p)
            if type(rt.p) == "table" and rt.p[i] and type(rt.p[i]) == "table" then
                return rt.p[i][1] or 0.0, rt.p[i][2] or 0.0, rt.p[i][3] or 0.0
            end

            -- try component-provided world reader
            local tr = transforms[i]
            if self and type(self._read_world_pos) == "function" and tr then
                local ok, a,b,c = pcall(function() return self:_read_world_pos(tr) end)
                if ok then
                    if type(a) == "table" then return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0 end
                    if type(a) == "number" and type(b) == "number" and type(c) == "number" then return a,b,c end
                end
            end

            -- try proxy:GetPosition
            local proxy = proxies[i]
            if proxy and type(proxy.GetPosition) == "function" then
                local ok, a,b,c = pcall(function() return proxy:GetPosition() end)
                if ok then
                    if type(a) == "table" then return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0 end
                    if type(a) == "number" and type(b) == "number" and type(c) == "number" then return a,b,c end
                end
            end

            -- try transform:GetPosition
            if tr then
                local ok, a,b,c = pcall(function() return tr:GetPosition() end)
                if ok then
                    if type(a) == "table" then return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0 end
                    if type(a) == "number" and type(b) == "number" and type(c) == "number" then return a,b,c end
                end
                -- fallback localPosition
                if type(tr.localPosition) == "table" or type(tr.localPosition) == "userdata" then
                    local pos = tr.localPosition
                    return pos.x or pos[1] or 0, pos.y or pos[2] or 0, pos.z or pos[3] or 0
                end
            end

            return 0.0, 0.0, 0.0
        end

        -- get start world: prefer component-specific getter (Verlet offers _get_start_world)
        local function get_start_world()
            if self and type(self._get_start_world) == "function" then
                local ok, a,b,c = pcall(function() return self:_get_start_world() end)
                if ok and type(a) == "number" and type(b) == "number" and type(c) == "number" then
                    return a,b,c
                end
            end
            -- fallback to component/GetPosition + unpack
            if type(self.GetPosition) == "function" then
                local ok, a,b,c = pcall(function() return self:GetPosition() end)
                if ok then
                    if type(a) == "table" then return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0 end
                    if type(a) == "number" and type(b) == "number" and type(c) == "number" then return a,b,c end
                end
            end
            -- final fallback to 0,0,0
            return 0,0,0
        end

        -- end position: prefer component.endPosition if set, else treat equal to start
        local function get_end_world()
            if type(self.endPosition) == "table" and #self.endPosition >= 3 then
                return self.endPosition[1] or 0, self.endPosition[2] or 0, self.endPosition[3] or 0
            end
            return get_start_world()
        end

        -- Build authoritative positions array (size n)
        local positions = {}
        for i = 1, n do
            local x,y,z = read_position(i)
            positions[i] = { x, y, z }
        end

        -- Ensure endpoints exist if runtime doesn't provide them
        if not (type(rt.p) == "table" and #rt.p >= n) then
            local sx,sy,sz = get_start_world()
            local ex,ey,ez = get_end_world()
            positions[1] = positions[1] or { sx, sy, sz }
            positions[n] = positions[n] or { ex, ey, ez }
            positions[1][1], positions[1][2], positions[1][3] = sx, sy, sz
            positions[n][1], positions[n][2], positions[n][3] = ex, ey, ez
        end

        -- compute per-link forward vectors (local direction) — same idea as your old code
        local forward = {}
        for i = 1, n do
            local fx,fy,fz = 0,0,0
            if n == 1 then
                local sx, sy, sz = get_start_world()
                local ex, ey, ez = get_end_world()
                fx,fy,fz = ex - sx, ey - sy, ez - sz
            else
                if i == 1 then
                    local nx,ny,nz = positions[i+1][1], positions[i+1][2], positions[i+1][3]
                    fx,fy,fz = nx - positions[i][1], ny - positions[i][2], nz - positions[i][3]
                elseif i == n then
                    local px,py,pz = positions[i-1][1], positions[i-1][2], positions[i-1][3]
                    fx,fy,fz = positions[i][1] - px, positions[i][2] - py, positions[i][3] - pz
                else
                    local px,py,pz = positions[i-1][1], positions[i-1][2], positions[i-1][3]
                    local nx,ny,nz = positions[i+1][1], positions[i+1][2], positions[i+1][3]
                    fx,fy,fz = (nx - px) * 0.5, (ny - py) * 0.5, (nz - pz) * 0.5
                end
            end
            local nfx,nfy,nfz = normalize(fx,fy,fz)
            if nfx == 0 and nfy == 0 and nfz == 0 then
                -- fallback to global start->end if degenerate
                local sx,sy,sz = get_start_world()
                local ex,ey,ez = get_end_world()
                nfx,nfy,nfz = normalize(ex - sx, ey - sy, ez - sz)
                if nfx == 0 and nfy == 0 and nfz == 0 then nfx,nfy,nfz = 1.0,0.0,0.0 end
            end
            forward[i] = { nfx, nfy, nfz }
        end

        -- WORLD up (use Y-up to match your working code)
        local WORLD_UP_X, WORLD_UP_Y, WORLD_UP_Z = 0.0, 1.0, 0.0

        -- For each link build world-space frame, apply alternating twist, convert -> quaternion (w,x,y,z), write
        for i = 1, n do
            local fx,fy,fz = forward[i][1], forward[i][2], forward[i][3]

            -- pick reference up (avoid near-parallel)
            local refUpX, refUpY, refUpZ = WORLD_UP_X, WORLD_UP_Y, WORLD_UP_Z
            if math.abs(dot(fx,fy,fz, refUpX,refUpY,refUpZ)) > 0.99 then
                refUpX, refUpY, refUpZ = 1.0, 0.0, 0.0
            end

            -- right = up × forward  (matches your old working code)
            local rx,ry,rz = cross(refUpX, refUpY, refUpZ, fx, fy, fz)
            rx,ry,rz = normalize(rx,ry,rz)

            -- guard degenerate right
            if rx == 0 and ry == 0 and rz == 0 then
                if math.abs(fx) < 0.9 then rx,ry,rz = 1,0,0 else rx,ry,rz = 0,1,0 end
                local proj = dot(rx,ry,rz, fx,fy,fz)
                rx,ry,rz = rx - proj * fx, ry - proj * fy, rz - proj * fz
                rx,ry,rz = normalize(rx,ry,rz)
            end

            -- up = forward × right
            local ux,uy,uz = cross(fx,fy,fz, rx,ry,rz)
            ux,uy,uz = normalize(ux,uy,uz)

            -- alternate ±90° twist about forward in world-space
            if (i % 2) == 0 then
                local angle = math.pi * 0.5
                local qw,qx,qy,qz = quat_from_axis_angle(fx,fy,fz, angle)
                rx,ry,rz = rotate_vec_by_quat(qw,qx,qy,qz, rx,ry,rz)
                ux,uy,uz = rotate_vec_by_quat(qw,qx,qy,qz, ux,uy,uz)
                rx,ry,rz = normalize(rx,ry,rz)
                ux,uy,uz = normalize(ux,uy,uz)
            end

            -- Build matrix columns: RIGHT, UP, FORWARD (mat3_to_quat expects this)
            local m00,m10,m20 = rx, ry, rz   -- column 0 = RIGHT
            local m01,m11,m21 = ux, uy, uz   -- column 1 = UP
            local m02,m12,m22 = fx, fy, fz   -- column 2 = FORWARD

            -- Convert -> quaternion (w,x,y,z)
            local qw,qx,qy,qz = mat3_to_quat(m00,m10,m20, m01,m11,m21, m02,m12,m22)

            -- normalize quaternion
            local ql = math.sqrt((qw or 0)*(qw or 0) + (qx or 0)*(qx or 0) + (qy or 0)*(qy or 0) + (qz or 0)*(qz or 0))
            if ql > 1e-9 then qw,qx,qy,qz = qw/ql, qx/ql, qy/ql, qz/ql else qw,qx,qy,qz = 1,0,0,0 end

            -- write rotation in (w,x,y,z) order
            local tr = transforms[i]
            if type(self._set_transform_rotation) == "function" then
                pcall(function() self:_set_transform_rotation(tr, qw, qx, qy, qz) end)
            else
                pcall(function()
                    if tr and tr.localRotation then
                        local rot = tr.localRotation
                        if type(rot) == "table" then rot.w, rot.x, rot.y, rot.z = qw,qx,qy,qz; tr.isDirty = true
                        elseif type(rot) == "userdata" then rot.w, rot.x, rot.y, rot.z = qw,qx,qy,qz; tr.isDirty = true end
                    end
                end)
            end
        end
    end,


    _get_start_world = function(self)
        return self:_unpack_pos(self:GetPosition())
    end,

    CheckState = function(self)
        self.m_CurrentLength = self.m_CurrentLength or 0.0

        if self.m_IsExtending then
            self.currentState = EXTENDING
            self.m_CurrentState = "EXTENDING"
        elseif self.m_IsRetracting then
            self.currentState = RETRACTING
            self.m_CurrentState = "RETRACTING"
        else
            if self.m_CurrentLength <= 0.0 then
                self.currentState = COMPLETELY_LAX
                self.m_CurrentState = "COMPLETELY_LAX"
            else
                if not self.playerTransform then
                    self.playerTransform = Engine.FindTransformByName(self.PlayerName)
                end
                if self.playerTransform then
                    local px, py, pz
                    local ok, a, b, c = pcall(function() return Engine.GetTransformPosition(self.playerTransform) end)
                    if ok then
                        px, py, pz = self:_unpack_pos(a, b, c)
                    else
                        px, py, pz = self:_unpack_pos(self.playerTransform)
                    end

                    local sx, sy, sz = self:_unpack_pos(self:GetPosition())
                    local dx, dy, dz = px - sx, py - sy, pz - sz
                    local playerDist = math.sqrt(dx*dx + dy*dy + dz*dz)

                    if playerDist > self.m_CurrentLength + 0.01 then
                        self.currentState = TAUT
                        self.m_CurrentState = "TAUT"
                    elseif playerDist < math.max(0.01, self.m_CurrentLength * 0.25) then
                        self.currentState = COMPLETELY_LAX
                        self.m_CurrentState = "COMPLETELY_LAX"
                    else
                        self.currentState = LAX
                        self.m_CurrentState = "LAX"
                    end
                else
                    self.currentState = LAX
                    self.m_CurrentState = "LAX"
                end
            end
        end

        if self.EnableLogs and self.lastState ~= nil and self.lastState ~= self.currentState then
            self:Log("State changed to", self:GetChainStateString())
        end
        self.lastState = self.currentState
    end,

    GetForwardDirection = function(self)
        if self.ForwardOverride and type(self.ForwardOverride) == "table" and #self.ForwardOverride >= 3 then
            local fx, fy, fz = self.ForwardOverride[1], self.ForwardOverride[2], self.ForwardOverride[3]
            local mag = math.sqrt(fx*fx + fy*fy + fz*fz)
            if mag > 0.0001 then
                return {fx/mag, fy/mag, fz/mag}
            end
        end
        if self.GetTransformForward then
            local ok, r1, r2, r3 = pcall(function() return self:GetTransformForward() end)
            if ok then
                if type(r1) == "table" then
                    local fx, fy, fz = r1[1] or r1.x or 0.0, r1[2] or r1.y or 0.0, r1[3] or r1.z or 0.0
                    local mag = math.sqrt(fx*fx + fy*fy + fz*fz)
                    if mag > 0.0001 then
                        return {fx/mag, fy/mag, fz/mag}
                    end
                elseif type(r1) == "number" and type(r2) == "number" and type(r3) == "number" then
                    local mag = math.sqrt(r1*r1 + r2*r2 + r3*r3)
                    if mag > 0.0001 then
                        return {r1/mag, r2/mag, r3/mag}
                    end
                end
            end
        end
        return {0.0, 0.0, 1.0}
    end,

    DumpLinkPositions = function(self)
        local rt = self._runtime or {}
        if not rt.childTransforms then return end
        for i, tr in ipairs(rt.childTransforms) do
            local x, y, z = self:_read_transform_position(tr)
            self:Log(string.format("Link %d pos=(%.3f, %.3f, %.3f)", i, x, y, z))
        end
    end,

    UpdateEditorFields = function(self)
        -- m_ fields are authoritative; keep them for editor display
        self.m_CurrentState = self:GetChainStateString()
        self.m_CurrentLength = self.m_CurrentLength or 0.0
        self.m_IsExtending = (self.m_IsExtending == true)
        self.m_IsRetracting = (self.m_IsRetracting == true)
        local rt = self._runtime or {}
        self.m_LinkCount = (rt.childProxies and #rt.childProxies) or 0
    end,

    -- Simple linear positioning (no physics, no bouncing)
    -- Now respects IsElastic and LinkMaxDistance (prevents per-link stretching when IsElastic == false)
    PositionLinksSimple = function(self)
        local rt = self._runtime or {}
        if not rt.childProxies or #rt.childProxies == 0 then return end

        local sx, sy, sz = self:_unpack_pos(self:GetPosition())
        local ex, ey, ez = self.endPosition[1], self.endPosition[2], self.endPosition[3]

        local dx, dy, dz = ex - sx, ey - sy, ez - sz
        local dist = math.sqrt(dx*dx + dy*dy + dz*dz)

        if dist < 0.0001 then
            for i, proxy in ipairs(rt.childProxies) do
                if proxy and proxy.SetPosition then
                    proxy:SetPosition(sx, sy, sz)
                end
            end
            return
        end

        local dir = {dx/dist, dy/dist, dz/dist}
        local linkCount = #rt.childProxies

        -- Determine requested total length: prefer authoritative m_CurrentLength; fall back to measured distance
        local totalRequested = (self.m_CurrentLength and self.m_CurrentLength > 0) and self.m_CurrentLength or dist

        -- If MaxLength is set, do not exceed it
        if self.MaxLength and self.MaxLength > 0 then
            totalRequested = math.min(totalRequested, self.MaxLength)
        end

        -- If not elastic, clamp totalRequested to the maximum allowed by LinkMaxDistance
        if not self.IsElastic then
            local maxAllowed = self.LinkMaxDistance * math.max(0, (linkCount - 1))
            if totalRequested > maxAllowed then
                totalRequested = maxAllowed
                -- Also clamp authoritative length so other systems see the constrained value
                self.m_CurrentLength = totalRequested
            end
        end

        local segmentLen = (linkCount > 1) and (totalRequested / (linkCount - 1)) or 0

        -- If not elastic, ensure segmentLen never exceeds LinkMaxDistance
        if not self.IsElastic and segmentLen > self.LinkMaxDistance then
            segmentLen = self.LinkMaxDistance
        end

        -- Place each link along the direction vector up to the current effective distance
        for i, proxy in ipairs(rt.childProxies) do
            if proxy and proxy.SetPosition then
                local requiredDist = (i - 1) * segmentLen
                if (self.m_CurrentLength or 0) >= requiredDist then
                    local placeDist = math.min(requiredDist, dist)
                    local tx = sx + dir[1] * placeDist
                    local ty = sy + dir[2] * placeDist
                    local tz = sz + dir[3] * placeDist
                    proxy:SetPosition(tx, ty, tz)
                end
            end
        end
    end,
}
