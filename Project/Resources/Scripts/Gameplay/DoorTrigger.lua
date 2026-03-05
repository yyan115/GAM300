-- Resources/Scripts/Gameplay/DoorTrigger.lua

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local Engine    = _G.Engine
local event_bus = _G.event_bus

local DoorTriggerMode = {
    InputKeyDown = 1,
    AutoOnEnter  = 2,
}

-------------------------------------------------
-- Helpers
-------------------------------------------------

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

-------------------------------------------------
-- Component
-------------------------------------------------

return Component {

    mixins = { TransformMixin },

    fields = {
        playerName = "Player",
        triggerRadius = 0.5,
        isActivatable = false,

        triggerMode = DoorTriggerMode.AutoOnEnter,

        targetLeftDoor = "LeftDoor1",
        targetRightDoor = "RightDoor2",

        weaponPickup = "LowPolyFeatherChainPickUp",
        weaponOnHand = "LowPolyFeatherChain",

        openDuration = 2.5,
        postOpenDelay = 2.0,
        openAngle = 115,
        overshootAngle = 10,

        pickupSFX = {},     -- table of AudioClips for pickup
        doorOpenSFX = {},   -- table of AudioClips for door

        hasOpened = false,
        openingTime = 0.0,
        isOpening = false,
        isWaiting = false,
    },

    -------------------------------------------------
    -- Start
    -------------------------------------------------

    Start = function(self)
        self.leftTransform  = Engine.FindTransformByName(self.targetLeftDoor)
        self.rightTransform = Engine.FindTransformByName(self.targetRightDoor)

        self.leftDoorEnt  = Engine.GetEntityByName(self.targetLeftDoor)
        self.rightDoorEnt = Engine.GetEntityByName(self.targetRightDoor)

        self.weaponPickupEnt = Engine.GetEntityByName(self.weaponPickup)
        self.weaponOnHandEnt = Engine.GetEntityByName(self.weaponOnHand)

        if not self.leftTransform or not self.rightTransform then
            print("[DoorTrigger] ERROR: Door transforms not found")
            self.initFailed = true
            return
        end

        -- Store starting Y rotations
        local leftRot  = Engine.GetTransformRotation(self.leftTransform)
        local rightRot = Engine.GetTransformRotation(self.rightTransform)
        self.leftStartRotY  = leftRot[2]
        self.rightStartRotY = rightRot[2]
    end,

    -------------------------------------------------
    -- Update
    -------------------------------------------------

    Update = function(self, dt)
        if self.initFailed then return end

        -------------------------------------------------
        -- Player range detection
        -------------------------------------------------
        if not self.hasOpened then
            local inRange = CheckPlayerInRange(self)
            self.isActivatable = inRange

            if inRange then
                -- --- Weapon pickup (instant) ---
                if self.weaponPickupEnt and self.weaponOnHandEnt then
                    local pickupActiveComp = GetComponent(self.weaponPickupEnt, "ActiveComponent")
                    local handActiveComp   = GetComponent(self.weaponOnHandEnt, "ActiveComponent")

                    -- Play pickup SFX
                    local weaponSFX = GetComponent(self.weaponPickupEnt, "AudioComponent")
                    if weaponSFX and self.pickupSFX[0] then
                        weaponSFX:PlayOneShot(self.pickupSFX[0])
                    end

                    if pickupActiveComp then pickupActiveComp.isActive = false end
                    if handActiveComp then handActiveComp.isActive = true end
                    _G.playerHasWeapon = true
                    print("[DoorTrigger] Weapon picked up instantly!")
                end

                -- --- Start door opening ---
                self.hasOpened = true
                self.isOpening = true
                self.openingTime = 0.0

                -- Play door open SFX
                local doorAudio = GetComponent(self.leftDoorEnt, "AudioComponent")
                if doorAudio and self.doorOpenSFX[0] then
                    doorAudio:PlayOneShot(self.doorOpenSFX[0])
                end

                if event_bus and event_bus.publish then
                    event_bus.publish("cinematic.trigger", true)
                end
            end
        end

        -------------------------------------------------
        -- Door Animation
        -------------------------------------------------
        if self.isOpening and self.leftTransform and self.rightTransform then
            self.openingTime = self.openingTime + dt
            local t = math.min(self.openingTime / self.openDuration, 1.0)
            t = t * t * (3 - 2 * t) -- SmoothStep

            local lRotY = Lerp(self.leftStartRotY,  self.leftStartRotY - self.openAngle, t)
            local rRotY = Lerp(self.rightStartRotY, self.rightStartRotY + self.openAngle, t)

            -- Overshoot
            if t > 0.85 then
                local overshootT = (t - 0.85)/0.15
                overshootT = overshootT * overshootT
                local overshootAmount = self.overshootAngle * (1 - overshootT)
                lRotY = lRotY - overshootAmount
                rRotY = rRotY + overshootAmount
            end

            self.leftTransform.localRotation.y  = lRotY
            self.rightTransform.localRotation.y = rRotY
            self.leftTransform.isDirty = true
            self.rightTransform.isDirty = true

            if t >= 1.0 then
                self.isOpening = false
                self.isWaiting = true
                self.delayTime = 0.0

                -- Disable colliders
                if self.leftDoorEnt then
                    local leftCol = GetComponent(self.leftDoorEnt, "ColliderComponent")
                    if leftCol then leftCol.enabled = false end
                end
                if self.rightDoorEnt then
                    local rightCol = GetComponent(self.rightDoorEnt, "ColliderComponent")
                    if rightCol then rightCol.enabled = false end
                end

                print("[DoorTrigger] Doors fully opened and colliders disabled")
            end
        end

        -------------------------------------------------
        -- Post Delay (doors stay visible)
        -------------------------------------------------
        if self.isWaiting then
            self.delayTime = self.delayTime + dt
            if self.delayTime >= self.postOpenDelay then
                self.isWaiting = false
            end
        end
    end
}