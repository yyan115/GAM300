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

-- Convert Euler angles (degrees) to Quaternion
local function eulerToQuat(pitch, yaw, roll)
    local p = math.rad(pitch or 0) * 0.5
    local y = math.rad(yaw or 0)   * 0.5
    local r = math.rad(roll or 0)  * 0.5

    local sinP, cosP = math.sin(p), math.cos(p)
    local sinY, cosY = math.sin(y), math.cos(y)
    local sinR, cosR = math.sin(r), math.cos(r)

    return {
        w = cosP * cosY * cosR + sinP * sinY * sinR,
        x = sinP * cosY * cosR - cosP * sinY * sinR,
        y = cosP * sinY * cosR + sinP * cosY * sinR,
        z = cosP * cosY * sinR - sinP * sinY * cosR
    }
end

-- Multiply two Quaternions together to combine rotations
local function multiplyQuat(q1, q2)
    return {
        w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z,
        x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
        y = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x,
        z = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w
    }
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

        -- Weapon Fly Timing and Offsets
        weaponFlyDuration = 0.5, 
        weaponHandOffsetY = 0.6, -- Approximates the hand/chest height from the player's root
        weaponHandOffsetX = -0.1,

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

        isWeaponFlying = false,
        weaponFlyTime = 0.0,
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

        local lRot = self.leftTransform.localRotation
        local rRot = self.rightTransform.localRotation
        self.leftStartRot  = { w = lRot.w, x = lRot.x, y = lRot.y, z = lRot.z }
        self.rightStartRot = { w = rRot.w, x = rRot.x, y = rRot.y, z = rRot.z }
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
                self.hasOpened = true

                -- --- Start Weapon Fly ---
                if self.weaponPickupEnt and self.weaponOnHandEnt then
                    self.isWeaponFlying = true
                    self.weaponFlyTime = 0.0
                    
                    self.pickupTr = GetComponent(self.weaponPickupEnt, "Transform")
                    if self.pickupTr then
                        self.pickupStartPos = { 
                            x = self.pickupTr.localPosition.x, 
                            y = self.pickupTr.localPosition.y, 
                            z = self.pickupTr.localPosition.z 
                        }
                    end

                    if event_bus and event_bus.publish then
                        event_bus.publish("picked_up_weapon", true)
                    end
                end

                -- --- Start door opening ---
                self.isOpening = true
                self.openingTime = 0.0

                -- Play door open SFX
                local DoorTriggerSFX = self:GetComponent("AudioComponent")
                if DoorTriggerSFX and self.doorOpenSFX[1] then
                    DoorTriggerSFX:PlayOneShot(self.doorOpenSFX[1])
                end

                if event_bus and event_bus.publish then
                    event_bus.publish("cinematic.trigger", true)
                end
            end
        end

        -------------------------------------------------
        -- Weapon Flying Animation
        -------------------------------------------------
        if self.isWeaponFlying and self.pickupTr then
            self.weaponFlyTime = self.weaponFlyTime + dt
            local t = math.min(self.weaponFlyTime / self.weaponFlyDuration, 1.0)
            local easeT = t * t * (3 - 2 * t) -- Smoothstep

            -- Dynamically track the player so it homes in even if they walk away
            local playerEnt = Engine.GetEntityByName(self.playerName)
            local playerTrComp = GetComponent(playerEnt, "Transform")

            if playerTrComp then
                local targetX = playerTrComp.localPosition.x + self.weaponHandOffsetX
                local targetY = playerTrComp.localPosition.y + self.weaponHandOffsetY
                local targetZ = playerTrComp.localPosition.z

                self.pickupTr.localPosition.x = Lerp(self.pickupStartPos.x, targetX, easeT)
                self.pickupTr.localPosition.y = Lerp(self.pickupStartPos.y, targetY, easeT)
                self.pickupTr.localPosition.z = Lerp(self.pickupStartPos.z, targetZ, easeT)
                self.pickupTr.isDirty = true
            end

            -- Once it hits the hand, finalize the swap!
            if t >= 1.0 then
                self.isWeaponFlying = false

                local pickupActiveComp = GetComponent(self.weaponPickupEnt, "ActiveComponent")
                local handActiveComp   = GetComponent(self.weaponOnHandEnt, "ActiveComponent")

                -- Play pickup SFX exactly when it hits the hand
                local pickupWeaponSFX = self:GetComponent("AudioComponent")
                if pickupWeaponSFX and self.pickupSFX[1] then
                    pickupWeaponSFX:PlayOneShot(self.pickupSFX[1])
                end

                if pickupActiveComp then pickupActiveComp.isActive = false end
                if handActiveComp then handActiveComp.isActive = true end
                
                _G.playerHasWeapon = true
                print("[DoorTrigger] Weapon successfully caught!")
            end
        end

        -------------------------------------------------
        -- Door Animation
        -------------------------------------------------
        if self.isOpening and self.leftTransform and self.rightTransform then
            self.openingTime = self.openingTime + dt
            
            local p = math.min(self.openingTime / self.openDuration, 1.0)
            
            local lAngle = 0
            local rAngle = 0

            -- Phase 1: SmoothStep to the Overshoot angle (First 80% of the duration)
            if p < 0.8 then
                local lp = p / 0.8 
                local t = lp * lp * (3 - 2 * lp) 
                lAngle = Lerp(0, -(self.openAngle + self.overshootAngle), t)
                rAngle = Lerp(0, (self.openAngle + self.overshootAngle), t)
            
            -- Phase 2: SmoothStep back to the resting angle (Last 20% of the duration)
            else
                local lp = (p - 0.8) / 0.2 
                local t = lp * lp * (3 - 2 * lp) 
                lAngle = Lerp(-(self.openAngle + self.overshootAngle), -self.openAngle, t)
                rAngle = Lerp((self.openAngle + self.overshootAngle), self.openAngle, t)
            end

            local lQuatOffset = eulerToQuat(0, lAngle, 0)
            local rQuatOffset = eulerToQuat(0, rAngle, 0)
            
            local finalLeftRot = multiplyQuat(self.leftStartRot, lQuatOffset)
            local finalRightRot = multiplyQuat(self.rightStartRot, rQuatOffset)

            self.leftTransform.localRotation.w = finalLeftRot.w
            self.leftTransform.localRotation.x = finalLeftRot.x
            self.leftTransform.localRotation.y = finalLeftRot.y
            self.leftTransform.localRotation.z = finalLeftRot.z

            self.rightTransform.localRotation.w = finalRightRot.w
            self.rightTransform.localRotation.x = finalRightRot.x
            self.rightTransform.localRotation.y = finalRightRot.y
            self.rightTransform.localRotation.z = finalRightRot.z
            
            self.leftTransform.isDirty = true
            self.rightTransform.isDirty = true

            if p >= 1.0 then
                self.isOpening = false
                self.isWaiting = true
                self.delayTime = 0.0

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