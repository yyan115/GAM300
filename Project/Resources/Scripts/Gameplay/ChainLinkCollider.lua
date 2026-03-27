-- ChainLinkCollider.lua
-- Attach to the Link prefab. Every duplicated link gets this script.
-- Uses self.entityId (always set by engine) to look up its own link index
-- from _G._chainLinkIndexByEntityId, which ChainBootstrap writes in Start
-- before any link Awakes run.

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {},    -- required so the inspector/field parser doesn't fail

    Awake = function(self)
        print(string.format("[ChainLinkCollider] Awake entityId=%s", tostring(self.entityId)))
        self._col = self:GetComponent("ColliderComponent")
        self._rb  = self:GetComponent("RigidBodyComponent")

        -- Parse link index directly from entity name (e.g. "Link42" -> 42)
        local entityName = Engine.GetEntityName and Engine.GetEntityName(self.entityId) or ""
        self._linkIndex = tonumber(entityName:match("%d+$")) or 0
        print(string.format("[ChainLinkCollider] entityName=%s linkIndex=%s", tostring(entityName), tostring(self._linkIndex)))
        
        -- Start disabled
        if self._col then self._col.enabled = false end
        if self._rb  then self._rb.enabled  = false end

        if _G.event_bus and _G.event_bus.subscribe then
            self._colliderStateSub = _G.event_bus.subscribe("chain.collider_state", function(payload)
                if not payload or self._linkIndex == 0 then return end

                local interval = tonumber(payload.colliderInterval) or 5
                -- Only interval links react
                if interval <= 0 or (self._linkIndex % interval) ~= 0 then return end

                local enable = payload.shouldLive
                           and (self._linkIndex <= (payload.activeN or 0))

                if self._col then self._col.enabled = enable end
                if self._rb  then self._rb.enabled  = enable end
            end)
        end
    end,

    OnDisable = function(self)
        if self._col then self._col.enabled = false end
        if self._rb  then self._rb.enabled  = false end

        if _G.event_bus and _G.event_bus.unsubscribe and self._colliderStateSub then
            _G.event_bus.unsubscribe(self._colliderStateSub)
            self._colliderStateSub = nil
        end
    end,
}