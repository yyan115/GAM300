-- ChainBootstrap.lua
-- Editor-facing component: create links, expose fields, wire handler + controller

local Component = require("extension.mono_helper")
local LinkHandlerModule = require("GamePlay.ChainLinkTransformHandler")
local ControllerModule = require("GamePlay.ChainController")

return Component {
    -- editor-exposed fields (trimmed; add more as needed)
    fields = {
        NumberOfLinks = 200,
        LinkName = "Link",
        ChainSpeed = 10.0,
        MaxLength = 0.0,
        TriggerKey = "G",
        PlayerName = "Kusane_Player_LeftHandMiddle1",
        SimulatedHitDistance = 0.0,
        EnableLogs = false,
        AutoStart = false,
        VerletGravity = 9.81,
        VerletDamping = 0.02,
        ConstraintIterations = 2,
        IsElastic = true,
        LinkMaxDistance = 0.025,
        PinEndWhenExtended = true,
        AnchorAngleThresholdDeg = 45
    },

    -------------------------------------------------------------------------
    -- Position readers (copied from PlayerChain for compatibility)
    -------------------------------------------------------------------------
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
        -- prefer Engine world-position helpers if available, fallback to transform reader
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
        -- fallback to component-level transform position reader
        return self:_read_transform_position(tr)
    end,

    -------------------------------------------------------------------------
    -- Chain Controls
    -------------------------------------------------------------------------
    -- Input handlers driving controller behavior
    _on_chain_down = function(self, payload)
        self._chain_pressing = true
        self._chain_held = false
        print("RECEIVED Down")
    end,

    _on_chain_up = function(self, payload)
        self._chain_pressing = false
        self._chain_held = false
        print("RECEIVED UP")
        if not self.controller then return end

        local len = (self.controller.chainLen or 0)
        local isExt = self.controller.isExtending or false
        local isRet = self.controller.isRetracting or false

        -- If chain is fully retracted, releasing the chain button starts extension
        if not isExt and not isRet and len <= 1e-4 then
            self.controller:StartExtension()
            return
        end

        -- If chain has any length (extended/lax), pressing -> retract
        if len > 1e-4 and (not isRet) then
            self.controller:StartRetraction()
            return
        end
    end,

    _on_chain_hold = function(self, payload)
        -- optional: set a flag if you want hold-specific behavior
        self._chain_held = true
        print("RECEIVED Held")
    end,

    -------------------------------------------------------------------------
    -- Class Initialization
    -------------------------------------------------------------------------
    Start = function(self)
        -- create entity duplicates as before
        if self.NumberOfLinks > 0 then
            Engine.CreateEntityDup(self.LinkName, self.LinkName, self.NumberOfLinks)
        end

        -- gather transforms
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

        -- seed positions to current transform positions
        for i, tr in ipairs(self._runtime.childTransforms) do
            local x,y,z = self:_read_transform_position(tr)
            self.controller.positions[i][1], self.controller.positions[i][2], self.controller.positions[i][3] = x,y,z
            self.controller.prev[i][1], self.controller.prev[i][2], self.controller.prev[i][3] = x,y,z
        end

        -- cache player transform and seed controller startPos to avoid startup jumps
        self.playerTransform = Engine.FindTransformByName(self.PlayerName)
        if self.playerTransform then
            local sx, sy, sz = self:_read_world_pos(self.playerTransform)
            self.controller.startPos = { sx, sy, sz }
        else
            -- stable fallback: component's current position (less likely to be the link origin)
            local sx, sy, sz = self:_unpack_pos(self:GetPosition())
            self.controller.startPos = { sx, sy, sz }
        end

        -- subscribe to chain input events (use existing global event bus subscribe API)
        self._chainSubDown = nil
        self._chainSubUp = nil
        self._chainSubHold = nil
        if _G.event_bus and _G.event_bus.subscribe then
            self._chainSubDown = _G.event_bus.subscribe("chain.down", function(payload)
                if not payload then return end
                pcall(function() self:_on_chain_down(payload) end)
            end)
            self._chainSubUp = _G.event_bus.subscribe("chain.up", function(payload)
                if not payload then return end
                pcall(function() self:_on_chain_up(payload) end)
            end)
            self._chainSubHold = _G.event_bus.subscribe("chain.hold", function(payload)
                if not payload then return end
                pcall(function() self:_on_chain_hold(payload) end)
            end)
        end

        -- optionally autos start
        if self.AutoStart then
            self.controller:StartExtension()
        end
    end,

    Update = function(self, dt)
        if not self.controller then return end

        -- provide callback to get start world pos
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
                -- prefer cached playerTransform; try to find it once per frame if not cached
                if not self.playerTransform then
                    self.playerTransform = Engine.FindTransformByName(self.PlayerName)
                end
                if self.playerTransform then
                    return self:_read_world_pos(self.playerTransform)
                end
                -- fallback to last known controller startPos (avoid jumping to link transform origins)
                local sp = (self.controller and self.controller.startPos) or { self:_unpack_pos(self:GetPosition()) }
                return sp[1], sp[2], sp[3]
            end
        }

        local positions, startPos, endPos = self.controller:Update(dt, settings)

        -- write positions to transforms
        self.linkHandler:ApplyPositions(positions)

        -- update rotations (use linkHandler routine)
        local maxStep = (self.RotationMaxStepRadians or (self.RotationMaxStep and math.rad(self.RotationMaxStep))) or math.rad(60)
        self.linkHandler:ApplyRotations(positions, startPos, endPos, maxStep, true)

        -- expose editor-visible fields
        local public = self.controller:GetPublicState()
        self.m_CurrentLength = public.ChainLength
        self.m_IsExtending = public.IsExtending
        self.m_IsRetracting = public.IsRetracting
        self.m_LinkCount = public.LinkCount

        if self.EnableLogs and self.DumpEveryFrame then
            self.linkHandler:Dump()
        end
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._chainSubDown then pcall(function() _G.event_bus.unsubscribe(self._chainSubDown) end) end
            if self._chainSubUp then pcall(function() _G.event_bus.unsubscribe(self._chainSubUp) end) end
            if self._chainSubHold then pcall(function() _G.event_bus.unsubscribe(self._chainSubHold) end) end
        end
    end,

    -- Public API hooks forward to controller
    StartExtension = function(self) if self.controller then self.controller:StartExtension() end end,
    StartRetraction = function(self) if self.controller then self.controller:StartRetraction() end end,
    StopExtension = function(self) if self.controller then self.controller:StopExtension() end end,
    GetChainState = function(self) return { Length = self.m_CurrentLength, Count = self.m_LinkCount } end
}
