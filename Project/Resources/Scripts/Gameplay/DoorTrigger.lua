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

-------------------------------------------------
-- Helpers
-------------------------------------------------

local function OpenDoors(self)
    if self.hasOpened then return end

    print(string.format(
        "[DoorTrigger] Opening Doors: %s and %s",
        self.targetLeftDoor,
        self.targetRightDoor
    ))

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
    local distSq = dx*dx + dz*dz
    local r = self.triggerRadius

    return distSq <= (r*r)
end

local function AreEntitiesInactive(entityNames)
    for _, name in ipairs(entityNames) do
        local ent = Engine.GetEntityByName(name)

        if ent then
            local activeComp = GetComponent(ent, "ActiveComponent")

            if activeComp and activeComp.isActive then
                return false
            end
        end
    end

    return true
end

-------------------------------------------------
-- Component
-------------------------------------------------

return Component {

    mixins = { TransformMixin },

    fields = {
        playerName = "Player",
        triggerRadius = 0.5,
        isActivatable = false,

        triggerMode = DoorTriggerMode.InputKeyDown,

        targetLeftDoor = "LeftDoor1",
        targetRightDoor = "RightDoor2",

        weaponPickup = "WeaponPickup",
        weaponOnHand = "LowPolyFeatherChain",

        openZ = false, -- move the door by z axis instead of x
        openOffset = 1.0,
        openDuration = 3.0,
        postOpenDelay = 2.0,

        hasOpened = false,
        openingTime = 0.0,
        delayTime = 0.0,
        isOpening = false,
        isWaiting = false,

        entitiesToCheckInactive = {
            "EnemyMeleeA",
            "EnemyMeleeB",
            "EnemyMeleeC",
            "EnemyRangeD"
        },

        pickupSFX = {},
        doorOpenSFX = {}
    },

    -------------------------------------------------
    -- Start
    -------------------------------------------------

    Start = function(self)

        print("[DoorTrigger] Start called for Entity: " .. tostring(self.entityId))

        self.leftTransform  = Engine.FindTransformByName(self.targetLeftDoor)
        self.rightTransform = Engine.FindTransformByName(self.targetRightDoor)

        self.leftDoorEnt  = Engine.GetEntityByName(self.targetLeftDoor)
        self.rightDoorEnt = Engine.GetEntityByName(self.targetRightDoor)

        self.weaponPickupEnt = Engine.GetEntityByName(self.weaponPickup)
        self.weaponOnHandEnt = Engine.GetEntityByName(self.weaponOnHand)

        if not self.leftTransform or not self.rightTransform then
            print("[DoorTrigger] ERROR: Door transforms not found")
            return
        end

        local leftPos  = Engine.GetTransformPosition(self.leftTransform)
        local rightPos = Engine.GetTransformPosition(self.rightTransform)

        local lx, ly, lz = leftPos[1], leftPos[2], leftPos[3]
        local rx, ry, rz = rightPos[1], rightPos[2], rightPos[3]

        self.leftStartPos  = {x=lx, y=ly, z=lz}
        self.rightStartPos = {x=rx, y=ry, z=rz}

        if not openZ then
            self.leftTargetPos  = {x = lx - self.openOffset, y = ly, z = lz}
            self.rightTargetPos = {x = rx + self.openOffset, y = ry, z = rz}
        end

        if openZ then
            self.leftTargetPos  = {x = lx, y = ly, z = lz - self.openOffset}
            self.rightTargetPos = {x = rx, y = ry, z = rz + self.openOffset}
        end
    end,

    -------------------------------------------------
    -- Update
    -------------------------------------------------

    Update = function(self, dt)

        -------------------------------------------------
        -- Player range detection
        -------------------------------------------------

        if not self.hasOpened then

            local inRange = CheckPlayerInRange(self)

            if inRange ~= self.isActivatable then

                self.isActivatable = inRange

                print("[DoorTrigger] Player " ..
                    (inRange and "ENTERED" or "EXITED") ..
                    " trigger zone")

                local weaponPickupActive =
                    GetComponent(self.weaponPickupEnt, "ActiveComponent")

                local weaponOnHandActive =
                    GetComponent(self.weaponOnHandEnt, "ActiveComponent")

                if inRange then
                    if weaponPickupActive and weaponPickupActive.isActive then
                        weaponPickupActive.isActive = false
                    end

                    if weaponOnHandActive and not weaponOnHandActive.isActive then
                        weaponOnHandActive.isActive = true
                    end
                end
            end
        end

        -------------------------------------------------
        -- Trigger Modes
        -------------------------------------------------

        if not self.hasOpened then

            if self.triggerMode == DoorTriggerMode.AutoOnEnter then
                if self.isActivatable then
                    OpenDoors(self)
                end
            end

            if self.triggerMode == DoorTriggerMode.OnEntitiesDestroyed then
                if self.isActivatable and
                   AreEntitiesInactive(self.entitiesToCheckInactive) then

                    print("[DoorTrigger] All required entities inactive.")
                    OpenDoors(self)
                end
            end
        end

        -------------------------------------------------
        -- Door Animation
        -------------------------------------------------

        if self.isOpening then

            self.openingTime = self.openingTime + dt
            local t = math.min(self.openingTime / self.openDuration, 1.0)

            local lx = Lerp(self.leftStartPos.x,  self.leftTargetPos.x,  t)
            local rx = Lerp(self.rightStartPos.x, self.rightTargetPos.x, t)

            if self.leftTransform and self.rightTransform then

                self.leftTransform.localPosition.x = lx
                self.leftTransform.localPosition.y = self.leftStartPos.y
                self.leftTransform.localPosition.z = self.leftStartPos.z
                self.leftTransform.isDirty = true

                self.rightTransform.localPosition.x = rx
                self.rightTransform.localPosition.y = self.rightStartPos.y
                self.rightTransform.localPosition.z = self.rightStartPos.z
                self.rightTransform.isDirty = true
            end

            if t >= 1.0 then
                self.isOpening = false
                self.isWaiting = true
                self.delayTime = 0.0
            end
        end

        -------------------------------------------------
        -- Post Delay
        -------------------------------------------------

        if self.isWaiting then

            self.delayTime = self.delayTime + dt

            if self.delayTime >= self.postOpenDelay then

                self.isWaiting = false

                local leftDoorActive =
                    GetComponent(self.leftDoorEnt, "ActiveComponent")

                local rightDoorActive =
                    GetComponent(self.rightDoorEnt, "ActiveComponent")

                if leftDoorActive then
                    leftDoorActive.isActive = false
                end

                if rightDoorActive then
                    rightDoorActive.isActive = false
                end
            end
        end
    end
}
