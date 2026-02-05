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
    print(string.format("[DoorTrigger] Opening Doors: %s and %s", self.targetLeftDoor, self.targetRightDoor))
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
        if not self._playerTr then 
            -- print("[DoorTrigger] ERROR: Player transform not found!") 
            return false 
        end
    end

    local pp = Engine.GetTransformPosition(self._playerTr)
    if not pp then return false end

    local px, pz = pp[1], pp[3]
    local ex, ey, ez = self:GetPosition()

    local dx, dz = px - ex, pz - ez
    local distSq = (dx*dx + dz*dz)
    local r = self.triggerRadius
    
    -- Uncomment the line below if you need to see real-time distance in console
    -- print(string.format("[DoorTrigger] Dist: %.2f / Radius: %.2f", math.sqrt(distSq), r))

    return distSq <= (r*r)
end

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
        openOffset = 1.0,
        openDuration = 3.0,
        postOpenDelay = 2.0,
        hasOpened = false,
        openingTime = 0.0,
        delayTime = 0.0,
        isOpening = false,
        isWaiting = false,
    },

    Start = function(self)
        print("[DoorTrigger] Start called for Entity: " .. tostring(self.entityId))
        
        self.leftTransform  = Engine.FindTransformByName(self.targetLeftDoor)
        self.leftDoorEnt = Engine.GetEntityByName(self.targetLeftDoor)
        self.rightTransform = Engine.FindTransformByName(self.targetRightDoor)
        self.rightDoorEnt = Engine.GetEntityByName(self.targetRightDoor)

        self.weaponPickupEnt = Engine.GetEntityByName(self.weaponPickup)
        self.weaponOnHandEnt = Engine.GetEntityByName(self.weaponOnHand)

        if not self.leftTransform or not self.rightTransform then
            print(string.format("[DoorTrigger] ERROR: Could not find door transforms! L: %s, R: %s", 
                tostring(self.leftTransform), tostring(self.rightTransform)))
            return
        end

        -- Get the tables (arrays) from the engine
        local leftPosTable  = Engine.GetTransformPosition(self.leftTransform)
        local rightPosTable = Engine.GetTransformPosition(self.rightTransform)

        -- Extract the values safely
        local lx, ly, lz = leftPosTable[1],  leftPosTable[2],  leftPosTable[3]
        local rx, ry, rz = rightPosTable[1], rightPosTable[2], rightPosTable[3]

        self.leftStartPos  = {x=lx, y=ly, z=lz}
        self.rightStartPos = {x=rx, y=ry, z=rz}

        self.leftTargetPos  = {x = lx - self.openOffset, y = ly, z = lz}
        self.rightTargetPos = {x = rx + self.openOffset, y = ry, z = lz}

        print(string.format("[DoorTrigger] Initialized. Mode: %d, Radius: %.2f", self.triggerMode, self.triggerRadius))
    end,

    Update = function(self, dt)
        -- Check player range
        if not self.hasOpened then
            local inRange = CheckPlayerInRange(self)
            if inRange ~= self.isActivatable then
                self.isActivatable = inRange
                print("[DoorTrigger] Player " .. (inRange and "ENTERED" or "EXITED") .. " trigger zone")

                local weaponPickupActive = GetComponent(self.weaponPickupEnt, "ActiveComponent")
                weaponPickupActive.isActive = false
                local weaponOnHandActive = GetComponent(self.weaponOnHandEnt, "ActiveComponent")
                weaponOnHandActive.isActive = true
            end
        end

        -- Trigger logic
        if not self.hasOpened then
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

            -- DEBUG LOGS: Every frame during animation
            print(string.format("[DoorTrigger] Animating t: %.2f | LeftX: %.3f | RightX: %.3f", t, lx, rx))

            -- Ensure we have handles before calling C++ methods
            if self.leftTransform and self.rightTransform then
                self.leftTransform.localPosition.x = lx

                self.leftTransform.localPosition.y = self.leftStartPos.y

                self.leftTransform.localPosition.z = self.leftStartPos.z

                self.leftTransform.isDirty = true

                self.rightTransform.localPosition.x = rx

                self.rightTransform.localPosition.y = self.rightStartPos.y

                self.rightTransform.localPosition.z = self.rightStartPos.z
                self.rightTransform.isDirty = true
            else
                print("[DoorTrigger] ERROR: Transforms lost during animation!")
            end

            if t >= 1.0 then
                print("[DoorTrigger] Opening animation complete. Final LeftX: " .. lx)
                self.isOpening = false
                self.isWaiting = true
                self.delayTime = 0.0
            end
        end

        -- Post delay
        if self.isWaiting then
            self.delayTime = self.delayTime + dt
            if self.delayTime >= self.postOpenDelay then
                print("[DoorTrigger] Post-open delay finished.")
                self.isWaiting = false

                local leftDoorActive = GetComponent(self.leftDoorEnt, "ActiveComponent")
                leftDoorActive.isActive = false
                local rightDoorActive = GetComponent(self.rightDoorEnt, "ActiveComponent")
                rightDoorActive.isActive = false
            end
        end
    end
}