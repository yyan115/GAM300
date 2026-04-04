-- Resources/Scripts/Gameplay/DoorTrigger.lua

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local Engine    = _G.Engine
local event_bus = _G.event_bus

-------------------------------------------------
-- Component
-------------------------------------------------

return Component {

    mixins = { TransformMixin },

    fields = {
        
    },

    -------------------------------------------------
    -- Start
    -------------------------------------------------

    Start = function(self)

    end,

    -------------------------------------------------
    -- Update
    -------------------------------------------------

    Update = function(self, dt)

    end,

    -------------------------------------------------
    -- Helper: climb to root entity
    -------------------------------------------------

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

    -------------------------------------------------
    -- Trigger
    -------------------------------------------------

    OnTriggerEnter = function(self, otherEntityId)
        local rootId = self:_toRoot(otherEntityId)
        local tagComp = GetComponent(rootId, "TagComponent")
        if not (tagComp and Tag.Compare(tagComp.tagIndex, "Player")) then return end

        local hookPromptSpriteId = Engine.GetChildAtIndex(self.entityId, 0)
        local hookPromptSpriteActiveComp = GetComponent(hookPromptSpriteId, "ActiveComponent")
        hookPromptSpriteActiveComp.isActive = true
    end,

    OnTriggerExit = function(self, otherEntityId)
        local rootId = self:_toRoot(otherEntityId)
        local tagComp = GetComponent(rootId, "TagComponent")
        if not (tagComp and Tag.Compare(tagComp.tagIndex, "Player")) then return end

        local hookPromptSpriteId = Engine.GetChildAtIndex(self.entityId, 0)
        local hookPromptSpriteActiveComp = GetComponent(hookPromptSpriteId, "ActiveComponent")
        hookPromptSpriteActiveComp.isActive = false
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._bossKilledSub then _G.event_bus.unsubscribe(self._bossKilledSub); self._bossKilledSub = nil end
            if self._respawnSub    then _G.event_bus.unsubscribe(self._respawnSub);    self._respawnSub    = nil end
        end
    end,
}