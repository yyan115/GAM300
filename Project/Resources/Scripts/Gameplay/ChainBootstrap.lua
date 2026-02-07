-- ChainBootstrap.lua (refactored wiring: use component Subscribe, set controller start/end via API, seed positions safely)
local Component = require("extension.mono_helper")
local LinkHandlerModule = require("Gameplay.ChainLinkTransformHandler")
local ControllerModule = require("Gameplay.ChainController")

return Component {
    fields = {
        NumberOfLinks = 200,
        LinkName = "Link",
        ChainSpeed = 10.0,
        MaxLength = 10.0,
        PlayerName = "Kusane_Player_LeftHandMiddle1",
        EnableLogs = false,
        AutoStart = false,
        VerletGravity = 0.5,
        VerletDamping = 0.02,
        ConstraintIterations = 20,
        IsElastic = false,
        LinkMaxDistance = 0.025,
        PinEndWhenExtended = true,
        AnchorAngleThresholdDeg = 45
    },

    _unpack_pos = function(self, a, b, c)
        if type(a) == "table" then
            return a[1] or a.x or 0.0, a[2] or a.y or 0.0, a[3] or a.z or 0.0
        end
        return (type(a) == "number") and a or 0.0,
               (type(b) == "number") and b or 0.0,
               (type(c) == "number") and c or 0.0
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
        if not tr then return 0,0,0 end
        local ok, a, b, c = pcall(function() return Engine.GetTransformWorldPosition and Engine.GetTransformWorldPosition(tr) end)
        if ok and a ~= nil then
            if type(a) == "table" then
                return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
            end
            if type(a) == "number" and type(b) == "number" and type(c) == "number" then
                return a, b, c
            end
        end
        return self:_read_transform_position(tr)
    end,

    -- Attempt to set world position robustly. Falls back to SetPosition/localPosition.
    _write_world_pos = function(self, tr, x, y, z)
        if not tr then return false end
        -- Preferred engine API
        local ok = false
        if Engine and type(Engine.SetTransformWorldPosition) == "function" then
            ok = pcall(function() Engine.SetTransformWorldPosition(tr, x, y, z) end)
            if ok then return true end
        end

        -- Some engines expose SetPosition on transform expecting world coordinates
        if type(tr.SetPosition) == "function" then
            local suc = pcall(function() tr:SetPosition(x, y, z) end)
            if suc then return true end
        end

        -- Last resort: write localPosition (component may be in world-space already)
        if type(tr.localPosition) ~= "nil" then
            pcall(function()
                local pos = tr.localPosition
                if type(pos) == "table" then
                    pos.x, pos.y, pos.z = x, y, z
                    tr.isDirty = true
                elseif type(pos) == "userdata" then
                    pos.x, pos.y, pos.z = x, y, z
                    tr.isDirty = true
                end
            end)
            return true
        end

        return false
    end,
    
    _on_chain_down = function(self, payload)
        print("down")
        self._chain_pressing = true
        self._chain_held = false
    end,

    _on_chain_up = function(self, payload)
        print("up chain control")
        self._chain_pressing = false
        self._chain_held = false
        
        -- Deactivate camera aiming
        if event_bus and event_bus.publish then
            event_bus.publish("chain.aim_camera", {active = false})
        end
        
        if not self.controller then return end

        local len = (self.controller.chainLen or 0)
        local isExt = self.controller.isExtending or false
        local isRet = self.controller.isRetracting or false

        if not isExt and not isRet and len <= 1e-4 then self.controller:StartExtension(self._cameraForward) end

        if len > 1e-4 and (not isRet) then
            self.controller:StartRetraction()
            return
        end
    end,

    _on_chain_hold = function(self, payload)
        print("hold")
        self._chain_held = true

        -- Tell camera to move to aim position ONLY if chain is not extended
        if self.controller then
            local len = (self.controller.chainLen or 0)
            local isExt = self.controller.isExtending or false
            
            if len <= 1e-4 and not isExt then
                -- Chain is retracted, activate camera aiming
                if event_bus and event_bus.publish then
                    event_bus.publish("chain.aim_camera", {active = true})
                end
            end
        end
    end,

    Start = function(self)
        if self.NumberOfLinks > 0 then
            Engine.CreateEntityDup(self.LinkName, self.LinkName, self.NumberOfLinks)
        end

        self._runtime = self._runtime or {}
        self._runtime.childTransforms = {}
        for i = 1, math.max(1, self.NumberOfLinks) do
            local name = self.LinkName .. tostring(i)
            local tr = Engine.FindTransformByName(name)
            if tr then table.insert(self._runtime.childTransforms, tr) end
        end

        -- instantiate handler and controller
        self.linkHandler = LinkHandlerModule.New(self)
        self.linkHandler:InitTransforms(self._runtime.childTransforms)

        local params = {
            NumberOfLinks = self.NumberOfLinks,
            ChainSpeed = self.ChainSpeed,
            MaxLength = self.MaxLength,
            IsElastic = self.IsElastic,
            LinkMaxDistance = self.LinkMaxDistance,
            VerletGravity = self.VerletGravity,
            VerletDamping = self.VerletDamping,
            ConstraintIterations = self.ConstraintIterations,
            PinEndWhenExtended = self.PinEndWhenExtended,
            AnchorAngleThresholdRad = math.rad(self.AnchorAngleThresholdDeg or 45)
        }
        self.controller = ControllerModule.New(params)

        -- seed positions to current transform world positions (world-space)
        for i, tr in ipairs(self._runtime.childTransforms) do
            local x,y,z
            -- prefer engine world getter when available
            local ok, a,b,c = pcall(function() 
                if Engine and Engine.GetTransformWorldPosition then 
                    return Engine.GetTransformWorldPosition(tr) 
                end
                return nil
            end)
            if ok and a ~= nil then
                if type(a) == "table" then x,y,z = a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
                else x,y,z = a,b,c end
            else
                x,y,z = self:_read_world_pos(tr)
            end

            if self.controller.positions[i] then
                self.controller.positions[i][1], self.controller.positions[i][2], self.controller.positions[i][3] = x, y, z
                self.controller.prev[i][1], self.controller.prev[i][2], self.controller.prev[i][3] = x, y, z
            end
        end
        
        -- Initialize camera forward with fallback
        self._cameraForward = {0, 0, 1}

        -- fallback to global event bus if Subscribe unavailable (defensive)
        if _G.event_bus and _G.event_bus.subscribe then
            self._cameraForwardSub = _G.event_bus.subscribe("ChainAim_basis", function(payload)
                if payload and payload.forward then
                    local fwd = payload.forward
                    local fx = fwd.x or fwd[1] or 0
                    local fy = fwd.y or fwd[2] or 0
                    local fz = fwd.z or fwd[3] or 0
                    print(fx)
                    print(fy)
                    print(fz)
                    -- Negate to get the direction the camera is looking (not towards camera)
                    --fx = -fx
                    --fz = -fz

                    -- Normalize to be safe
                    local mag = math.sqrt(fx*fx + fy*fy + fz*fz)
                    if mag > 0.0001 then
                        self._cameraForward = {fx/mag, fy/mag, fz/mag}
                    end
                end
            end)
            self._chainSubDown = _G.event_bus.subscribe("chain.down", function(payload) if not payload then return end pcall(function() self:_on_chain_down(payload) end) end)
            self._chainSubUp = _G.event_bus.subscribe("chain.up", function(payload) if not payload then return end pcall(function() self:_on_chain_up(payload) end) end)
            self._chainSubHold = _G.event_bus.subscribe("chain.hold", function(payload) if not payload then return end pcall(function() self:_on_chain_hold(payload) end) end)
        end

        if self.AutoStart then
            self.controller:StartExtension(self._cameraForward)
        end
    end,

    Update = function(self, dt)
        if not self.controller then return end
        
        -- determine start position (cache player transform if present)
        self.playerTransform = Engine.FindTransformByName(self.PlayerName)
        if self.playerTransform then
            local sx, sy, sz = self:_read_world_pos(self.playerTransform)
            self.controller:SetStartPos(sx, sy, sz)
        else
            print("Cannot find the bloody player, WHY. YOU NAMED WRONG IS IT OR ENGINE FAILING AGAIN.")
        end

        
        -- quick debug dump (temporary)
        local function dump_state(ctrl)
            if not ctrl then return end
            local n = ctrl.n or 0
            local first = ctrl.positions[1]; local last = ctrl.positions[n]
            print(string.format("[CHAIN DEBUG] len=%.3f extend=%s retract=%s start=(%.3f,%.3f,%.3f) end=(%.3f,%.3f,%.3f)",
                ctrl.chainLen or 0, tostring(ctrl.isExtending), tostring(ctrl.isRetracting),
                ctrl.startPos[1] or 0, ctrl.startPos[2] or 0, ctrl.startPos[3] or 0,
                ctrl.endPos[1] or 0, ctrl.endPos[2] or 0, ctrl.endPos[3] or 0))
            if first and last then
                print(string.format("[CHAIN DEBUG] p1=(%.3f,%.3f,%.3f) pn=(%.3f,%.3f,%.3f)", first[1],first[2],first[3], last[1],last[2],last[3]))
            end
            -- invMass summary
            local k=0
            for i=1,math.min(10,#ctrl.invMass) do if ctrl.invMass[i]==0 then k=k+1 end end
            print("[CHAIN DEBUG] front-kinematic-count(first10):", k)
            -- anchors
            local a={}
            for idx,_ in pairs(ctrl.anchors) do table.insert(a, idx) end
            if #a>0 then print("[CHAIN DEBUG] anchors:", table.concat(a, ",")) end
            print(self._cameraForward[1]);
            print(self._cameraForward[2]);
            print(self._cameraForward[3]);
        end
        -- call: dump_state(self.controller)
        local settings = {
            ChainSpeed = self.ChainSpeed,
            MaxLength = self.MaxLength,
            IsElastic = self.IsElastic,
            LinkMaxDistance = self.LinkMaxDistance,
            VerletGravity = self.VerletGravity,
            VerletDamping = self.VerletDamping,
            ConstraintIterations = self.ConstraintIterations,
            AnchorAngleThresholdRad = math.rad(self.AnchorAngleThresholdDeg or 45),
            PinEndWhenExtended = self.PinEndWhenExtended,
            getStart = function()
                if not self.playerTransform then
                    self.playerTransform = Engine.FindTransformByName(self.PlayerName)
                end
                if self.playerTransform then
                    return self:_read_world_pos(self.playerTransform)
                end
                local sp = (self.controller and self.controller.startPos) or { self:_unpack_pos(self:GetPosition()) }
                return sp[1], sp[2], sp[3]
            end
        }

        local positions, startPos, endPos = self.controller:Update(dt, settings)

        self.linkHandler:ApplyPositions(positions)

        local maxStep = (self.RotationMaxStepRadians or (self.RotationMaxStep and math.rad(self.RotationMaxStep))) or math.rad(60)
        self.linkHandler:ApplyRotations(positions, startPos, endPos, maxStep, true)

        local public = self.controller:GetPublicState()
        self.m_CurrentLength = public.ChainLength
        self.m_IsExtending = public.IsExtending
        self.m_IsRetracting = public.IsRetracting
        self.m_LinkCount = public.LinkCount

        if self.EnableLogs then
            dump_state(self.controller)
            --self.linkHandler:Dump()
        end
    end,

    OnDisable = function(self)
        -- mono_helper will automatically unsubscribe tokens registered via self:Subscribe
        -- fallback: if manual tokens were used, attempt to unsubscribe them defensively
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._cameraForwardSub then pcall(function() _G.event_bus.unsubscribe(self._cameraForwardSub) end) end
            if self._chainSubDown then pcall(function() _G.event_bus.unsubscribe(self._chainSubDown) end) end
            if self._chainSubUp then pcall(function() _G.event_bus.unsubscribe(self._chainSubUp) end) end
            if self._chainSubHold then pcall(function() _G.event_bus.unsubscribe(self._chainSubHold) end) end
        end
    end,

    StartExtension = function(self) if self.controller then self.controller:StartExtension(self._cameraForward) end end,
    StartRetraction = function(self) if self.controller then self.controller:StartRetraction() end end,
    StopExtension = function(self) if self.controller then self.controller:StopExtension() end end,
    GetChainState = function(self) return { Length = self.m_CurrentLength, Count = self.m_LinkCount } end
}
