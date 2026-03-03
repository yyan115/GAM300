-- ChainEndpointController.lua
local Component = require("extension.mono_helper")

return Component {
    fields = {
        PlayerName = "Player",
        HookSnapDistance = 0.0,  -- max offset from hooked entity pivot; 0 = snap exactly to pivot
        DebugLogs = false,       -- enable debug logs from inspector
    },

    Start = function(self)
        self._modelRender = self:GetComponent("ModelRenderComponent")
        if not self._modelRender then
            print("[ChainEndpointController] WARNING: ModelRenderComponent not found on endpoint object")
        end

        self._rb = self:GetComponent("RigidBodyComponent")
        if self._rb then
            self._rb:SetEnabled(false)
        else
            print("[ChainEndpointController] WARNING: RigidBodyComponent not found on endpoint object")
        end

        self._playerEntityId = nil
        if Engine and Engine.GetEntityByName then
            self._playerEntityId = Engine.GetEntityByName(self.PlayerName)
        else
            print("[ChainEndpointController] WARNING: Engine.GetEntityByName not available")
        end

        -- Resolve this component's entity id for parenting calls. Keep heuristics minimal.
        self._entityId = nil
        do
            local ok, eid = pcall(function() if self.GetEntityId then return self:GetEntityId() end end)
            if ok and eid then self._entityId = eid end
        end
        if not self._entityId then
            local ok, eid = pcall(function() if self.GetEntity then return self:GetEntity() end end)
            if ok and eid then self._entityId = eid end
        end
        if not self._entityId and self.entityId then self._entityId = self.entityId end
        if not self._entityId and self.entity and type(self.entity) == "number" then self._entityId = self.entity end
        if not self._entityId and self.gameObject and self.gameObject.EntityId then self._entityId = self.gameObject.EntityId end

        -- Try resolve endpoint transform handle once (non-fatal)
        self._endpointTransform = nil
        if self._entityId and Engine then
            pcall(function()
                if Engine.FindTransformByEntity then
                    self._endpointTransform = Engine.FindTransformByEntity(self._entityId)
                elseif Engine.GetTransformForEntity then
                    self._endpointTransform = Engine.GetTransformForEntity(self._entityId)
                elseif Engine.FindTransformByName and Engine.GetEntityName then
                    local name = Engine.GetEntityName(self._entityId)
                    if name and name ~= "" then
                        self._endpointTransform = Engine.FindTransformByName(name)
                    end
                end
            end)
        end

        -- Track if we successfully parented so we can avoid manual transform updates
        self._isParented = false

        self._isVisible       = false
        self._isExtending     = false
        self._hookedEntityId  = nil
        self._hookedTransform = nil
        self._hookedColliderTransform = nil
        self._hookedOffsetX   = 0
        self._hookedOffsetY   = 0
        self._hookedOffsetZ   = 0

        self:_setModelVisible(false)

        if _G.event_bus and _G.event_bus.subscribe then
            self._subMoved = _G.event_bus.subscribe("chain.endpoint_moved", function(payload)
                if not payload then return end
                pcall(function() self:_onEndpointMoved(payload) end)
            end)
            self._subRetracted = _G.event_bus.subscribe("chain.endpoint_retracted", function(payload)
                if not payload then return end
                pcall(function() self:_onEndpointRetracted(payload) end)
            end)
            self._subAttach = _G.event_bus.subscribe("chain.endpoint_attach", function(payload)
                if not payload then return end
                pcall(function() self:_onAttach(payload) end)
            end)
            self._subDetach = _G.event_bus.subscribe("chain.endpoint_detach", function(payload)
                if not payload then return end
                pcall(function() self:_onDetach(payload) end)
            end)
            self._subCheckCollision = _G.event_bus.subscribe("chain.endpoint_check_collision", function(payload)
                if not payload then return end
                pcall(function() self:_onCheckCollision(payload) end)
            end)
        else
            print("[ChainEndpointController] WARNING: event_bus not available")
        end
    end,

    -- Single helper for debug printing
    _dbg = function(self, msg)
        if self.DebugLogs then
            print(msg)
        end
    end,

    _getEntityDebugName = function(self, entityId)
        if not entityId then return "nil" end
        if Engine and Engine.GetEntityName then
            local ok, name = pcall(function() return Engine.GetEntityName(entityId) end)
            if ok and name and name ~= "" then return name end
        end
        return "id=" .. tostring(entityId)
    end,

    _getRootEntityId = function(self, entityId)
        local targetId = entityId
        if Engine and Engine.GetParentEntity then
            local depth = 0
            while true do
                depth = depth + 1
                if depth > 32 then
                    self:_dbg("[ChainEndpointController] WARNING: _getRootEntityId hit depth limit at '" .. self:_getEntityDebugName(targetId) .. "'")
                    break
                end
                local parentId = Engine.GetParentEntity(targetId)
                if not parentId or parentId < 0 then break end
                targetId = parentId
            end
        end
        return targetId
    end,

    _clearHook = function(self)
        self:_dbg("[ChainEndpointController] _clearHook — releasing '" .. self:_getEntityDebugName(self._hookedEntityId) .. "'")

        if self._isParented and self._entityId and Engine and Engine.SetParentEntity then
            pcall(function()
                Engine.SetParentEntity(self._entityId, -1)
            end)
            self:_dbg("[ChainEndpointController] Unparented endpoint (on clear)")
        end
        self._isParented = false

        self._hookedEntityId          = nil
        self._hookedTransform         = nil
        self._hookedColliderTransform = nil
        self._hookedOffsetX           = 0
        self._hookedOffsetY           = 0
        self._hookedOffsetZ           = 0
    end,

    _setModelVisible = function(self, visible)
        if not self._modelRender then return end
        pcall(function() ModelRenderComponent.SetVisible(self._modelRender, visible) end)
        self._isVisible = visible
        self:_dbg("[ChainEndpointController] Model visible=" .. tostring(visible))
    end,

    -- Chain state events
    _onEndpointMoved = function(self, payload)
        if not self._isVisible then
            self:_setModelVisible(true)
        end

        if payload.position then
            self._lastEndpointX = payload.position.x
            self._lastEndpointY = payload.position.y
            self._lastEndpointZ = payload.position.z
        end

        local extending = payload.isExtending or false
        if extending and not self._isExtending then
            self._isExtending = true
            self._rbKeepAliveFrames = 0
            if self._rb then self._rb:SetEnabled(true) end
            self:_dbg("[ChainEndpointController] Extending — trigger enabled")
        elseif not extending and self._isExtending then
            self._isExtending = false
            self._rbKeepAliveFrames = 3
            self:_dbg("[ChainEndpointController] No longer extending — trigger window open (3 frames)")
        end

        if not self._isExtending and self._rbKeepAliveFrames and self._rbKeepAliveFrames > 0 then
            self._rbKeepAliveFrames = self._rbKeepAliveFrames - 1
            if self._rbKeepAliveFrames == 0 and not self._hookedEntityId then
                if self._rb then self._rb:SetEnabled(false) end
                self:_dbg("[ChainEndpointController] Trigger window closed — RB disabled")
            end
        end

        if self._hookedEntityId then
            local hookedName = self:_getEntityDebugName(self._hookedEntityId)
            local px, py, pz = nil, nil, nil

            -- Prefer querying endpoint's own world position when parented and transform resolved
            if self._isParented and self._endpointTransform and Engine and Engine.GetTransformWorldPosition then
                local ok, a, b, c = pcall(function()
                    return Engine.GetTransformWorldPosition(self._endpointTransform)
                end)
                if ok and a ~= nil then
                    if type(a) == "table" then
                        px = a[1] or a.x or 0
                        py = a[2] or a.y or 0
                        pz = a[3] or a.z or 0
                    elseif type(a) == "number" then
                        px, py, pz = a, b, c
                    end
                    self:_dbg(string.format("[ChainEndpointController] TRACK (parented) '%s' endpoint world pivot=(%.3f,%.3f,%.3f)", hookedName, px, py, pz))
                else
                    px = nil
                end
            end

            -- Fallback: original root-pivot + offset calculation
            if not px then
                if self._hookedTransform then
                    local ok, a, b, c = pcall(function()
                        return Engine.GetTransformWorldPosition(self._hookedTransform)
                    end)
                    if ok and a ~= nil then
                        if type(a) == "table" then
                            px = a[1] or a.x or 0
                            py = a[2] or a.y or 0
                            pz = a[3] or a.z or 0
                        elseif type(a) == "number" then
                            px, py, pz = a, b, c
                        end
                        self:_dbg(string.format("[ChainEndpointController] TRACK '%s' root pivot=(%.3f,%.3f,%.3f)", hookedName, px, py, pz))
                    else
                        self:_dbg("[ChainEndpointController] WARNING: GetTransformWorldPosition failed for '" .. hookedName .. "'")
                    end
                else
                    self:_dbg("[ChainEndpointController] WARNING: no root transform for '" .. hookedName .. "'")
                end
            end

            if px then
                local ox = self._hookedOffsetX or 0
                local oy = self._hookedOffsetY or 0
                local oz = self._hookedOffsetZ or 0
                local snapDist = tonumber(self.HookSnapDistance) or 0
                local offLen = math.sqrt(ox*ox + oy*oy + oz*oz)
                if offLen > snapDist then
                    if offLen < 1e-9 then
                        ox, oy, oz = 0, 0, 0
                    else
                        local scale = snapDist / offLen
                        ox = ox * scale
                        oy = oy * scale
                        oz = oz * scale
                    end
                    self:_dbg(string.format("[ChainEndpointController] Offset clamped: snapDist=%.3f offLen=%.3f -> new offset=(%.3f,%.3f,%.3f)",
                        snapDist, offLen, ox, oy, oz))
                end

                local wx = px + ox
                local wy = py + oy
                local wz = pz + oz

                self:_dbg(string.format("[ChainEndpointController] HOOK '%s' world pos=(%.3f,%.3f,%.3f) final offset=(%.3f,%.3f,%.3f)",
                    hookedName, wx, wy, wz, ox, oy, oz))

                if _G.event_bus and _G.event_bus.publish then
                    _G.event_bus.publish("chain.endpoint_hooked_position", {
                        x = wx, y = wy, z = wz,
                        entityId = self._hookedEntityId,
                    })
                end

                if self._hookedColliderTransform then
                    if _G.event_bus and _G.event_bus.publish then
                        _G.event_bus.publish("chain.endpoint_hooked_rotation", {
                            transform = self._hookedColliderTransform,
                        })
                    end
                end
            else
                self:_dbg("[ChainEndpointController] ERROR: Could not resolve position for '" .. hookedName .. "'")
            end
        end
    end,

    _onEndpointRetracted = function(self, payload)
        if self._isVisible then
            self:_setModelVisible(false)
        end
        self:_dbg("[ChainEndpointController] Endpoint retracted — clearing hook state")

        if self._hookedEntityId then
            local hookedName = self:_getEntityDebugName(self._hookedEntityId)
            local rootId = self:_getRootEntityId(self._hookedEntityId)
            local rootName = self:_getEntityDebugName(rootId)
            self:_dbg("[ChainEndpointController] Was hooked to '" .. hookedName .. "' (root='" .. rootName .. "') on retract — publishing chain.enemy_hooked")
            if _G.event_bus and _G.event_bus.publish then
                _G.event_bus.publish("chain.enemy_hooked", {
                    entityId = rootId,
                    duration = 2.0, --4.0 equivalent to humping
                })
            end
        end

        self._isExtending = false
        self:_clearHook()
        if self._rb then self._rb:SetEnabled(false) end
    end,

    -- Trigger
    OnTriggerEnter = function(self, otherEntityId)
        local otherName = self:_getEntityDebugName(otherEntityId)

        local rbActive = self._rb and self._rb:IsEnabled()
        if not rbActive then
            self:_dbg("[ChainEndpointController] OnTriggerEnter ignored — RB not active | entity='" .. otherName .. "'")
            return
        end
        if self._hookedEntityId then
            self:_dbg("[ChainEndpointController] OnTriggerEnter ignored — already hooked to '" .. self:_getEntityDebugName(self._hookedEntityId) .. "' | new contact='" .. otherName .. "'")
            return
        end

        if self._playerEntityId and otherEntityId == self._playerEntityId then
            self:_dbg("[ChainEndpointController] OnTriggerEnter ignored — hit player '" .. otherName .. "'")
            return
        end

        local rootId = self:_getRootEntityId(otherEntityId)
        local rootName = self:_getEntityDebugName(rootId)
        local tag = nil
        if Engine and Engine.GetEntityTag then
            local ok, t = pcall(function() return Engine.GetEntityTag(rootId) end)
            if ok then tag = t else
                self:_dbg("[ChainEndpointController] WARNING: GetEntityTag failed for root '" .. rootName .. "'")
            end
        end

        self:_dbg("[ChainEndpointController] OnTriggerEnter entity='" .. otherName .. "' root='" .. rootName .. "' tag='" .. tostring(tag) .. "'")

        local isHookable = (tag == "Enemy")
        if not isHookable then
            self:_dbg("[ChainEndpointController] OnTriggerEnter ignored — root='" .. rootName .. "' tag='" .. tostring(tag) .. "' not hookable")
            return
        end

        self:_dbg("[ChainEndpointController] OnTriggerEnter HOOKING — entity='" .. otherName .. "' root='" .. rootName .. "'")

        self._hookedEntityId = otherEntityId
        self._isExtending = false
        if self._rb then self._rb:SetEnabled(false) end

        self._hookedTransform = Engine.FindTransformByName(rootName)
        if self._hookedTransform then
            self:_dbg("[ChainEndpointController] Root transform cached: '" .. rootName .. "'")
        else
            self:_dbg("[ChainEndpointController] WARNING: FindTransformByName failed for root '" .. rootName .. "'")
        end

        self._hookedColliderTransform = Engine.FindTransformByName(otherName)
        if self._hookedColliderTransform then
            self:_dbg("[ChainEndpointController] Collider transform cached for rotation: '" .. otherName .. "'")
        else
            self:_dbg("[ChainEndpointController] WARNING: FindTransformByName failed for collider '" .. otherName .. "' — rotation fallback to root")
            self._hookedColliderTransform = self._hookedTransform
        end

        local impactX = self._lastEndpointX or 0
        local impactY = self._lastEndpointY or 0
        local impactZ = self._lastEndpointZ or 0

        local rootPivotX, rootPivotY, rootPivotZ = 0, 0, 0
        if self._hookedTransform then
            local ok, a, b, c = pcall(function()
                return Engine.GetTransformWorldPosition(self._hookedTransform)
            end)
            if ok and a ~= nil then
                if type(a) == "table" then
                    rootPivotX = a[1] or a.x or 0
                    rootPivotY = a[2] or a.y or 0
                    rootPivotZ = a[3] or a.z or 0
                elseif type(a) == "number" then
                    rootPivotX, rootPivotY, rootPivotZ = a, b, c
                end
                self:_dbg(string.format("[ChainEndpointController] Root pivot='%s' (%.3f,%.3f,%.3f)", rootName, rootPivotX, rootPivotY, rootPivotZ))
            else
                self:_dbg("[ChainEndpointController] WARNING: GetTransformWorldPosition failed for root '" .. rootName .. "'")
            end
        end

        local ox = impactX - rootPivotX
        local oy = impactY - rootPivotY
        local oz = impactZ - rootPivotZ

        local snapDist = tonumber(self.HookSnapDistance) or 0
        local offLen = math.sqrt(ox*ox + oy*oy + oz*oz)
        if offLen > snapDist then
            if offLen < 1e-9 then
                ox, oy, oz = 0, 0, 0
            else
                local scale = snapDist / offLen
                ox = ox * scale
                oy = oy * scale
                oz = oz * scale
            end
            self:_dbg(string.format("[ChainEndpointController] Initial offset clamped: snapDist=%.3f offLen=%.3f -> (%.3f,%.3f,%.3f)",
                snapDist, offLen, ox, oy, oz))
        end

        self._hookedOffsetX = ox
        self._hookedOffsetY = oy
        self._hookedOffsetZ = oz

        self:_dbg(string.format("[ChainEndpointController] Hook locked on '%s' (root='%s') impact=(%.3f,%.3f,%.3f) rootPivot=(%.3f,%.3f,%.3f) offset=(%.3f,%.3f,%.3f)",
            otherName, rootName,
            impactX, impactY, impactZ,
            rootPivotX, rootPivotY, rootPivotZ,
            self._hookedOffsetX, self._hookedOffsetY, self._hookedOffsetZ))

        -- try to parent endpoint under the root so engine maintains transform automatically
        if self._entityId and Engine and Engine.SetParentEntity then
            local ok = pcall(function()
                Engine.SetParentEntity(self._entityId, rootId)
            end)
            if ok then
                self._isParented = true
                self:_dbg("[ChainEndpointController] Engine.SetParentEntity succeeded: endpoint -> " .. tostring(rootId))
            else
                self._isParented = false
                self:_dbg("[ChainEndpointController] WARNING: Engine.SetParentEntity failed for endpoint -> " .. tostring(rootId))
            end
        else
            self:_dbg("[ChainEndpointController] Note: endpoint entity id unavailable or Engine.SetParentEntity not present; skipping parent call")
        end

        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("chain.endpoint_hit_entity", {
                entityId   = otherEntityId,
                entityName = otherName,
                rootName   = rootName,
            })
        end
    end,

    _onAttach = function(self, payload)
        self:_dbg("[ChainEndpointController] _onAttach called")
    end,

    _onDetach = function(self, payload)
        self:_dbg("[ChainEndpointController] _onDetach called")
        if self._isParented and self._entityId and Engine and Engine.SetParentEntity then
            pcall(function() Engine.SetParentEntity(self._entityId, -1) end)
            self:_dbg("[ChainEndpointController] _onDetach: unparented endpoint (if parented)")
        end
        self._isParented = false
    end,

    _onCheckCollision = function(self, payload)
        self:_dbg("[ChainEndpointController] _onCheckCollision called")
    end,

    OnDisable = function(self)
        self._isExtending = false
        self:_clearHook()
        if self._rb then self._rb:SetEnabled(false) end
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._subMoved          then pcall(function() _G.event_bus.unsubscribe(self._subMoved)          end) end
            if self._subRetracted      then pcall(function() _G.event_bus.unsubscribe(self._subRetracted)      end) end
            if self._subAttach         then pcall(function() _G.event_bus.unsubscribe(self._subAttach)         end) end
            if self._subDetach         then pcall(function() _G.event_bus.unsubscribe(self._subDetach)         end) end
            if self._subCheckCollision then pcall(function() _G.event_bus.unsubscribe(self._subCheckCollision) end) end
        end
    end,
}
