-- ThrowableController.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        Mass = 1.0,
        ThrowForce = 15.0,
        PullForce = 8.0,
        TautThreshold = 0.2,
        PlayerName = "Kusane_Player_LeftHandMiddle1",
        RestCheckDelay = 1.5,
        RestVelocityThreshold = 0.25,
    },

    Start = function(self)
        print("[TC:Start] ---- ThrowableController booting ----")

        -- Grab RigidBody
        self._rigidbody = self:GetComponent("RigidBodyComponent")
        if self._rigidbody then
            print(string.format("[TC:Start] RigidBodyComponent FOUND. motionID=%s", tostring(self._rigidbody.motionID)))
        else
            print("[TC:Start] ERROR: RigidBodyComponent NOT FOUND — nothing will move")
        end

        -- Resolve entity ID
        self._entityId = nil
        pcall(function() if self.GetEntityId then self._entityId = self:GetEntityId() end end)
        if not self._entityId then
            pcall(function() if self.GetEntity then self._entityId = self:GetEntity() end end)
        end
        if not self._entityId and self.entityId then self._entityId = self.entityId end
        print(string.format("[TC:Start] entityId=%s", tostring(self._entityId)))

        -- Walk to root
        self._rootEntityId = self._entityId
        if Engine and Engine.GetParentEntity and self._entityId then
            local current = self._entityId
            local depth   = 0
            while true do
                depth = depth + 1
                if depth > 32 then break end
                local parentId = Engine.GetParentEntity(current)
                if not parentId or parentId < 0 then break end
                current = parentId
            end
            self._rootEntityId = current
        end
        print(string.format("[TC:Start] rootEntityId=%s", tostring(self._rootEntityId)))

        -- Cache transform
        self._transform = nil
        if self._rootEntityId and Engine and Engine.FindTransformByID then
            pcall(function() self._transform = Engine.FindTransformByID(self._rootEntityId) end)
        end
        print(string.format("[TC:Start] transform=%s", tostring(self._transform)))

        -- State
        self._attached    = false
        self._wasThrown   = false
        self._restTimer   = nil
        self._taut        = false
        self._playerPos   = { 0, 0, 0 }
        self._dbgTimer    = 0   -- throttle for per-frame logs

        -- Check event_bus
        if not _G.event_bus then
            print("[TC:Start] ERROR: _G.event_bus is NIL — no events will be received!")
            return
        end
        if not _G.event_bus.subscribe then
            print("[TC:Start] ERROR: event_bus has no subscribe function!")
            return
        end
        print("[TC:Start] event_bus OK — subscribing...")

        self._subAttached = _G.event_bus.subscribe("chain.throwable_attached", function(payload)
            if not payload then
                print("[TC:Event] throwable_attached received but payload is NIL")
                return
            end
            print(string.format("[TC:Event] throwable_attached: payload.entityId=%s | my rootEntityId=%s | match=%s",
                tostring(payload.entityId), tostring(self._rootEntityId), tostring(payload.entityId == self._rootEntityId)))
            if payload.entityId ~= self._rootEntityId then return end
            pcall(function() self:_onAttached() end)
        end)

        self._subThrow = _G.event_bus.subscribe("chain.throwable_throw", function(payload)
            if not payload then
                print("[TC:Event] throwable_throw received but payload is NIL")
                return
            end
            print(string.format("[TC:Event] throwable_throw: payload.entityId=%s | my rootEntityId=%s | match=%s | dir=(%.2f,%.2f,%.2f)",
                tostring(payload.entityId), tostring(self._rootEntityId), tostring(payload.entityId == self._rootEntityId),
                payload.dirX or 0, payload.dirY or 0, payload.dirZ or 0))
            if payload.entityId ~= self._rootEntityId then return end
            pcall(function() self:_onThrow(payload) end)
        end)

        self._subSwing = _G.event_bus.subscribe("chain.throwable_swing", function(payload)
            if not payload then return end
            print(string.format("[TC:Event] throwable_swing: payload.entityId=%s | my rootEntityId=%s | match=%s",
                tostring(payload.entityId), tostring(self._rootEntityId), tostring(payload.entityId == self._rootEntityId)))
        end)

        self._subDetached = _G.event_bus.subscribe("chain.throwable_detached", function(payload)
            if not payload then return end
            print(string.format("[TC:Event] throwable_detached: payload.entityId=%s | my rootEntityId=%s | match=%s",
                tostring(payload.entityId), tostring(self._rootEntityId), tostring(payload.entityId == self._rootEntityId)))
            if payload.entityId ~= self._rootEntityId then return end
            pcall(function() self:_onDetached() end)
        end)

        self._subConstraint = _G.event_bus.subscribe("chain.movement_constraint", function(payload)
            if not payload then return end
            local threshold = tonumber(self.TautThreshold) or 0.2
            local wasTaut = self._taut
            self._taut = ((payload.ratio or 0) > threshold)
            if self._taut ~= wasTaut then
                print(string.format("[TC:Event] chain tension changed: taut=%s (ratio=%.3f threshold=%.3f)",
                    tostring(self._taut), payload.ratio or 0, threshold))
            end
        end)

        print("[TC:Start] All subscriptions registered. rootEntityId=" .. tostring(self._rootEntityId))
    end,

    _onAttached = function(self)
        self._attached  = true
        self._wasThrown = false
        self._restTimer = nil
        self._taut      = false
        if self._rigidbody then
            local prevMotion = self._rigidbody.motionID
            self._rigidbody.motionID = 2
            print(string.format("[TC:_onAttached] motionID: %s -> 2 (DYNAMIC)", tostring(prevMotion)))
        else
            print("[TC:_onAttached] ERROR: _rb is nil, cannot go dynamic!")
        end
    end,

    _onThrow = function(self, payload)
        self._attached  = false
        self._wasThrown = true
        self._restTimer = 0
        self._taut      = false

        if not self._rigidbody then
            print("[TC:_onThrow] ERROR: _rb is nil, cannot apply impulse!")
            return
        end

        local throwForce = tonumber(self.ThrowForce) or 15.0
        local mass       = math.max(tonumber(self.Mass) or 1.0, 0.01)
        local impulse    = throwForce / mass
        local ix = (payload.dirX or 0) * impulse
        local iy = (payload.dirY or 0) * impulse
        local iz = (payload.dirZ or 0) * impulse

        print(string.format("[TC:_onThrow] motionID=%s | impulse=(%.2f,%.2f,%.2f) | force=%.1f mass=%.2f",
            tostring(self._rigidbody.motionID), ix, iy, iz, throwForce, mass))

        -- Zero velocity
        local lv = self._rigidbody.linearVel
        local av = self._rigidbody.angularVel
        print(string.format("[TC:_onThrow] linearVel type=%s | angularVel type=%s", type(lv), type(av)))
        if lv then lv.x,lv.y,lv.z = 0,0,0 end
        if av then av.x,av.y,av.z = 0,0,0 end

        -- Apply impulse
        local ok, err = pcall(function() self._rigidbody:AddImpulse(ix, iy, iz) end)
        if ok then
            print(string.format("[TC:_onThrow] AddImpulse OK"))
        else
            print(string.format("[TC:_onThrow] AddImpulse FAILED: %s", tostring(err)))
        end
    end,

    _onDetached = function(self)
        self._attached  = false
        self._wasThrown = false
        self._restTimer = nil
        self._taut      = false
        if self._rigidbody then
            local lv = self._rigidbody.linearVel; if lv then lv.x,lv.y,lv.z = 0,0,0 end
            local av = self._rigidbody.angularVel; if av then av.x,av.y,av.z = 0,0,0 end
        end
        print(string.format("[TC:_onDetached] id=%s", tostring(self._rootEntityId)))
    end,

    _readWorldPos = function(self, transform)
        if not transform then return nil end
        if Engine and Engine.GetTransformWorldPosition then
            local ok, a, b, c = pcall(function() return Engine.GetTransformWorldPosition(transform) end)
            if ok and a ~= nil then
                if type(a) == "table" then
                    return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
                elseif type(a) == "number" then
                    return a, b, c
                end
            end
        end
        if transform.localPosition then
            local p = transform.localPosition
            return p.x or p[1] or 0, p.y or p[2] or 0, p.z or p[3] or 0
        end
        return nil
    end,

    Update = function(self, dt)

        -- Throttled state dump every 2 seconds so console doesn't flood
        self._dbgTimer = (self._dbgTimer or 0) + dt
        if self._dbgTimer >= 2.0 then
            self._dbgTimer = 0
            if self._rigidbody then
                local lv = self._rigidbody.linearVel
                local vx = lv and (lv.x or 0) or 0
                local vy = lv and (lv.y or 0) or 0
                local vz = lv and (lv.z or 0) or 0
                print(string.format("[TC:Update] attached=%s wasThrown=%s taut=%s | motionID=%s | vel=(%.3f,%.3f,%.3f)",
                    tostring(self._attached), tostring(self._wasThrown), tostring(self._taut),
                    tostring(self._rigidbody.motionID), vx, vy, vz))
            else
                print(string.format("[TC:Update] attached=%s wasThrown=%s | _rb=NIL",
                    tostring(self._attached), tostring(self._wasThrown)))
            end
        end

        -- Pull force while attached and taut
        if not self._attached then
            -- no log, fires every frame
        elseif not self._rigidbody then
            print("[TC:Pull] BLOCKED: _rigidbody is nil")
        elseif not self._taut then
            print(string.format("[TC:Pull] BLOCKED: taut=false (chain not taut enough) ratio needs > %.2f", tonumber(self.TautThreshold) or 0.2))
        else
            local playerTr = Engine.FindTransformByName(self.PlayerName)
            if not playerTr then
                print("[TC:Pull] BLOCKED: player transform not found, PlayerName=" .. tostring(self.PlayerName))
            else
                local px, py, pz = self:_readWorldPos(playerTr)
                if not px then
                    print("[TC:Pull] BLOCKED: could not read player world position")
                else
                    self._playerPos = { px, py, pz }
                    if not self._transform then
                        print("[TC:Pull] BLOCKED: self._transform is nil (own transform not cached)")
                    else
                        local tx, ty, tz = self:_readWorldPos(self._transform)
                        if not tx then
                            print("[TC:Pull] BLOCKED: could not read own world position")
                        else
                            local dx = px - tx; local dy = py - ty; local dz = pz - tz
                            local dist = math.sqrt(dx*dx + dy*dy + dz*dz)
                            if dist <= 0.05 then
                                print(string.format("[TC:Pull] BLOCKED: dist=%.4f too small (<= 0.05)", dist))
                            else
                                local pullF = tonumber(self.PullForce) or 8.0
                                local mass  = math.max(tonumber(self.Mass) or 1.0, 0.01)
                                local scale = pullF / mass
                                local fx = (dx / dist) * scale
                                local fy = (dy / dist) * scale
                                local fz = (dz / dist) * scale
                                print(string.format("[TC:Pull] FIRING impulse=(%.2f,%.2f,%.2f) dist=%.2f", fx, fy, fz, dist))
                                local ok, err = pcall(function() self._rigidbody:AddImpulse(fx, fy, fz) end)
                                if not ok then
                                    print(string.format("[TC:Pull] AddImpulse FAILED: %s", tostring(err)))
                                end
                            end
                        end
                    end
                end
            end
        end

        -- Rest check
        if self._wasThrown and self._restTimer ~= nil then
            self._restTimer = self._restTimer + dt
            local restDelay = tonumber(self.RestCheckDelay) or 1.5
            if self._restTimer >= restDelay and self._rigidbody then
                local vel = self._rigidbody.linearVel
                local vx, vy, vz = 0, 0, 0
                if type(vel) == "table" or type(vel) == "userdata" then
                    vx = vel.x or vel[1] or 0
                    vy = vel.y or vel[2] or 0
                    vz = vel.z or vel[3] or 0
                end
                local speed = math.sqrt(vx*vx + vy*vy + vz*vz)
                local threshold = tonumber(self.RestVelocityThreshold) or 0.25
                if speed < threshold then
                    self._wasThrown = false
                    self._restTimer = nil
                    local lv = self._rigidbody.linearVel; if lv then lv.x,lv.y,lv.z = 0,0,0 end
                    local av = self._rigidbody.angularVel; if av then av.x,av.y,av.z = 0,0,0 end
                    print(string.format("[TC:Update] object at rest (speed=%.4f < %.4f)", speed, threshold))
                end
            end
        end
    end,

    OnDisable = function(self)
        self._attached  = false
        self._wasThrown = false
        self._restTimer = nil
        self._taut      = false
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._subAttached   then pcall(function() _G.event_bus.unsubscribe(self._subAttached)   end) end
            if self._subThrow      then pcall(function() _G.event_bus.unsubscribe(self._subThrow)      end) end
            if self._subSwing      then pcall(function() _G.event_bus.unsubscribe(self._subSwing)      end) end
            if self._subDetached   then pcall(function() _G.event_bus.unsubscribe(self._subDetached)   end) end
            if self._subConstraint then pcall(function() _G.event_bus.unsubscribe(self._subConstraint) end) end
        end
    end,
}