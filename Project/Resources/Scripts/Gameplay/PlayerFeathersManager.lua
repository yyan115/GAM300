require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    fields = {
        FeathersCollectedTextName = "FeathersCollectedText",
        FeathersSpriteEntityName = "FeathersSprite",
        FeathersCollectionAnimationFrames = 25,
        featherPickupSFX = {},
    },

    Awake = function(self)
        _G._numFeathers = 0

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
                    _G._numFeathers = _G._numFeathers + 1
                    if self._feathersCollectedTextComponent then
                        self._feathersCollectedTextComponent.text = string.format("x%d", _G._numFeathers)
                    end

                    if self._feathersSpriteEntity then
                        local feathersSpriteAnimator = GetComponent(self._feathersSpriteEntity, "SpriteAnimationComponent")
                        feathersSpriteAnimator.currentFrameIndex = 0
                        feathersSpriteAnimator.currentClipIndex = 0
                        feathersSpriteAnimator:Resume()
                    end

                    if self._audio then
                        self._audio:PlayOneShot(self.featherPickupSFX[1])
                    end
                end
            end)

            -- Prevent double-subscription on the SAME instance
            if self._activatedFeatherSkillSub then 
                print("[PlayerFeathersManager] Already subscribed. Skipping.")
                return 
            end

            print("[PlayerFeathersManager] Subscribing to _activatedFeatherSkillSub")
            self._activatedFeatherSkillSub = event_bus.subscribe("activated_feather_skill", function(payload)
                if payload then
                    print(string.format("[PlayerFeathersManager] Event Received! EntityID: %s", tostring(self.entityId)))
                    if self._feathersCollectedTextComponent then
                        self._feathersCollectedTextComponent.text = string.format("x%d", _G._numFeathers)
                    end
                end
            end)
        else
            print("[PlayerFeathersManager] ERROR: event_bus not available!")
        end
    end,

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        self._audio = self:GetComponent("AudioComponent")

        self._feathersCollectedTextEntity = Engine.GetEntityByName(self.FeathersCollectedTextName)
        if self._feathersCollectedTextEntity then
            self._feathersCollectedTextComponent = GetComponent(self._feathersCollectedTextEntity, "TextRenderComponent")
            if self._feathersCollectedTextComponent then
                self._feathersCollectedTextComponent.text = "x0"
            end
        end

        self._feathersSpriteEntity = Engine.GetEntityByName(self.FeathersSpriteEntityName)
        local feathersSpriteAnimator = GetComponent(self._feathersSpriteEntity, "SpriteAnimationComponent")
        feathersSpriteAnimator.currentFrameIndex = 1
        feathersSpriteAnimator:Stop()
    end,

    Update = function(self, dt) 
        local feathersSpriteAnimator = GetComponent(self._feathersSpriteEntity, "SpriteAnimationComponent")
        if feathersSpriteAnimator.currentClipIndex == 0 then
            if feathersSpriteAnimator.currentFrameIndex == (self.FeathersCollectionAnimationFrames - 1) then
                feathersSpriteAnimator.currentClipIndex = 1
                feathersSpriteAnimator:Stop()
            end
        end
    end,

    OnDisable = function(self)
        if self._featherCollectedSub and _G.event_bus then
            print("[PlayerFeathersManager] Unsubscribing token: " .. tostring(self._featherCollectedSub))
            _G.event_bus.unsubscribe(self._featherCollectedSub)
            self._featherCollectedSub = nil
        end
    end,
}