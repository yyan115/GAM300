-- ChainEndpointController.lua
_G.CHAIN_DEBUG = _G.CHAIN_DEBUG ~= nil and _G.CHAIN_DEBUG or false
local function dbg(...) if _G.CHAIN_DEBUG then print(...) end end
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
            dbg("[ChainEndpointController] WARNING: ModelRenderComponent not found on endpoint object")
        end

        self._rb = self:GetComponent("RigidBodyComponent")
        if self._rb then
            self._rb:SetEnabled(false)
        else
            dbg("[ChainEndpointController] WARNING: RigidBodyComponent not found on endpoint object")
        end

        self._playerEntityId = nil
        if Engine and Engine.GetEntityByName then
            self._playerEntityId = Engine.GetEntityByName(self.PlayerName)
        else
            dbg("[ChainEndpointController] WARNING: Engine.GetEntityByName not available")
        end

        -- Resolve this component's entity id for parenting calls
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

        -- Resolve our own transform handle so we can read our world position
        -- when parented to the enemy (engine maintains it, we just read it back)
        self._endpointTransform = nil
        if self._entityId and Engine then
            pcall(function()
                if Engine.FindTransformByID then
                    self._endpointTransform = Engine.FindTransformByID(self._entityId)
                elseif Engine.FindTransformByEntity then
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
            dbg("[ChainEndpointController] WARNING: event_bus not available")
        end
    end,

    _dbg = function(self, msg)
        if self.DebugLogs or _G.CHAIN_DEBUG then
            dbg(msg)
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

        -- Unparent from enemy so Bootstrap can drive the transform freely again
        if self._isParented and self._entityId and Engine and Engine.SetParentEntity then
            pcall(function() Engine.SetParentEntity(self._entityId, -1) end)
            self:_dbg("[ChainEndpointController] Unparented endpoint")
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

    -- -------------------------------------------------------------------------
    -- Chain state events
    -- -------------------------------------------------------------------------

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

        -- When hooked and parented, read OUR OWN world position from the engine.
        -- The engine already moved us with the enemy via parenting — no manual
        -- root+offset math needed. We publish it so ChainBootstrap updates
        -- lockedEndPoint every frame, which keeps Verlet pinning positions[aN]
        -- at the correct moving world position and allows retraction to work correctly.
        if self._hookedEntityId and self._isParented then
            local wx, wy, wz = nil, nil, nil

            if self._endpointTransform and Engine and Engine.GetTransformWorldPosition then
                local ok, a, b, c = pcall(function()
                    return Engine.GetTransformWorldPosition(self._endpointTransform)
                end)
                if ok and a ~= nil then
                    if type(a) == "table" then
                        wx = a[1] or a.x or 0
                        wy = a[2] or a.y or 0
                        wz = a[3] or a.z or 0
                    elseif type(a) == "number" then
                        wx, wy, wz = a, b, c
                    end
                end
            end

            if wx then
                self:_dbg(string.format("[ChainEndpointController] HOOKED world pos=(%.3f,%.3f,%.3f)", wx, wy, wz))
                if _G.event_bus and _G.event_bus.publish then
                    _G.event_bus.publish("chain.endpoint_hooked_position", {
                        x = wx, y = wy, z = wz,
                        entityId = self._hookedEntityId,
                    })
                end
            else
                self:_dbg("[ChainEndpointController] WARNING: could not read own world position while hooked")
            end
        end
    end,

    _onEndpointRetracted = function(self, payload)
        if self._isVisible then
            self:_setModelVisible(false)
        end
        self:_dbg("[ChainEndpointController] Endpoint retracted — clearing hook state")

        -- Notify enemy AI to trigger hooked/slam behaviour
        if self._hookedEntityId then
            local hookedName = self:_getEntityDebugName(self._hookedEntityId)
            local rootId = self:_getRootEntityId(self._hookedEntityId)
            local rootName = self:_getEntityDebugName(rootId)
            self:_dbg("[ChainEndpointController] Was hooked to '" .. hookedName .. "' (root='" .. rootName .. "') on retract — publishing chain.enemy_hooked")
            if _G.event_bus and _G.event_bus.publish then
                _G.event_bus.publish("chain.enemy_hooked", {
                    entityId = rootId,
                    duration = 2.0,
                })
            end
        end

        self._isExtending = false
        self:_clearHook()
        if self._rb then self._rb:SetEnabled(false) end
    end,

    -- -------------------------------------------------------------------------
    -- Trigger
    -- -------------------------------------------------------------------------

    OnTriggerEnter = function(self, otherEntityId)
        local otherName = self:_getEntityDebugName(otherEntityId)

        dbg("[ChainEndpointController][TRIGGER] OnTriggerEnter fired entity='" .. otherName .. "' id=" .. tostring(otherEntityId))
        dbg(string.format("[ChainEndpointController][TRIGGER] rbActive=%s hookedEntityId=%s playerEntityId=%s",
            tostring(self._rb and self._rb:IsEnabled()),
            tostring(self._hookedEntityId),
            tostring(self._playerEntityId)))

        local rbActive = self._rb and self._rb:IsEnabled()
        if not rbActive then
            dbg("[ChainEndpointController][TRIGGER] REJECTED — RB not active | entity='" .. otherName .. "'")
            return
        end
        if self._hookedEntityId then
            dbg("[ChainEndpointController][TRIGGER] REJECTED — already hooked to '" .. self:_getEntityDebugName(self._hookedEntityId) .. "' | new contact='" .. otherName .. "'")
            return
        end
        if self._playerEntityId and otherEntityId == self._playerEntityId then
            dbg("[ChainEndpointController][TRIGGER] REJECTED — hit player '" .. otherName .. "'")
            return
        end

        -- Tag check via root
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
        dbg(string.format("[ChainEndpointController][TRIGGER] tag check — entity='%s' root='%s' tag='%s'", otherName, rootName, tostring(tag)))

        -- Extend here for more hookable types: (tag == "Enemy") or (tag == "Hookable")
        local isHookable = (tag == "Enemy")
        if not isHookable then
            dbg("[ChainEndpointController][TRIGGER] REJECTED — tag='" .. tostring(tag) .. "' not hookable")
            self:_dbg("[ChainEndpointController] OnTriggerEnter ignored — root='" .. rootName .. "' tag='" .. tostring(tag) .. "' not hookable")
            return
        end
        dbg("[ChainEndpointController][TRIGGER] PASSED all guards — proceeding to hook and snap")

        self:_dbg("[ChainEndpointController] OnTriggerEnter HOOKING — entity='" .. otherName .. "' root='" .. rootName .. "'")

        self._hookedEntityId = otherEntityId
        self._isExtending = false
        if self._rb then self._rb:SetEnabled(false) end

        -- Cache root transform via entity ID to avoid FindTransformByName returning
        -- a stale/wrong instance when multiple enemies share the same name
        self._hookedTransform = nil
        self._hookedColliderTransform = nil
        pcall(function()
            self._hookedTransform = Engine.FindTransformByID(rootId)
            self._hookedColliderTransform = Engine.FindTransformByID(otherEntityId)
        end)
        self:_dbg("[ChainEndpointController] hookedTransform=" .. tostring(self._hookedTransform ~= nil) .. " colliderTransform=" .. tostring(self._hookedColliderTransform ~= nil))

        local impactX = self._lastEndpointX or 0
        local impactY = self._lastEndpointY or 0
        local impactZ = self._lastEndpointZ or 0
        local snapDist = tonumber(self.HookSnapDistance) or 0

        -- Get root world position via its transform handle
        local rootWX, rootWY, rootWZ = impactX, impactY, impactZ
        if self._hookedTransform then
            local ok, a, b, c = pcall(function() return Engine.GetTransformWorldPosition(self._hookedTransform) end)
            if ok and a ~= nil then
                if type(a) == "table" then rootWX, rootWY, rootWZ = a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0
                elseif type(a) == "number" then rootWX, rootWY, rootWZ = a, b, c end
            end
        end
        self:_dbg(string.format("[ChainEndpointController][SNAP] impact=(%.3f,%.3f,%.3f) rootWorld=(%.3f,%.3f,%.3f)", impactX, impactY, impactZ, rootWX, rootWY, rootWZ))

        -- World offset from root pivot to impact point
        local dx = impactX - rootWX
        local dy = impactY - rootWY
        local dz = impactZ - rootWZ
        local totalDist = math.sqrt(dx*dx + dy*dy + dz*dz)
        self:_dbg(string.format("[ChainEndpointController][SNAP] totalDist=%.3f snapDist=%.3f", totalDist, snapDist))

        -- Parent first — engine recalculates our localPosition relative to root
        if self._entityId and Engine and Engine.SetParentEntity then
            local ok = pcall(function() Engine.SetParentEntity(self._entityId, rootId) end)
            if ok then
                self._isParented = true
                self:_dbg("[ChainEndpointController] Parented endpoint -> root '" .. rootName .. "' id=" .. tostring(rootId))
            else
                self._isParented = false
                self:_dbg("[ChainEndpointController] WARNING: SetParentEntity failed for root '" .. rootName .. "'")
            end
        end

        -- Write snapped local position.
        -- snapDist=0 means snap exactly to root pivot (local 0,0,0).
        -- snapDist>0 means sit that many units from the pivot toward the impact point.
        if self._isParented and self._endpointTransform then
            local sx, sy, sz = 0, 0, 0
            if snapDist > 1e-6 and totalDist > 1e-6 then
                local ratio = math.min(snapDist, totalDist) / totalDist
                sx, sy, sz = dx * ratio, dy * ratio, dz * ratio
            end
            pcall(function()
                local pos = self._endpointTransform.localPosition
                if type(pos) == "userdata" or type(pos) == "table" then
                    pos.x, pos.y, pos.z = sx, sy, sz
                else
                    self._endpointTransform.localPosition = { x = sx, y = sy, z = sz }
                end
                self._endpointTransform.isDirty = true
            end)
            self:_dbg(string.format("[ChainEndpointController][SNAP] snapDist=%.3f -> local=(%.3f,%.3f,%.3f)", snapDist, sx, sy, sz))
        end

        -- Publish hit so ChainBootstrap locks the controller and snapshots lockedEndPoint
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("chain.endpoint_hit_entity", {
                entityId   = otherEntityId,
                entityName = otherName,
                rootName   = rootName,
                rootTag    = tag,
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
            self:_dbg("[ChainEndpointController] _onDetach: unparented endpoint")
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