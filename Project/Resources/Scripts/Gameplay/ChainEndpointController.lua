-- ChainEndpointController.lua
-- Attach this script to the ChainEndpoint object.
-- Handles model visibility, trigger detection while extending,
-- and locking endpoint position to a struck entity.
local Component = require("extension.mono_helper")

return Component {
    fields = {
        PlayerName = "Player",
    },

    Start = function(self)
        -- ModelRenderComponent is on this object directly
        self._modelRender = self:GetComponent("ModelRenderComponent")
        if not self._modelRender then
            print("[ChainEndpointController] WARNING: ModelRenderComponent not found on endpoint object")
        end

        -- RigidBody — disabled by default, only enabled while extending (same as AttackHitbox)
        self._rb = self:GetComponent("RigidBodyComponent")
        if self._rb then
            self._rb:SetEnabled(false)
        end

        -- Resolve player entity ID so we never hit ourselves
        self._playerEntityId = nil
        if Engine and Engine.GetEntityByName then
            self._playerEntityId = Engine.GetEntityByName(self.PlayerName)
        end

        -- State
        self._isVisible      = false
        self._isExtending    = false
        self._hookedEntityId = nil   -- entity the endpoint is stuck to
        self._hookedTransform = nil  -- its transform for position tracking

        -- Model starts hidden
        self:_setModelVisible(false)

        -- Subscribe to chain endpoint events from ChainBootstrap
        if _G.event_bus and _G.event_bus.subscribe then
            self._subMoved = _G.event_bus.subscribe("chain.endpoint_moved", function(payload)
                if not payload then return end
                pcall(function() self:_onEndpointMoved(payload) end)
            end)

            self._subRetracted = _G.event_bus.subscribe("chain.endpoint_retracted", function(payload)
                if not payload then return end
                pcall(function() self:_onEndpointRetracted(payload) end)
            end)

            -- Future command subscriptions
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
        end
    end,

    -- -------------------------------------------------------------------------
    -- Chain state events from ChainBootstrap
    -- -------------------------------------------------------------------------

    -- Called every frame while chain is active
    _onEndpointMoved = function(self, payload)
        if not self._isVisible then
            self:_setModelVisible(true)
        end

        -- Cache current endpoint world position — needed to compute impact offset on trigger enter
        if payload.position then
            self._lastEndpointX = payload.position.x
            self._lastEndpointY = payload.position.y
            self._lastEndpointZ = payload.position.z
        end

        -- Enable RB only while actively extending so OnTriggerEnter can fire.
        -- Keep RB alive briefly after extending stops (chain locked via raycast)
        -- so the physics engine has time to process the trigger collision.
        local extending = payload.isExtending or false
        if extending and not self._isExtending then
            self._isExtending = true
            self._rbKeepAliveFrames = 0
            if self._rb then self._rb:SetEnabled(true) end
            print("[ChainEndpointController] Extending — trigger enabled")
        elseif not extending and self._isExtending then
            self._isExtending = false
            -- Don't kill RB immediately — give physics 3 frames to fire OnTriggerEnter
            self._rbKeepAliveFrames = 3
            print("[ChainEndpointController] No longer extending — trigger window open")
        end

        -- Count down keep-alive and disable RB when expired
        if not self._isExtending and self._rbKeepAliveFrames and self._rbKeepAliveFrames > 0 then
            self._rbKeepAliveFrames = self._rbKeepAliveFrames - 1
            if self._rbKeepAliveFrames == 0 and not self._hookedEntityId then
                if self._rb then self._rb:SetEnabled(false) end
                print("[ChainEndpointController] Trigger window closed — RB disabled")
            end
        end

        -- If hooked to an entity, publish pivot + offset as the locked position
        -- so the endpoint stays at the exact impact point as the entity moves
        if self._hookedEntityId and self._hookedTransform then
            local pos = Engine.GetTransformPosition(self._hookedTransform)
            if pos then
                local wx = pos[1] + (self._hookedOffsetX or 0)
                local wy = pos[2] + (self._hookedOffsetY or 0)
                local wz = pos[3] + (self._hookedOffsetZ or 0)
                if _G.event_bus and _G.event_bus.publish then
                    _G.event_bus.publish("chain.endpoint_hooked_position", {
                        x = wx, y = wy, z = wz,
                        entityId = self._hookedEntityId,
                    })
                end
            end
        end
    end,

    -- Called once when chain fully retracted
    _onEndpointRetracted = function(self, payload)
        if self._isVisible then
            self:_setModelVisible(false)
        end
        -- Full reset on retract
        self._isExtending = false
        self:_clearHook()
        if self._rb then self._rb:SetEnabled(false) end
    end,

    -- -------------------------------------------------------------------------
    -- Trigger — fires when endpoint collides with something while extending
    -- -------------------------------------------------------------------------

    OnTriggerEnter = function(self, otherEntityId)
        -- Fire during extending OR during the keep-alive window after chain locks
        local rbActive = self._rb and self._rb:IsEnabled()
        if not rbActive then return end
        -- Already hooked to something
        if self._hookedEntityId then return end

        -- Walk to root entity (same as AttackHitbox)
        local targetId = self:_toRoot(otherEntityId)

        -- Never hook the player
        if self._playerEntityId and targetId == self._playerEntityId then return end

        -- Try to get entity name for debug
        local entityName = "unknown"
        if Engine and Engine.GetEntityName then
            local ok, name = pcall(function() return Engine.GetEntityName(targetId) end)
            if ok and name and name ~= "" then
                entityName = name
            end
        end
        print("[ChainEndpointController] OnTriggerEnter — entity id=" .. tostring(targetId) .. " name=" .. entityName)

        -- Lock onto this entity
        self._hookedEntityId = targetId
        self._isExtending = false
        if self._rb then self._rb:SetEnabled(false) end

        -- Cache transform for position tracking each frame
        self._hookedTransform = Engine.FindTransformByName(entityName ~= "unknown" and entityName or "")

        -- Store the world position of the endpoint at moment of impact
        -- and the entity's pivot at that moment, so we can compute a local offset.
        -- This means as the entity moves, the hook stays at the exact impact point
        -- rather than snapping to its pivot.
        local impactX = self._lastEndpointX or 0
        local impactY = self._lastEndpointY or 0
        local impactZ = self._lastEndpointZ or 0

        local pivotX, pivotY, pivotZ = 0, 0, 0
        if self._hookedTransform then
            local pos = Engine.GetTransformPosition(self._hookedTransform)
            if pos then
                pivotX, pivotY, pivotZ = pos[1], pos[2], pos[3]
            end
        end

        -- Local offset = impact point - entity pivot
        self._hookedOffsetX = impactX - pivotX
        self._hookedOffsetY = impactY - pivotY
        self._hookedOffsetZ = impactZ - pivotZ

        print("[ChainEndpointController] Hooked at impact=("
            .. string.format("%.2f,%.2f,%.2f", impactX, impactY, impactZ)
            .. ") offset=("
            .. string.format("%.2f,%.2f,%.2f", self._hookedOffsetX, self._hookedOffsetY, self._hookedOffsetZ)
            .. ")")

        -- Notify ChainBootstrap
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("chain.endpoint_hit_entity", {
                entityId = targetId,
                entityName = entityName,
            })
        end
    end,

    -- -------------------------------------------------------------------------
    -- Future functions — prints on call only
    -- -------------------------------------------------------------------------

    _onAttach = function(self, payload)
        print("[ChainEndpointController] _onAttach called")
    end,

    _onDetach = function(self, payload)
        print("[ChainEndpointController] _onDetach called")
    end,

    _onCheckCollision = function(self, payload)
        print("[ChainEndpointController] _onCheckCollision called")
    end,

    -- -------------------------------------------------------------------------
    -- Helpers
    -- -------------------------------------------------------------------------

    _toRoot = function(self, entityId)
        local targetId = entityId
        if Engine and Engine.GetParentEntity then
            while true do
                local parentId = Engine.GetParentEntity(targetId)
                if not parentId or parentId < 0 then break end
                targetId = parentId
            end
        end
        return targetId
    end,

    _clearHook = function(self)
        self._hookedEntityId  = nil
        self._hookedTransform = nil
        self._hookedOffsetX   = 0
        self._hookedOffsetY   = 0
        self._hookedOffsetZ   = 0
    end,

    _setModelVisible = function(self, visible)
        if not self._modelRender then return end
        pcall(function()
            ModelRenderComponent.SetVisible(self._modelRender, visible)
        end)
        self._isVisible = visible
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