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

    UpdateLinkRotations = function(self)
        local rt = self._runtime or {}
        if not rt.childTransforms or #rt.childTransforms == 0 then return end

        local sx, sy, sz = self:_unpack_pos(self:GetPosition())
        local ex, ey, ez = self.endPosition[1], self.endPosition[2], self.endPosition[3]
        
        -- Calculate direction from start to end
        local dx, dy, dz = ex - sx, ey - sy, ez - sz
        local dist = math.sqrt(dx*dx + dy*dy + dz*dz)
        
        if dist < 0.0001 then
            -- Chain is collapsed, use default forward
            dx, dy, dz = 0, 0, 1
            dist = 1
        end
        
        -- Normalize direction
        local dirX, dirY, dirZ = dx/dist, dy/dist, dz/dist
        
        -- Default up vector (uses Y-up as your code did)
        local upX, upY, upZ = 0, 1, 0
        
        -- If direction is too close to up, use alternate up
        if math.abs(dirX*upX + dirY*upY + dirZ*upZ) > 0.99 then
            upX, upY, upZ = 1, 0, 0
        end
        
        -- Calculate right vector (cross product: up × dir)
        local rightX = upY * dirZ - upZ * dirY
        local rightY = upZ * dirX - upX * dirZ
        local rightZ = upX * dirY - upY * dirX
        local rightLen = math.sqrt(rightX*rightX + rightY*rightY + rightZ*rightZ)
        
        if rightLen < 1e-6 then
            -- fallback: make right perpendicular to dir using an arbitrary axis
            if math.abs(dirX) < 0.9 then
                rightX, rightY, rightZ = 1, 0, 0
            else
                rightX, rightY, rightZ = 0, 1, 0
            end
            -- orthogonalize
            local proj = (rightX*dirX + rightY*dirY + rightZ*dirZ)
            rightX = rightX - proj * dirX
            rightY = rightY - proj * dirY
            rightZ = rightZ - proj * dirZ
            rightLen = math.sqrt(rightX*rightX + rightY*rightY + rightZ*rightZ)
        end

        rightX, rightY, rightZ = rightX/rightLen, rightY/rightLen, rightZ/rightLen
        
        -- Recalculate up vector (cross product: dir × right)
        upX = dirY * rightZ - dirZ * rightY
        upY = dirZ * rightX - dirX * rightZ
        upZ = dirX * rightY - dirY * rightX
        local upLen = math.sqrt(upX*upX + upY*upY + upZ*upZ)
        if upLen > 1e-6 then
            upX, upY, upZ = upX / upLen, upY / upLen, upZ / upLen
        end

        -- For each link, alternate rotation by 90 degrees around the chain axis
        for i, tr in ipairs(rt.childTransforms) do
            local finalRightX, finalRightY, finalRightZ = rightX, rightY, rightZ
            local finalUpX, finalUpY, finalUpZ = upX, upY, upZ
            
            -- Apply ±90° twist around forward for alternating links
            if (i % 2) == 0 then
                -- Rotate right and up 90° around forward (Rodrigues)
                local angle = math.pi / 2
                local c = math.cos(angle)
                local s = math.sin(angle)

                -- rotate right vector
                local dot_right = dirX * rightX + dirY * rightY + dirZ * rightZ
                local crossX = dirY * rightZ - dirZ * rightY
                local crossY = dirZ * rightX - dirX * rightZ
                local crossZ = dirX * rightY - dirY * rightX
                finalRightX = rightX * c + crossX * s + dirX * dot_right * (1 - c)
                finalRightY = rightY * c + crossY * s + dirY * dot_right * (1 - c)
                finalRightZ = rightZ * c + crossZ * s + dirZ * dot_right * (1 - c)
                
                -- rotate up vector
                local dot_up = dirX * upX + dirY * upY + dirZ * upZ
                local crossUpX = dirY * upZ - dirZ * upY
                local crossUpY = dirZ * upX - dirX * upZ
                local crossUpZ = dirX * upY - dirY * upX
                finalUpX = upX * c + crossUpX * s + dirX * dot_up * (1 - c)
                finalUpY = upY * c + crossUpY * s + dirY * dot_up * (1 - c)
                finalUpZ = upZ * c + crossUpZ * s + dirZ * dot_up * (1 - c)

                -- re-normalize small numerical drift
                local rl = math.sqrt(finalRightX*finalRightX + finalRightY*finalRightY + finalRightZ*finalRightZ)
                if rl > 1e-6 then finalRightX, finalRightY, finalRightZ = finalRightX/rl, finalRightY/rl, finalRightZ/rl end
                local ul = math.sqrt(finalUpX*finalUpX + finalUpY*finalUpY + finalUpZ*finalUpZ)
                if ul > 1e-6 then finalUpX, finalUpY, finalUpZ = finalUpX/ul, finalUpY/ul, finalUpZ/ul end
            end
            
            -- Build rotation matrix (row-major rows are: [right.x, up.x, forward.x], [right.y, up.y, forward.y], [right.z, up.z, forward.z])
            local m00, m01, m02 = finalRightX, finalUpX, dirX
            local m10, m11, m12 = finalRightY, finalUpY, dirY
            local m20, m21, m22 = finalRightZ, finalUpZ, dirZ
            
            -- Convert matrix to quaternion (row-major)
            local trace = m00 + m11 + m22
            local qx, qy, qz, qw
            
            if trace > 0 then
                local s = math.sqrt(trace + 1.0) * 2
                qw = 0.25 * s
                qx = (m21 - m12) / s
                qy = (m02 - m20) / s
                qz = (m10 - m01) / s
            elseif m00 > m11 and m00 > m22 then
                local s = math.sqrt(1.0 + m00 - m11 - m22) * 2
                qw = (m21 - m12) / s
                qx = 0.25 * s
                qy = (m01 + m10) / s
                qz = (m02 + m20) / s
            elseif m11 > m22 then
                local s = math.sqrt(1.0 + m11 - m00 - m22) * 2
                qw = (m02 - m20) / s
                qx = (m01 + m10) / s
                qy = 0.25 * s
                qz = (m12 + m21) / s
            else
                local s = math.sqrt(1.0 + m22 - m00 - m11) * 2
                qw = (m10 - m01) / s
                qx = (m02 + m20) / s
                qy = (m12 + m21) / s
                qz = 0.25 * s
            end

            -- Normalize quaternion to avoid scaling / flips
            local qlen = math.sqrt((qw or 0)*(qw or 0) + (qx or 0)*(qx or 0) + (qy or 0)*(qy or 0) + (qz or 0)*(qz or 0))
            if qlen > 1e-9 then
                qw, qx, qy, qz = qw / qlen, qx / qlen, qy / qlen, qz / qlen
            else
                qw, qx, qy, qz = 1, 0, 0, 0
            end

            -- Write rotation: note _set_transform_rotation expects (transform, w, x, y, z)
            -- (If your helper expects a different order, swap accordingly.)
            if type(self._set_transform_rotation) == "function" then
                pcall(function() self:_set_transform_rotation(tr, qw, qx, qy, qz) end)
            else
                -- fallback: try writing localRotation directly if helper missing
                pcall(function()
                    if tr and tr.localRotation then
                        local rot = tr.localRotation
                        if type(rot) == "table" then
                            rot.w, rot.x, rot.y, rot.z = qw, qx, qy, qz
                            tr.isDirty = true
                        elseif type(rot) == "userdata" then
                            rot.w, rot.x, rot.y, rot.z = qw, qx, qy, qz
                            tr.isDirty = true
                        end
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
