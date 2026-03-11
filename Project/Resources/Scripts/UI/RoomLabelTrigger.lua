-- RoomTrigger.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local event_bus = _G.event_bus

return Component {

    fields = {
        RoomIndex = 1,  -- map this in inspector for each trigger
    },

    Start = function(self)
        self._triggered = false  -- track if this trigger has already fired
    end,

    OnTriggerEnter = function(self, otherEntityId)
        if self._triggered then return end  -- exit if already triggered
        if not otherEntityId then return end

        -- Only trigger when player enters
        local rootId = otherEntityId
        if Engine and Engine.GetParentEntity then
            while true do
                local parentId = Engine.GetParentEntity(rootId)
                if not parentId or parentId < 0 then break end
                rootId = parentId
            end
        end

        if Engine.GetEntityName(rootId) ~= "Player" then return end

        -- Mark as triggered
        self._triggered = true

        -- Publish event to manager
        if event_bus and event_bus.publish then
            event_bus.publish("room_trigger_entered", { roomIndex = self.RoomIndex })
        end
    end,
}