require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

local function lerp(a, b, t)
    return a + (b - a) * t
end

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

local function multiplyQuat(q1, q2)
    return {
        w = q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z,
        x = q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
        y = q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x,
        z = q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w
    }
end

local function rotateVec(q, v)
    local qx, qy, qz, qw = q.x, q.y, q.z, q.w
    local vx, vy, vz = v.x, v.y, v.z
    local tx = 2 * (qy * vz - qz * vy)
    local ty = 2 * (qz * vx - qx * vz)
    local tz = 2 * (qx * vy - qy * vx)
    
    return {
        x = vx + qw * tx + (qy * tz - qz * ty),
        y = vy + qw * ty + (qz * tx - qx * tz),
        z = vz + qw * tz + (qx * ty - qy * tx)
    }
end

local function getWorldTransform(entityId)
    local wPos = { x=0, y=0, z=0 }
    local wRot = { w=1, x=0, y=0, z=0 }
    
    local currId = entityId
    while currId and currId >= 0 do
        local t = GetComponent(currId, "Transform")
        if t then
            if t.localScale then
                wPos.x = wPos.x * t.localScale.x
                wPos.y = wPos.y * t.localScale.y
                wPos.z = wPos.z * t.localScale.z
            end
            
            wPos = rotateVec(t.localRotation, wPos)
            wPos.x = wPos.x + t.localPosition.x
            wPos.y = wPos.y + t.localPosition.y
            wPos.z = wPos.z + t.localPosition.z
            
            wRot = multiplyQuat(t.localRotation, wRot)
        end
        
        if Engine and Engine.GetParentEntity then
            local pId = Engine.GetParentEntity(currId)
            if pId == currId then break end 
            currId = pId
        else
            break
        end
    end
    return wPos, wRot
end

return Component {
    mixins = { TransformMixin },

    fields = {
        CandleLightUpDelay = 0.5,
        CandleLightUpInterval = 0.3,
        StationaryCinematicDuration = 3.0,
        PanDuration = 1.5,
        CameraTargetName = "CandleCameraTarget",
    },

    Awake = function(self)
        self._beginLightUpDelay = false
        self._beginLightUpSequence = false
        self._lightUpSequenceFinished = false
        self._currentLightUpDelay = 0.0
        self._currentInterval = 0.0
        self._currentPairIndex = 0
        self._cinematicStarted = false
        self._currentCinematicDuration = 0.0
    end,

    Start = function(self)
        self._candleCount = Engine.GetChildCount(self.entityId)
    end,

    Update = function(self, dt)
        -- Phase 1: Slight delay before starting the candle light up sequence
        if self._beginLightUpDelay and not self._beginLightUpSequence then
            self._currentLightUpDelay = self._currentLightUpDelay + dt
            if self._currentLightUpDelay >= self.CandleLightUpDelay then
                self._beginLightUpDelay = false
                self._beginLightUpSequence = true
            end
        end

        -- Phase 2: Actual candle light up sequence
        if self._beginLightUpSequence and not self._lightUpSequenceFinished then
            self._currentInterval = self._currentInterval + dt
            if self._currentInterval >= self.CandleLightUpInterval then
                local firstCandleId = Engine.GetChildAtIndex(self.entityId, self._currentPairIndex)
                local secondCandleId = Engine.GetChildAtIndex(self.entityId, self._currentPairIndex + 1)

                -- Enable the bloom component on the pair of candle entities
                local firstCandleBloomComp = GetComponent(firstCandleId, "BloomComponent")
                local secondCandleBloomComp = GetComponent(secondCandleId, "BloomComponent")
                firstCandleBloomComp.enabled = true
                secondCandleBloomComp.enabled = true

                -- Enable the child point lights of the pair of candle entities
                local firstPointLightId = Engine.GetChildAtIndex(firstCandleId, 0)
                local secondPointLightId = Engine.GetChildAtIndex(secondCandleId, 0)
                local firstPointLightActive = GetComponent(firstPointLightId, "ActiveComponent")
                local secondPointLightActive = GetComponent(secondPointLightId, "ActiveComponent")
                firstPointLightActive.isActive = true
                secondPointLightActive.isActive = true

                if event_bus and event_bus.publish then
                    event_bus.publish("candle_lit", {})
                end

                self._currentInterval = 0.0
                self._currentPairIndex = self._currentPairIndex + 2

                if self._currentPairIndex >= self._candleCount then
                    self._lightUpSequenceFinished = true
                end
            end
        end

        -- Phase 3: Re-enable attacks after cinematic duration elapses
        if self._cinematicStarted and self._currentCinematicDuration < self.StationaryCinematicDuration + self.PanDuration then
            self._currentCinematicDuration = self._currentCinematicDuration + dt
            if self._currentCinematicDuration >= self.StationaryCinematicDuration + self.PanDuration then
                if event_bus and event_bus.publish then
                    event_bus.publish("set_attacks_enabled", true)
                end
            end
        end
    end,

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

    OnTriggerEnter = function(self, otherEntityId)
        if self._lightUpSequenceFinished then return end

        local rootId = self:_toRoot(otherEntityId)
        local tagComp = GetComponent(rootId, "TagComponent")

        if tagComp and Tag.Compare(tagComp.tagIndex, "Player") then
            self._beginLightUpDelay = true
            self._cinematicStarted = true
            self._currentCinematicDuration = 0.0

            if event_bus and event_bus.publish then
                event_bus.publish("set_attacks_enabled", false)
                -- Tell the cinematic camera to look at the candles
                if self.CameraTargetName and self.CameraTargetName ~= "" then
                    event_bus.publish("cinematic.targetName", self.CameraTargetName)
                end
                event_bus.publish("cinematic.trigger", true)
                event_bus.publish("cinematic.stayDuration", self.StationaryCinematicDuration)
                event_bus.publish("cinematic.transitionDuration", self.PanDuration)
            end
        end
    end,

    OnDisable = function(self)

    end,
}