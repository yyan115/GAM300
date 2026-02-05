-- Resources/Scripts/Gameplay/DoorTrigger.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local DoorTriggerMode = {
    InputKeyDown = 1,
    AutoOnEnter  = 2,
    OnEntitiesDestroyed = 3,
}

local function OpenDoors(self)
    if self.hasOpened then return end
    self.hasOpened = true
    
end
   
function Component:TriggerCameraPanBack()
    -- 
end

return Component {
    mixins = { TransformMixin },

    fields = {
        --- Trigger Detection ---
        triggerRadius = 0.5,
        isActivatable = false,

        --- Trigger Mode ---
        triggerMode = DoorTriggerMode.InputKeyDown,

        --- Door Opening ---
        targetLeftDoor = "LeftDoor",
        targetRightDoor = "RightDoor",
        openOffset = 0.5,
        openDuration = 3.0,
        postOpenDelay = 2.0,

        --- Runtime ---
        hasOpened = false,
        openingTime = 0.0,
        delayTime = 0.0,
        isOpening = false,
        isWaiting = false,
    },

    Start = function(self)
        self.leftDoor = GameObject.Find(self.targetLeftDoor)
        self.rightDoor = GameObject.Find(self.targetRightDoor)

        self.leftTransform = self.leftDoor:GetComponent("Transform")
        self.rightTransform = self.rightDoor:GetComponent("Transform")

        self.leftStartPos = self.leftTransform.position
        self.rightStartPos = self.rightTransform.position

        self.leftTargetPos = self.leftStartPos + Vector3.left * self.openOffset
        self.rightTargetPos = self.rightStartPos + Vector3.right * self.openOffset
    end,


    Update = function(self)
        -- trigger logic (from enum)
        if not self.hasOpened then
            if self.triggerMode == DoorTriggerMode.InputKeyDown then
                if self.isActivatable and Input.GetMouseButtonDown(Input.MouseButton.Left) then
                    OpenDoors(self)
                end
            end
        end

        -- door opening animation
        if self.isOpening then
            self.openingTime = self.openingTime + Time.deltaTime
            local t = math.min(self.openingTime / self.openDuration, 1.0)

            self.leftTransform.position =
                Vector3.Lerp(self.leftStartPos, self.leftTargetPos, t)

            self.rightTransform.position =
                Vector3.Lerp(self.rightStartPos, self.rightTargetPos, t)

            if t >= 1.0 then
                self.isOpening = false
                self.isWaiting = true
                self.delayTime = 0.0
            end
        end

        -- post-open delay
        if self.isWaiting then
            self.delayTime = self.delayTime + Time.deltaTime

            if self.delayTime >= self.postOpenDelay then
                self.isWaiting = false
                self:TriggerCameraPanBack()
            end
        end
    end
}