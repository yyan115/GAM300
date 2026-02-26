-- ChainBootstrap.lua (refactored wiring: use component Subscribe, set controller start/end via API, seed positions safely)
local Component = require("extension.mono_helper")
local LinkHandlerModule = require("Gameplay.ChainLinkTransformHandler")
local ControllerModule = require("Gameplay.ChainController")

return Component {
    fields = {
        NumberOfLinks = 200,
        LinkName = "Link",
        ChainSpeed = 100.0,
        MaxLength = 10.0,
        PlayerName = "Kusane_Player_LeftHandMiddle1",
        VerletGravity = 0.5,
        VerletDamping = 0.02,
        ConstraintIterations = 20,
        IsElastic = true,
        LinkMaxDistance = 0.025,
        PinEndWhenExtended = true,
        AnchorAngleThresholdDeg = 45,
        SubSteps = 4,
        ChainEndpointName = "ChainEndpoint",
        GroundClamp = true,
        GroundClampOffset = 0.1,
        WallClamp = true,
        WallClampInterval = 10,
        WallClampRadius = 0
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
        local ok = false
        if Engine and type(Engine.SetTransformWorldPosition) == "function" then
            ok = pcall(function() Engine.SetTransformWorldPosition(tr, x, y, z) end)
            if ok then return true end
        end
        if type(tr.SetPosition) == "function" then
            local suc = pcall(function() tr:SetPosition(x, y, z) end)
            if suc then return true end
        end
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

        -- Deactivate camera aiming
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("chain.aim_camera", {active = false})
        end

        if not self.controller then
            self._chain_held = false
            return
        end

        local len = (self.controller.chainLen or 0)
        local isExt = self.controller.isExtending or false
        local isRet = self.controller.isRetracting or false

        print(string.format("[ChainBootstrap] _on_chain_up: len=%.4f isExt=%s isRet=%s chain_held=%s",
            len, tostring(isExt), tostring(isRet), tostring(self._chain_held)))

        if not isExt and not isRet and len <= 1e-4 then
            if not self._chain_held then
                -- Quick tap: request player forward from PlayerMovement via event bus, fire on next Update
                self._pendingPlayerForward = nil
                self._pendingTapFire = true
                if _G.event_bus and _G.event_bus.publish then
                    _G.event_bus.publish("request_player_forward", true)
                end
                print("[ChainBootstrap] TAP -> requested player_forward_response")
            else
                -- Held then released: fire in camera aim direction
                local direction = self._cameraForward
                print(string.format("[ChainBootstrap] HOLD release -> camera forward: (%.3f, %.3f, %.3f)",
                    direction[1], direction[2], direction[3]))
                self.controller:StartExtension(direction, self.MaxLength, self.LinkMaxDistance)
            end
        elseif len > 1e-4 and (not isRet) then
            self.controller:StartRetraction()
        end

        -- Reset held flag AFTER we've used it above
        self._chain_held = false
    end,

    _on_chain_hold = function(self, payload)
        print("hold")
        self._chain_held = true

        -- Tell camera to move to aim position ONLY if chain is not extended
        if self.controller then
            local len = (self.controller.chainLen or 0)
            local isExt = self.controller.isExtending or false
            if len <= 1e-4 and not isExt then
                if _G.event_bus and _G.event_bus.publish then
                    _G.event_bus.publish("chain.aim_camera", {active = true})
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

        -- Init tap/hold state
        self._chain_pressing = false
        self._chain_held = false

        -- Pending tap-fire state (deferred one frame until player_forward_response arrives)
        self._pendingTapFire = false
        self._pendingPlayerForward = nil

        -- Find chain endpoint object
        self._endpointTransform = nil
        if self.ChainEndpointName and self.ChainEndpointName ~= "" then
            self._endpointTransform = Engine.FindTransformByName(self.ChainEndpointName)
            if not self._endpointTransform then
                print("[ChainBootstrap] WARNING: ChainEndpoint object not found: " .. tostring(self.ChainEndpointName))
            end
        end

        if _G.event_bus and _G.event_bus.subscribe then
            self._cameraForwardSub = _G.event_bus.subscribe("ChainAim_basis", function(payload)
                if payload and payload.forward then
                    local fwd = payload.forward
                    local fx = fwd.x or fwd[1] or 0
                    local fy = fwd.y or fwd[2] or 0
                    local fz = fwd.z or fwd[3] or 0
                    local mag = math.sqrt(fx*fx + fy*fy + fz*fz)
                    if mag > 0.0001 then
                        self._cameraForward = {fx/mag, fy/mag, fz/mag}
                    end
                end
            end)
            self._chainSubDown = _G.event_bus.subscribe("chain.down", function(payload) if not payload then return end pcall(function() self:_on_chain_down(payload) end) end)
            self._chainSubUp   = _G.event_bus.subscribe("chain.up",   function(payload) if not payload then return end pcall(function() self:_on_chain_up(payload)   end) end)
            self._chainSubHold = _G.event_bus.subscribe("chain.hold", function(payload) if not payload then return end pcall(function() self:_on_chain_hold(payload) end) end)

            -- Receive player forward direction response for tap-fire.
            -- Multiple PlayerMovement instances may respond (stale ones have nil _facingX/_facingZ
            -- and send 0,0,1). Only accept the first response that carries real values.
            self._subPlayerForward = _G.event_bus.subscribe("player_forward_response", function(payload)
                if not payload then return end
                if self._pendingPlayerForward then return end
                local x = payload.x
                local z = payload.z
                if not x or not z then return end
                self._pendingPlayerForward = { x, payload.y or 0, z }
            end)

            self._subHookedPos = _G.event_bus.subscribe("chain.endpoint_hooked_position", function(payload)
                if not payload then return end
                pcall(function()
                    if self.controller then
                        local aN = self.controller.activeN
                        local x, y, z = payload.x, payload.y, payload.z
                        self.controller.lockedEndPoint[1] = x
                        self.controller.lockedEndPoint[2] = y
                        self.controller.lockedEndPoint[3] = z
                        self.controller.positions[aN][1] = x
                        self.controller.positions[aN][2] = y
                        self.controller.positions[aN][3] = z
                        self.controller.prev[aN][1] = x
                        self.controller.prev[aN][2] = y
                        self.controller.prev[aN][3] = z
                    end
                end)
            end)

            self._subHitEntity = _G.event_bus.subscribe("chain.endpoint_hit_entity", function(payload)
                if not payload then return end
                pcall(function()
                    if self.controller then
                        print("[ChainBootstrap] Endpoint hit entity " .. tostring(payload.entityId) .. " — locking endpoint")
                        self.controller.endPointLocked = true
                    end
                end)
            end)
        end
    end,

    Update = function(self, dt)
        if not self.controller then return end

        -- Deferred tap-fire: wait until player_forward_response arrives from PlayerMovement
        if self._pendingTapFire then
            if self._pendingPlayerForward then
                local direction = self._pendingPlayerForward
                print(string.format("[ChainBootstrap] TAP (deferred) -> player forward: (%.3f, %.3f, %.3f)",
                    direction[1], direction[2], direction[3]))
                self.controller:StartExtension(direction, self.MaxLength, self.LinkMaxDistance)
                self._pendingTapFire = false
                self._pendingPlayerForward = nil
            end
            -- else: still waiting for response, will retry next frame
        end
        
        self.playerTransform = Engine.FindTransformByName(self.PlayerName)
        if self.playerTransform then
            local sx, sy, sz = self:_read_world_pos(self.playerTransform)
            self.controller:SetStartPos(sx, sy, sz)
        else
            print("Cannot find the bloody player, WHY. YOU NAMED WRONG IS IT OR ENGINE FAILING AGAIN.")
        end

        local settings = {
            ChainSpeed = self.ChainSpeed,
            MaxLength = self.MaxLength,
            IsElastic = self.IsElastic,
            LinkMaxDistance = self.LinkMaxDistance,
            VerletGravity = self.VerletGravity,
            VerletDamping = self.VerletDamping,
            ConstraintIterations = self.ConstraintIterations,
            SubSteps = self.SubSteps,
            GroundClamp = self.GroundClamp,
            GroundClampOffset = self.GroundClampOffset,
            WallClamp = self.WallClamp,
            WallClampInterval = self.WallClampInterval,
            WallClampRadius = self.WallClampRadius,
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
        local activeN = self.controller.activeN

        self.linkHandler:ApplyPositions(positions, activeN)

        local maxStep = (self.RotationMaxStepRadians or (self.RotationMaxStep and math.rad(self.RotationMaxStep))) or math.rad(60)
        self.linkHandler:ApplyRotations(positions, startPos, endPos, maxStep, true, activeN)

        local public = self.controller:GetPublicState()
        self.m_CurrentLength = public.ChainLength
        self.m_IsExtending = public.IsExtending
        self.m_IsRetracting = public.IsRetracting
        self.m_LinkCount = public.LinkCount
        self.m_ActiveLinkCount = public.ActiveLinkCount

        if not self._endpointTransform and self.ChainEndpointName and self.ChainEndpointName ~= "" then
            self._endpointTransform = Engine.FindTransformByName(self.ChainEndpointName)
        end

        if self._endpointTransform then
            local chainIsActive = (self.m_CurrentLength or 0) > 1e-4 or self.m_IsExtending
            if chainIsActive then
                self:_write_world_pos(self._endpointTransform, endPos[1], endPos[2], endPos[3])

                local fwd = self.controller.lastForward
                local fx, fy, fz = fwd[1] or 0, fwd[2] or 0, fwd[3] or 1

                local ux, uy, uz = 0, 1, 0
                local dot = ux*fx + uy*fy + uz*fz
                local rx = uy*fz - uz*fy
                local ry = uz*fx - ux*fz
                local rz = ux*fy - uy*fx
                local axisLen = math.sqrt(rx*rx + ry*ry + rz*rz)

                local qw, qx, qy, qz
                if axisLen < 1e-6 then
                    if dot > 0 then
                        qw, qx, qy, qz = 1, 0, 0, 0
                    else
                        qw, qx, qy, qz = 0, 1, 0, 0
                    end
                else
                    rx, ry, rz = rx/axisLen, ry/axisLen, rz/axisLen
                    local angle = math.acos(math.max(-1, math.min(1, dot)))
                    local halfAngle = angle * 0.5
                    local s = math.sin(halfAngle)
                    qw = math.cos(halfAngle)
                    qx = rx * s
                    qy = ry * s
                    qz = rz * s
                end

                local qlen = math.sqrt(qw*qw + qx*qx + qy*qy + qz*qz)
                if qlen > 1e-12 then
                    qw, qx, qy, qz = qw/qlen, qx/qlen, qy/qlen, qz/qlen
                else
                    qw, qx, qy, qz = 1, 0, 0, 0
                end

                pcall(function()
                    local rot = self._endpointTransform.localRotation
                    if rot and (type(rot) == "table" or type(rot) == "userdata") then
                        rot.w, rot.x, rot.y, rot.z = qw, qx, qy, qz
                        self._endpointTransform.isDirty = true
                    end
                end)

                if _G.event_bus and _G.event_bus.publish then
                    _G.event_bus.publish("chain.endpoint_moved", {
                        position = { x = endPos[1], y = endPos[2], z = endPos[3] },
                        isLocked = public.EndPointLocked,
                        chainLength = self.m_CurrentLength,
                        isExtending = self.m_IsExtending,
                        isRetracting = self.m_IsRetracting,
                    })
                end

                self._wasChainActive = true
            else
                local sp = self.controller.startPos
                self:_write_world_pos(self._endpointTransform, sp[1], sp[2], sp[3])

                if self._wasChainActive then
                    self._wasChainActive = false
                    if _G.event_bus and _G.event_bus.publish then
                        _G.event_bus.publish("chain.endpoint_retracted", {
                            position = { x = sp[1], y = sp[2], z = sp[3] },
                        })
                    end
                end
            end
        end
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._cameraForwardSub then pcall(function() _G.event_bus.unsubscribe(self._cameraForwardSub) end) end
            if self._chainSubDown     then pcall(function() _G.event_bus.unsubscribe(self._chainSubDown)     end) end
            if self._chainSubUp       then pcall(function() _G.event_bus.unsubscribe(self._chainSubUp)       end) end
            if self._chainSubHold     then pcall(function() _G.event_bus.unsubscribe(self._chainSubHold)     end) end
            if self._subPlayerForward then pcall(function() _G.event_bus.unsubscribe(self._subPlayerForward) end) end
            if self._subHookedPos     then pcall(function() _G.event_bus.unsubscribe(self._subHookedPos)     end) end
            if self._subHitEntity     then pcall(function() _G.event_bus.unsubscribe(self._subHitEntity)     end) end
        end
    end,

    StartExtension = function(self)
        if self.controller then
            self.controller:StartExtension(self._cameraForward, self.MaxLength, self.LinkMaxDistance)
        end
    end,
    StartRetraction = function(self) if self.controller then self.controller:StartRetraction() end end,
    StopExtension   = function(self) if self.controller then self.controller:StopExtension()   end end,
    GetChainState   = function(self) return { Length = self.m_CurrentLength, Count = self.m_LinkCount, ActiveCount = self.m_ActiveLinkCount } end
}