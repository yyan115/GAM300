-- camera_follow.lua
require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

local function clamp(x, minv, maxv)
    if x < minv then return minv end
    if x > maxv then return maxv end
    return x
end

-- portable atan2(y, x)
local function atan2(y, x)
    -- if engine/Lua provides math.atan2, use it
    if math.atan2 then
        return math.atan2(y, x)
    end

    -- try Lua 5.3+ style math.atan(y, x)
    local ok, res = pcall(math.atan, y, x)
    if ok then
        return res
    end

    -- fallback: single-arg atan with quadrant handling
    if x > 0 then
        return math.atan(y / x)
    elseif x < 0 and y >= 0 then
        return math.atan(y / x) + math.pi
    elseif x < 0 and y < 0 then
        return math.atan(y / x) - math.pi
    elseif x == 0 and y > 0 then
        return math.pi / 2
    elseif x == 0 and y < 0 then
        return -math.pi / 2
    else
        return 0.0
    end
end

-- try SetRotation(p, y, r) then SetRotation({x=,y=,z=})
local function safeSetRotation(self, pitchDeg, yawDeg, rollDeg)
    if not self.SetRotation then
        return
    end

    local ok = pcall(function()
        self:SetRotation(pitchDeg, yawDeg, rollDeg or 0.0)
    end)
    if not ok then
        pcall(function()
            self:SetRotation({ x = pitchDeg, y = yawDeg, z = rollDeg or 0.0 })
        end)
    end
end

return Component {
    mixins = { TransformMixin },

    fields = {
        followDistance   = 5.0,
        heightOffset     = 0.0,
        followLerp       = 10.0,   -- how quickly camera catches up

        mouseSensitivity = 0.15,   -- tweak in inspector
        minPitch         = -30.0,  -- degrees
        maxPitch         = 60.0,   -- degrees
    },

    Awake = function(self)
        print("[LUA][CameraFollow] Awake (entityId =", tostring(self.__entity_id), ")")

        -- orbit angles
        self._yaw        = 0.0
        self._pitch      = 15.0

        -- target data from event bus
        self._targetPos  = { x = 0.0, y = 0.0, z = 0.0 }
        self._hasTarget  = false
        self._posSub     = nil

        -- mouse tracking
        self._lastMouseX = 0.0
        self._lastMouseY = 0.0
        self._firstMouse = true

        -- subscribe to player_position events
        if event_bus and event_bus.subscribe then
            print("[LUA][CameraFollow] Subscribing to 'player_position'")
            self._posSub = event_bus.subscribe("player_position", function(payload)
                if not payload then return end

                -- accept both table {x=,y=,z=} or array-like {1,2,3}
                local x = payload.x or payload[1] or 0.0
                local y = payload.y or payload[2] or 0.0
                local z = payload.z or payload[3] or 0.0

                self._targetPos.x = x
                self._targetPos.y = y
                self._targetPos.z = z

                if not self._hasTarget then
                    print(string.format(
                        "[LUA][CameraFollow] First player_position received: x=%.3f y=%.3f z=%.3f",
                        x, y, z
                    ))
                end

                self._hasTarget = true
            end)
        else
            print("[LUA][CameraFollow] WARNING: event_bus not available; camera will never get a target")
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe and self._posSub then
            event_bus.unsubscribe(self._posSub)
            self._posSub = nil
        end
    end,

    -- Handles right-mouse drag to update yaw/pitch
    _updateMouseLook = function(self, dt)
        if not (Input and Input.GetMouseButton and Input.GetMouseX and Input.GetMouseY) then
            return
        end

        -- hold RIGHT mouse to rotate camera
        if Input.GetMouseButton(Input.MouseButton.Right) then
            local xpos = Input.GetMouseX()
            local ypos = Input.GetMouseY()

            if self._firstMouse then
                self._firstMouse = false
                self._lastMouseX = xpos
                self._lastMouseY = ypos
                return
            end

            local xoffset = xpos - self._lastMouseX
            local yoffset = self._lastMouseY - ypos -- inverted so moving mouse up looks up

            self._lastMouseX = xpos
            self._lastMouseY = ypos

            local sens = self.mouseSensitivity or 0.15
            xoffset = xoffset * sens
            yoffset = yoffset * sens

            self._yaw   = self._yaw   + xoffset
            self._pitch = self._pitch + yoffset

            self._pitch = clamp(self._pitch, self.minPitch or -80.0, self.maxPitch or 80.0)
        else
            -- when button released, reset so we don't get a huge jump next time
            self._firstMouse = true
        end
    end,

    Update = function(self, dt)
        if not (self.GetPosition and self.SetPosition and self.SetRotation) then
            return
        end

        -- if we never got a player position, nothing to follow/look at
        if not self._hasTarget then
            return
        end

        -- 1) Update yaw/pitch from mouse drag
        self:_updateMouseLook(dt)

        -- 2) Desired camera position from orbit around _targetPos
        local tx = self._targetPos.x
        local ty = self._targetPos.y
        local tz = self._targetPos.z

        local radius    = self.followDistance or 5.0
        local pitchRad  = math.rad(self._pitch)
        local yawRad    = math.rad(self._yaw)

        local horizontalRadius = radius * math.cos(pitchRad)
        local offsetX = horizontalRadius * math.sin(yawRad)
        local offsetZ = horizontalRadius * math.cos(yawRad)
        local offsetY = radius * math.sin(pitchRad) + (self.heightOffset or 0.0)

        local desiredX = tx + offsetX
        local desiredY = ty + offsetY
        local desiredZ = tz + offsetZ

        -- 3) Smooth follow
        local cx, cy, cz = self:GetPosition()
        local lerpSpeed  = self.followLerp or 10.0
        local t          = 1.0 - math.exp(-lerpSpeed * dt)

        local newX = cx + (desiredX - cx) * t
        local newY = cy + (desiredY - cy) * t
        local newZ = cz + (desiredZ - cz) * t

        self:SetPosition(newX, newY, newZ)

        -- 4) Always look at the target (player)
        local fx = tx - newX
        local fy = ty - newY
        local fz = tz - newZ
        local flen = math.sqrt(fx * fx + fy * fy + fz * fz)
        if flen > 0.0001 then
            fx = fx / flen
            fy = fy / flen
            fz = fz / flen

            local pitchDeg = math.deg(math.asin(fy))
            -- use our portable atan2 here
            local yawDeg   = math.deg(atan2(fx, fz))

            if not self._debuggedOnce then
                print(string.format(
                    "[LUA][CameraFollow] lookAt: fx=%.3f fy=%.3f fz=%.3f pitch=%.2f yaw=%.2f",
                    fx, fy, fz, pitchDeg, yawDeg
                ))
                self._debuggedOnce = true
            end

            safeSetRotation(self, pitchDeg, yawDeg, 0.0)

            if not self._debugRotOnce then
                self:DebugTransform()
                self._debugRotOnce = true
            end
        end

        -- mark transform dirty so C++ side updates matrices
        self.isDirty = true
    end,
}
