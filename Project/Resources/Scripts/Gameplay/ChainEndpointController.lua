-- ChainEndpointController.lua
-- Attach this script to the ChainEndpoint object.
-- Handles model visibility and receives commands from ChainBootstrap via event bus.
local Component = require("extension.mono_helper")

return Component {
    fields = {
    },

    Start = function(self)
        -- ModelRenderComponent is on this object directly
        self._modelRender = self:GetComponent("ModelRenderComponent")
        if not self._modelRender then
            print("[ChainEndpointController] WARNING: ModelRenderComponent not found on endpoint object")
        end

        -- Model starts hidden (chain is idle)
        self:_setModelVisible(false)

        -- Track current visibility state
        self._isVisible = false

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

            -- Future command subscriptions from ChainBootstrap/ChainManager
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

    -- Called every frame while chain is active (extending, in flight, or retracting)
    _onEndpointMoved = function(self, payload)
        if not self._isVisible then
            self:_setModelVisible(true)
        end
    end,

    -- Called once when chain has fully retracted back to the hand
    _onEndpointRetracted = function(self, payload)
        if self._isVisible then
            self:_setModelVisible(false)
        end
    end,

    -- -------------------------------------------------------------------------
    -- Future functions — functionality not yet implemented, prints on call only
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

    _setModelVisible = function(self, visible)
        if not self._modelRender then return end
        pcall(function()
            ModelRenderComponent.SetVisible(self._modelRender, visible)
        end)
        self._isVisible = visible
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._subMoved          then pcall(function() _G.event_bus.unsubscribe(self._subMoved)          end) end
            if self._subRetracted      then pcall(function() _G.event_bus.unsubscribe(self._subRetracted)      end) end
            if self._subAttach         then pcall(function() _G.event_bus.unsubscribe(self._subAttach)         end) end
            if self._subDetach         then pcall(function() _G.event_bus.unsubscribe(self._subDetach)         end) end
            if self._subCheckCollision then pcall(function() _G.event_bus.unsubscribe(self._subCheckCollision) end) end
        end
    end,
}