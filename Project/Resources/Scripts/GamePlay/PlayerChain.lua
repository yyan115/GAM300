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
        VerletDamping = 0.02,            -- Damping (0-1, higher = more damping)
        ConstraintIterations = 2,        -- Distance constraint iterations (higher = stiffer)
        EnableVerletPhysics = true,      -- Toggle Verlet simulation on/off
        -- Read-only status fields (for editor display)
        m_CurrentState = "COMPLETELY_LAX",
        m_CurrentLength = 0.0,
        m_IsExtending = false,
        m_IsRetracting = false,
        m_LinkCount = 0
    },

    -- Lightweight helpers
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

    -- Optimized position read
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

    -- Optimized position write using TransformMixin proxies
    _write_world_pos = function(self, tr, x, y, z)
        if not tr then return false end
        
        -- Find and use the proxy with TransformMixin applied
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
        
        -- Fallback: direct manipulation
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

        -- Initialize state
        self.currentState = COMPLETELY_LAX
        self.chainLength = 0.0
        self.endPosition = {0.0, 0.0, 0.0}
        self.MaxLength = self.MaxLength or 0.0
        self.isExtending = false
        self.isRetracting = false
        self.extensionTime = 0.0
        self.lastForward = {0.0, 0.0, 1.0}
        self.playerTransform = nil
        self.lastState = nil

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

        -- Initialize Verlet
        Verlet.InitVerlet(self)

        local sx, sy, sz = self:_unpack_pos(self:GetPosition())
        self.endPosition[1], self.endPosition[2], self.endPosition[3] = sx, sy, sz
        self.chainLength = 0.0

        if self.EnableLogs then
            self:Log("Started with", #rt.childProxies, "links, AutoStart:", self.AutoStart)
        end
        
        -- Initialize editor display fields
        self.m_LinkCount = #rt.childProxies
        self.m_CurrentState = "COMPLETELY_LAX"
        self.m_CurrentLength = 0.0
        self.m_IsExtending = false
        self.m_IsRetracting = false

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
                if not self.isExtending and not self.isRetracting then
                    if self.currentState == COMPLETELY_LAX or self.currentState == LAX then
                        self:StartExtension()
                    end
                end
            end
        end

        -- Update chain mechanics
        if self.isExtending then 
            self:ExtendChain(dt) 
        end
        if self.isRetracting then 
            self:RetractChain(dt) 
        end

        -- Physics simulation
        if self.EnableVerletPhysics then
            Verlet.VerletStep(self, dt)
        else
            -- Fallback: simple linear positioning without physics
            self:PositionLinksSimple()
        end

        -- State management
        self:CheckState()
        
        -- Update editor display fields
        self:UpdateEditorFields()

        if self.DumpEveryFrame then 
            self:DumpLinkPositions() 
        end
    end,

    -- ========================================================================
    -- PUBLIC API - Exposed for external scripts
    -- ========================================================================
    
    -- Get current chain state (returns state constant)
    GetChainState = function(self) 
        return self.currentState 
    end,
    
    -- Get chain state as string
    GetChainStateString = function(self)
        local names = {
            [COMPLETELY_LAX] = "COMPLETELY_LAX",
            [LAX] = "LAX",
            [TAUT] = "TAUT",
            [EXTENDING] = "EXTENDING",
            [RETRACTING] = "RETRACTING"
        }
        return names[self.currentState] or "UNKNOWN"
    end,
    
    -- Get current chain length
    GetChainLength = function(self) 
        return self.chainLength 
    end,
    
    -- Get end position as table {x, y, z}
    GetEndPosition = function(self) 
        return {
            x = self.endPosition[1], 
            y = self.endPosition[2], 
            z = self.endPosition[3]
        }
    end,
    
    -- Get all link positions (returns array of {x, y, z} tables)
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
    
    -- Get specific link position by index
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
    
    -- Check if chain is actively extending
    IsExtending = function(self) 
        return self.isExtending == true 
    end,
    
    -- Check if chain is actively retracting
    IsRetracting = function(self) 
        return self.isRetracting == true 
    end,
    
    -- Get number of links
    GetLinkCount = function(self)
        local rt = self._runtime or {}
        return rt.childProxies and #rt.childProxies or 0
    end,

    -- ========================================================================
    -- CHAIN MECHANICS
    -- ========================================================================

    StartExtension = function(self)
        self.isExtending = true
        self.isRetracting = false
        self.extensionTime = 0.0
        self.currentState = EXTENDING
        self.chainLength = 0.0
        local sx, sy, sz = self:_unpack_pos(self:GetPosition())
        self.endPosition[1], self.endPosition[2], self.endPosition[3] = sx, sy, sz
        self.lastForward = self:GetForwardDirection()
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
                self.chainLength = math.sqrt(dx*dx + dy*dy + dz*dz)
                if self.EnableLogs then
                    self:Log("Hit at distance", self.chainLength)
                end
                self:StopExtension()
                return
            end
        end

        self.endPosition[1], self.endPosition[2], self.endPosition[3] = newEndX, newEndY, newEndZ
        self.chainLength = desired

        if (self.MaxLength > 0 and self.chainLength >= self.MaxLength) or self.extensionTime > 10.0 then
            if self.EnableLogs then
                self:Log("Extension stopped at length", self.chainLength)
            end
            self:StopExtension()
        end
    end,

    StartRetraction = function(self)
        if self.chainLength <= 0 then return end
        self.isRetracting = true
        self.isExtending = false
        self.currentState = RETRACTING
        if self.EnableLogs then
            self:Log("Retraction started")
        end
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
            if self.EnableLogs then
                self:Log("Retraction complete")
            end
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
        else
            self.currentState = LAX
        end
    end,

    _get_start_world = function(self)
        return self:_unpack_pos(self:GetPosition())
    end,

    CheckState = function(self)
        self.chainLength = self.chainLength or 0.0
        
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
                end
                if self.playerTransform then
                    local px, py, pz = self:_unpack_pos(Engine.GetTransformPosition(self.playerTransform))
                    local sx, sy, sz = self:_unpack_pos(self:GetPosition())
                    local dx, dy, dz = px - sx, py - sy, pz - sz
                    local playerDist = math.sqrt(dx*dx + dy*dy + dz*dz)
                    
                    if playerDist > self.chainLength + 0.01 then
                        self.currentState = TAUT
                    elseif playerDist < math.max(0.01, self.chainLength * 0.25) then
                        self.currentState = COMPLETELY_LAX
                    else
                        self.currentState = LAX
                    end
                else
                    self.currentState = LAX
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
    
    -- Update editor-visible status fields
    UpdateEditorFields = function(self)
        self.m_CurrentState = self:GetChainStateString()
        self.m_CurrentLength = self.chainLength or 0.0
        self.m_IsExtending = self.isExtending == true
        self.m_IsRetracting = self.isRetracting == true
        local rt = self._runtime or {}
        self.m_LinkCount = (rt.childProxies and #rt.childProxies) or 0
    end,
    
    -- Simple linear positioning (no physics, no bouncing)
    PositionLinksSimple = function(self)
        local rt = self._runtime or {}
        if not rt.childProxies or #rt.childProxies == 0 then return end
        
        local sx, sy, sz = self:_unpack_pos(self:GetPosition())
        local ex, ey, ez = self.endPosition[1], self.endPosition[2], self.endPosition[3]
        
        local dx, dy, dz = ex - sx, ey - sy, ez - sz
        local dist = math.sqrt(dx*dx + dy*dy + dz*dz)
        
        if dist < 0.0001 then
            -- All links at start position
            for i, proxy in ipairs(rt.childProxies) do
                if proxy and proxy.SetPosition then
                    proxy:SetPosition(sx, sy, sz)
                end
            end
            return
        end
        
        local dir = {dx/dist, dy/dist, dz/dist}
        local linkCount = #rt.childProxies
        local totalLen = (self.MaxLength and self.MaxLength > 0) and self.MaxLength or math.max(dist, 0.0001)
        local segmentLen = (linkCount > 1) and (totalLen / (linkCount - 1)) or 0
        
        for i, proxy in ipairs(rt.childProxies) do
            if proxy and proxy.SetPosition then
                local requiredDist = (i - 1) * segmentLen
                if (self.chainLength or 0) >= requiredDist then
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