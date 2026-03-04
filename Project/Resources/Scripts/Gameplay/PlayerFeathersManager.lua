require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    fields = {
        FeathersCollectedTextName = "FeathersCollectedText",
    },

    Awake = function(self)
        self._numFeathers = 0

        if event_bus and event_bus.subscribe then
            -- Prevent double-subscription on the SAME instance
            if self._featherCollectedSub then 
                print("[PlayerFeathersManager] Already subscribed. Skipping.")
                return 
            end

            print("[PlayerFeathersManager] Subscribing to featherCollected")
            self._featherCollectedSub = event_bus.subscribe("featherCollected", function(payload)
                if payload then
                    print(string.format("[PlayerFeathersManager] Event Received! EntityID: %s", tostring(self.entityId)))
                    self._numFeathers = self._numFeathers + 1
                    if self._feathersCollectedTextComponent then
                        self._feathersCollectedTextComponent.text = string.format("%d", self._numFeathers)
                    end
                end
            end)
        else
            print("[PlayerFeathersManager] ERROR: event_bus not available!")
        end
    end,

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        self._feathersCollectedTextEntity = Engine.GetEntityByName(self.FeathersCollectedTextName)
        if self._feathersCollectedTextEntity then
            self._feathersCollectedTextComponent = GetComponent(self._feathersCollectedTextEntity, "TextRenderComponent")
            if self._feathersCollectedTextComponent then
                self._feathersCollectedTextComponent.text = "0"
            end
        end
    end,

    Update = function(self, dt) 

    end,

    OnDisable = function(self)
        if self._featherCollectedSub and _G.event_bus then
            print("[PlayerFeathersManager] Unsubscribing token: " .. tostring(self._featherCollectedSub))
            _G.event_bus.unsubscribe(self._featherCollectedSub)
            self._featherCollectedSub = nil
        end
    end,
}