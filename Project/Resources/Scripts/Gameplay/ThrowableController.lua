-- ThrowableController.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        Mass = 1.0,
        ThrowForce = 10000.0,
        ThrowRampTime = 0.4,
        ThrowLiftBias = 0.35,
        SpringConstant = 1200.0,  -- N/m. how stiff the chain feels. raise if barely moves, lower if rushes
        SpringDamping  = 8.0,    -- kills bounce. raise if obj oscillates back toward you
        PlayerName = "Kusane_Player_LeftHandMiddle1",
        RestCheckDelay = 1.5,
        RestVelocityThreshold = 0.25,
    },

    Start = function(self)
        print("[TC:Start] ---- ThrowableController booting ----")

        self._rigidbody = self:GetComponent("RigidBodyComponent")
        if self._rigidbody then
            print(string.format("[TC:Start] RigidBodyComponent FOUND. motionID=%s", tostring(self._rigidbody.motionID)))
        else
            print("[TC:Start] ERROR: RigidBodyComponent NOT FOUND")
        end

        self._entityId = nil
        pcall(function() if self.GetEntityId then self._entityId = self:GetEntityId() end end)
        if not self._entityId then
            pcall(function() if self.GetEntity then self._entityId = self:GetEntity() end end)
        end
        if not self._entityId and self.entityId then self._entityId = self.entityId end

        self._rootEntityId = self._entityId
        if Engine and Engine.GetParentEntity and self._entityId then
            local current = self._entityId
            for _ = 1, 32 do
                local parentId = Engine.GetParentEntity(current)
                if not parentId or parentId < 0 then break end
                current = parentId
            end
            self._rootEntityId = current
        end

        self._transform = nil
        if self._rootEntityId and Engine and Engine.FindTransformByID then
            pcall(function() self._transform = Engine.FindTransformByID(self._rootEntityId) end)
        end

        print(string.format("[TC:Start] rootEntityId=%s transform=%s",
            tostring(self._rootEntityId), tostring(self._transform)))

        self._attached      = false
        self._isBeingThrown = false
        self._throwDir      = {0,0,1}
        self._throwTimer    = 0
        self._wasThrown     = false
        self._restTimer     = nil

        self._effectiveDist = 0
        self._chainLength   = 0
        self._pullTarget    = nil
        self._dbgTimer      = 0

        if not _G.event_bus then
            print("[TC:Start] ERROR: _G.event_bus is NIL")
            return
        end

        self._subAttached = _G.event_bus.subscribe("chain.throwable_attached", function(payload)
            if not payload or payload.entityId ~= self._rootEntityId then return end
            pcall(function() self:_onAttached() end)
        end)

        self._subThrow = _G.event_bus.subscribe("chain.throwable_throw", function(payload)
            if not payload or payload.entityId ~= self._rootEntityId then return end
            print(string.format("[TC:Event] throwable_throw dir=(%.2f,%.2f,%.2f)",
                payload.dirX or 0, payload.dirY or 0, payload.dirZ or 0))
            pcall(function() self:_onThrow(payload) end)
        end)

        self._subDetached = _G.event_bus.subscribe("chain.throwable_detached", function(payload)
            if not payload or payload.entityId ~= self._rootEntityId then return end
            pcall(function() self:_onDetached() end)
        end)

        -- chain.throwable_tension fires every frame while throwable is hooked,
        -- INCLUDING during retraction (unlike movement_constraint which dies on isRetracting).
        -- effectiveDist = arc distance player→throwable
        -- chainLength   = current chainLen (shrinks as chain retracts)
        -- stretch = effectiveDist - chainLength > 0 = chain is taut, spring fires
        self._subTension = _G.event_bus.subscribe("chain.throwable_tension", function(payload)
            if not payload or not self._attached then return end
            self._effectiveDist = payload.effectiveDist or 0
            self._chainLength   = payload.chainLength   or 0
            if payload.pullTargetX then
                self._pullTarget = {payload.pullTargetX, payload.pullTargetY, payload.pullTargetZ}
            else
                self._pullTarget = nil
            end
        end)

        print("[TC:Start] subscriptions OK. rootEntityId=" .. tostring(self._rootEntityId))
    end,

    _onAttached = function(self)
        self._attached      = true
        self._isBeingThrown = false
        self._wasThrown     = false
        self._restTimer     = nil
        self._throwTimer    = 0
        self._pullTarget    = nil
        self._effectiveDist = 0
        self._chainLength   = 0
        if self._rigidbody then
            self._rigidbody.motionID = 2
            print("[TC:_onAttached] motionID -> 2 (DYNAMIC)")
        end
    end,

    _onThrow = function(self, payload)
        if not self._rigidbody then
            print("[TC:_onThrow] ERROR: _rigidbody nil")
            return
        end

        local dx = payload.dirX or 0
        local dy = payload.dirY or 0
        local dz = payload.dirZ or 0

        local horizontal = math.sqrt(dx*dx + dz*dz)
        local liftBias = (tonumber(self.ThrowLiftBias) or 0.35) * horizontal
        dy = dy + liftBias

        local dlen = math.sqrt(dx*dx + dy*dy + dz*dz)
        if dlen > 1e-6 then dx,dy,dz = dx/dlen, dy/dlen, dz/dlen end

        self._throwDir      = {dx, dy, dz}
        self._throwTimer    = 0
        self._attached      = false
        self._isBeingThrown = true
        self._wasThrown     = true
        self._restTimer     = 0

        local lv = self._rigidbody.linearVel
        local av = self._rigidbody.angularVel
        if lv then lv.x,lv.y,lv.z = 0,0,0 end
        if av then av.x,av.y,av.z = 0,0,0 end

        print(string.format("[TC:_onThrow] throw started dir=(%.2f,%.2f,%.2f) lift=%.2f", dx, dy, dz, liftBias))
    end,

    _onDetached = function(self)
        self._attached      = false
        self._isBeingThrown = false
        self._wasThrown     = false
        self._restTimer     = nil
        self._pullTarget    = nil
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
                if type(a) == "table" then return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
                elseif type(a) == "number" then return a, b, c end
            end
        end
        if transform.localPosition then
            local p = transform.localPosition
            return p.x or p[1] or 0, p.y or p[2] or 0, p.z or p[3] or 0
        end
        return nil
    end,

    Update = function(self, dt)
        if not self._rigidbody then return end

        self._dbgTimer = (self._dbgTimer or 0) + dt
        if self._dbgTimer >= 1.0 then
            self._dbgTimer = 0
            local lv = self._rigidbody.linearVel
            local vx = lv and (lv.x or 0) or 0
            local vy = lv and (lv.y or 0) or 0
            local vz = lv and (lv.z or 0) or 0
            local stretch = math.max(0, (self._effectiveDist or 0) - (self._chainLength or 0))
            print(string.format("[TC:Update] attached=%s stretch=%.3f effDist=%.2f chainLen=%.2f | vel=(%.2f,%.2f,%.2f)",
                tostring(self._attached), stretch,
                self._effectiveDist or 0, self._chainLength or 0, vx, vy, vz))
        end

        -- ── SPRING PULL ────────────────────────────────────────────────────────
        -- stretch = effectiveDist - chainLength.
        -- During retraction: chainLength shrinks while throwable stays put →
        -- stretch grows every frame → spring fires harder the more chain has retracted.
        -- This is what makes it feel like the chain is pulling the object.
        if self._attached then
            local stretch = (self._effectiveDist or 0) - (self._chainLength or 0)
            if stretch > 0 then
                local tx, ty, tz
                if self._pullTarget then
                    tx, ty, tz = self._pullTarget[1], self._pullTarget[2], self._pullTarget[3]
                else
                    local playerTr = Engine.FindTransformByName(self.PlayerName)
                    if playerTr then tx, ty, tz = self:_readWorldPos(playerTr) end
                end

                if tx and self._transform then
                    local ox, oy, oz = self:_readWorldPos(self._transform)
                    if ox then
                        local dx = tx - ox; local dy = ty - oy; local dz = tz - oz
                        local dist = math.sqrt(dx*dx + dy*dy + dz*dz)
                        if dist > 0.01 then
                            local nx = dx/dist; local ny = dy/dist; local nz = dz/dist
                            local mass = math.max(tonumber(self.Mass) or 1.0, 0.01)
                            local k = (tonumber(self.SpringConstant) or 120.0) / mass
                            local d = (tonumber(self.SpringDamping)  or 8.0)   / mass

                            local springF = k * stretch

                            local lv = self._rigidbody.linearVel
                            local vx = lv and (lv.x or 0) or 0
                            local vy = lv and (lv.y or 0) or 0
                            local vz = lv and (lv.z or 0) or 0
                            local velAlongPull = vx*nx + vy*ny + vz*nz
                            local dampF = d * velAlongPull

                            local totalF = springF - dampF
                            if totalF > 0 then
                                pcall(function() self._rigidbody:AddImpulse(
                                    nx * totalF * dt,
                                    ny * totalF * dt,
                                    nz * totalF * dt)
                                end)
                            end
                        end
                    end
                end
            end
        end

        -- ── THROW ─────────────────────────────────────────────────────────────
        if self._isBeingThrown then
            self._throwTimer = self._throwTimer + dt
            local rampTime = math.max(tonumber(self.ThrowRampTime) or 0.4, 1e-4)
            local t = math.min(self._throwTimer / rampTime, 1.0)
            local mass  = math.max(tonumber(self.Mass) or 1.0, 0.01)
            local curF  = ((tonumber(self.ThrowForce) or 150.0) / mass) * t
            local td    = self._throwDir
            pcall(function() self._rigidbody:AddImpulse(
                (td[1] or 0) * curF * dt,
                (td[2] or 0) * curF * dt,
                (td[3] or 0) * curF * dt)
            end)
            if t >= 1.0 then
                self._isBeingThrown = false
                print("[TC:Update] throw force ramp complete")
            end
        end

        -- ── REST CHECK ────────────────────────────────────────────────────────
        if self._wasThrown and not self._isBeingThrown and self._restTimer ~= nil then
            self._restTimer = self._restTimer + dt
            if self._restTimer >= (tonumber(self.RestCheckDelay) or 1.5) then
                local vel = self._rigidbody.linearVel
                local vx, vy, vz = 0, 0, 0
                if type(vel) == "table" or type(vel) == "userdata" then
                    vx = vel.x or vel[1] or 0
                    vy = vel.y or vel[2] or 0
                    vz = vel.z or vel[3] or 0
                end
                local speed = math.sqrt(vx*vx + vy*vy + vz*vz)
                if speed < (tonumber(self.RestVelocityThreshold) or 0.25) then
                    self._wasThrown = false
                    self._restTimer = nil
                    local lv = self._rigidbody.linearVel; if lv then lv.x,lv.y,lv.z = 0,0,0 end
                    local av = self._rigidbody.angularVel; if av then av.x,av.y,av.z = 0,0,0 end
                    print(string.format("[TC:Update] object at rest (speed=%.4f)", speed))
                end
            end
        end
    end,

    OnDisable = function(self)
        self._attached      = false
        self._isBeingThrown = false
        self._wasThrown     = false
        self._restTimer     = nil
        self._pullTarget    = nil
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._subAttached then pcall(function() _G.event_bus.unsubscribe(self._subAttached) end) end
            if self._subThrow    then pcall(function() _G.event_bus.unsubscribe(self._subThrow)    end) end
            if self._subDetached then pcall(function() _G.event_bus.unsubscribe(self._subDetached) end) end
            if self._subTension  then pcall(function() _G.event_bus.unsubscribe(self._subTension)  end) end
        end
    end,
}