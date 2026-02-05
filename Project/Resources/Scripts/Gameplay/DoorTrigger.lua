-- Resources/Scripts/Gameplay/DoorTrigger.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local Engine = _G.Engine
local Input  = _G.Input
local Time   = _G.Time

local DoorTriggerMode = {
    InputKeyDown = 1,
    AutoOnEnter  = 2,
    OnEntitiesDestroyed = 3,
}

local function OpenDoors(self)
    if self.hasOpened then return end

    self.hasOpened = true
    self.isOpening = true
    self.openingTime = 0.0
    self.isActivatable = false
end

local function Lerp(a, b, t)
    return a + (b - a) * t
end

local function CheckPlayerInRange(self)
    if not self._playerTr then
        self._playerTr = Engine.FindTransformByName(self.playerName)
        if not self._playerTr then return false end
    end

    local pp = Engine.GetTransformPosition(self._playerTr)
    if not pp then return false end

    local px, pz = pp[1], pp[3]

    local ex, ey, ez = self:GetPosition()

    local dx, dz = px - ex, pz - ez
    local r = self.triggerRadius

    return (dx*dx + dz*dz) <= (r*r)
end

return Component {
    mixins = { TransformMixin },

    fields = {
        --- Trigger Detection ---
        playerName = "Player",
        triggerRadius = 0.5,
        isActivatable = false,

        --- Trigger Mode ---
        triggerMode = DoorTriggerMode.InputKeyDown,

        --- Door Opening ---
        targetLeftDoor = "LeftDoor1",
        targetRightDoor = "RightDoor1",
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
        self.leftTransform  = Engine.FindTransformByName(self.targetLeftDoor)
        self.rightTransform = Engine.FindTransformByName(self.targetRightDoor)

        local lx, ly, lz = self.leftTransform:GetPosition()
        local rx, ry, rz = self.rightTransform:GetPosition()

        self.leftStartPos  = {x=lx, y=ly, z=lz}
        self.rightStartPos = {x=rx, y=ry, z=rz}

        self.leftTargetPos  = {x = lx - self.openOffset, y = ly, z = lz}
        self.rightTargetPos = {x = rx + self.openOffset, y = ry, z = rz}

        self._playerTr = Engine.FindTransformByName(self.playerName)
    end,

    Update = function(self, dt)
        if self.isActivatable then
            print("Player in trigger zone")
        end
        
        -- Check player range
        if not self.hasOpened then
            self.isActivatable = CheckPlayerInRange(self)
        end

        -- Trigger input
        if not self.hasOpened then
            --if self.triggerMode == DoorTriggerMode.InputKeyDown then
            --    if self.isActivatable and Input. then
            --        OpenDoors(self)
            --    end
            --elseif self.triggerMode == DoorTriggerMode.AutoOnEnter then
            if self.triggerMode == DoorTriggerMode.AutoOnEnter then
                if self.isActivatable then
                    OpenDoors(self)
                end
            end
        end

        -- Door open animation
        if self.isOpening then
            self.openingTime = self.openingTime + dt
            local t = math.min(self.openingTime / self.openDuration, 1.0)

            local lx = Lerp(self.leftStartPos.x,  self.leftTargetPos.x,  t)
            local rx = Lerp(self.rightStartPos.x, self.rightTargetPos.x, t)

            self.leftTransform:SetPosition(lx, self.leftStartPos.y, self.leftStartPos.z)
            self.rightTransform:SetPosition(rx, self.rightStartPos.y, self.rightStartPos.z)

            if t >= 1.0 then
                self.isOpening = false
                self.isWaiting = true
                self.delayTime = 0.0
            end
        end

        -- Post delay
        if self.isWaiting then
            self.delayTime = self.delayTime + dt
            if self.delayTime >= self.postOpenDelay then
                self.isWaiting = false
            end
        end
    end
}