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
    if math.atan2 then return math.atan2(y, x) end
    local ok, res = pcall(math.atan, y, x)
    if ok then return res end
    if x > 0 then return math.atan(y / x)
    elseif x < 0 and y >= 0 then return math.atan(y / x) + math.pi
    elseif x < 0 and y < 0 then return math.atan(y / x) - math.pi
    elseif x == 0 and y > 0 then return math.pi / 2
    elseif x == 0 and y < 0 then return -math.pi / 2
    else return 0.0 end
end

-- Helper: Euler (deg) → Quaternion
local function eulerToQuat(pitch, yaw, roll)
    local p = math.rad(pitch or 0) * 0.5
    local y = math.rad(yaw or 0) * 0.5
    local r = math.rad(roll or 0) * 0.5
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

return Component {
    mixins = { TransformMixin },

    fields = {
        followDistance   = 2.0,
        heightOffset     = 1.0,
        followLerp       = 10.0,
        mouseSensitivity = 0.15,
        minPitch         = -30.0,
        maxPitch         = 60.0,
        minZoom          = 2.0,
        maxZoom          = 15.0,
        zoomSpeed        = 1.0,
        -- Camera collision settings
        collisionEnabled = true,
        collisionOffset  = 0.2,    -- How far to pull camera in front of hit point
        collisionLerpIn  = 20.0,   -- Fast snap when hitting wall
        collisionLerpOut = 5.0,    -- Slower ease when wall clears
    },

    Awake = function(self)
        self._yaw   = 180.0   -- start orbit rotated 180° around Y
        self._pitch = 15.0    -- keep a slight downward tilt

        self._targetPos  = { x = 0.0, y = 0.0, z = 0.0 }
        self._hasTarget  = false
        self._posSub     = nil
        self._lastMouseX = 0.0
        self._lastMouseY = 0.0
        self._firstMouse = true
        self._currentCollisionDist = nil  -- Track current collision-adjusted distance

        if event_bus and event_bus.subscribe then
            self._posSub = event_bus.subscribe("player_position", function(payload)
                if not payload then return end
                local x = payload.x or payload[1] or 0.0
                local y = payload.y or payload[2] or 0.0
                local z = payload.z or payload[3] or 0.0
                -- Store raw position, height offset applied in Update based on zoom
                self._targetPos.x, self._targetPos.y, self._targetPos.z = x, y, z
                self._hasTarget = true
            end)
        end

        -- Lock cursor when camera starts (game mode)
        if Screen and Screen.SetCursorLocked then
            Screen.SetCursorLocked(true)
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe and self._posSub then
            event_bus.unsubscribe(self._posSub)
            self._posSub = nil
        end

        -- Unlock cursor when camera is disabled
        if Screen and Screen.SetCursorLocked then
            Screen.SetCursorLocked(false)
        end
    end,

    _updateScrollZoom = function(self)
        if not (Input and Input.GetScrollY) then return end

        local scrollY = Input.GetScrollY()
        if scrollY ~= 0 then
            local zoomSpeed = self.zoomSpeed or 1.0
            self.followDistance = self.followDistance - scrollY * zoomSpeed
            self.followDistance = clamp(self.followDistance, self.minZoom or 2.0, self.maxZoom or 15.0)

            -- Consume scroll so it doesn't accumulate
            if Input.ConsumeScroll then
                Input.ConsumeScroll()
            end
        end
    end,

    _updateMouseLook = function(self, dt)
        -- Process mouse look for camera rotation
        if not (Input and Input.GetMouseX and Input.GetMouseY) then return end
        local xpos, ypos = Input.GetMouseX(), Input.GetMouseY()
        if self._firstMouse then
            self._firstMouse = false
            self._lastMouseX, self._lastMouseY = xpos, ypos
            return
        end
        local xoffset = (xpos - self._lastMouseX) * (self.mouseSensitivity or 0.15)
        local yoffset = (self._lastMouseY - ypos) * (self.mouseSensitivity or 0.15)
        self._lastMouseX, self._lastMouseY = xpos, ypos
        self._yaw   = self._yaw   - xoffset  -- Inverted for correct left/right panning
        self._pitch = clamp(self._pitch - yoffset, self.minPitch or -80.0, self.maxPitch or 80.0)

        -- Broadcast camera yaw for camera-relative player movement
        if event_bus and event_bus.publish then
            event_bus.publish("camera_yaw", self._yaw)
        end
    end,

    Update = function(self, dt)
        if not (self.GetPosition and self.SetPosition and self.SetRotation) then return end
        if not self._hasTarget then return end

        -- Toggle cursor lock with Escape
        if Input and Input.GetKeyDown and Input.GetKeyDown(Input.Key.Escape) then
            if Screen and Screen.SetCursorLocked and Screen.IsCursorLocked then
                local isLocked = Screen.IsCursorLocked()
                Screen.SetCursorLocked(not isLocked)
                self._firstMouse = true  -- Reset mouse tracking to avoid camera jump
            end
        end

        -- Re-lock cursor when clicking in game (if unlocked)
        if Input and Input.GetMouseButtonDown and Input.GetMouseButtonDown(Input.MouseButton.Left) then
            if Screen and Screen.SetCursorLocked and Screen.IsCursorLocked then
                if not Screen.IsCursorLocked() then
                    Screen.SetCursorLocked(true)
                    self._firstMouse = true
                end
            end
        end

        -- Only update camera look when cursor is locked
        if Screen and Screen.IsCursorLocked and Screen.IsCursorLocked() then
            self:_updateMouseLook(dt)
        end

        -- Update scroll zoom
        self:_updateScrollZoom()

        local radius = self.followDistance or 5.0
        local pitchRad = math.rad(self._pitch)
        local yawRad = math.rad(self._yaw)

        -- Scale height offset based on zoom distance (less offset when zoomed in)
        local minZoom = self.minZoom or 2.0
        local maxZoom = self.maxZoom or 15.0
        local zoomFactor = (radius - minZoom) / (maxZoom - minZoom)  -- 0 at min zoom, 1 at max zoom
        zoomFactor = clamp(zoomFactor, 0.0, 1.0)

        -- Target look-at point: scale from feet (0.5) at close zoom to chest (1.2) at far zoom
        local lookAtHeight = 0.5 + zoomFactor * 0.7  -- 0.5 to 1.2
        local tx = self._targetPos.x
        local ty = self._targetPos.y + lookAtHeight
        local tz = self._targetPos.z

        -- Camera height offset also scales with zoom
        local baseHeightOffset = self.heightOffset or 1.0
        local scaledHeightOffset = baseHeightOffset * (0.3 + zoomFactor * 0.7)  -- 30% to 100% of offset

        local horizontalRadius = radius * math.cos(pitchRad)
        local offsetX = horizontalRadius * math.sin(yawRad)
        local offsetZ = horizontalRadius * math.cos(yawRad)
        local offsetY = radius * math.sin(pitchRad) + scaledHeightOffset

        local desiredX, desiredY, desiredZ = tx + offsetX, ty + offsetY, tz + offsetZ

        -- Camera collision detection using raycast
        if self.collisionEnabled and Physics and Physics.Raycast then
            -- Direction from target to desired camera position
            local dirX = desiredX - tx
            local dirY = desiredY - ty
            local dirZ = desiredZ - tz
            local dirLen = math.sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ)

            if dirLen > 0.001 then
                -- Normalize direction
                dirX, dirY, dirZ = dirX/dirLen, dirY/dirLen, dirZ/dirLen

                -- Raycast from target toward camera
                -- Returns: distance (float, -1 if no hit)
                local dist = Physics.Raycast(
                    tx, ty, tz,           -- origin (player position)
                    dirX, dirY, dirZ,     -- direction (toward camera)
                    dirLen + 0.5          -- max distance (slightly beyond desired)
                )

                local hit = (dist >= 0 and dist < dirLen)
                local collisionOffset = self.collisionOffset or 0.2
                local targetDist = dirLen  -- Default: no collision

                if hit then
                    -- Wall detected! Pull camera in front of hit point
                    targetDist = math.max(dist - collisionOffset, 0.5)  -- Minimum 0.5 distance from player
                end

                -- Smooth collision distance (fast in, slow out)
                if self._currentCollisionDist == nil then
                    self._currentCollisionDist = targetDist
                else
                    local lerpSpeed
                    if targetDist < self._currentCollisionDist then
                        -- Snapping in (wall hit) - fast
                        lerpSpeed = self.collisionLerpIn or 20.0
                    else
                        -- Easing out (wall cleared) - slow
                        lerpSpeed = self.collisionLerpOut or 5.0
                    end
                    local collisionT = 1.0 - math.exp(-lerpSpeed * dt)
                    self._currentCollisionDist = self._currentCollisionDist + (targetDist - self._currentCollisionDist) * collisionT
                end

                -- Apply collision-adjusted position
                local adjustedDist = self._currentCollisionDist
                desiredX = tx + dirX * adjustedDist
                desiredY = ty + dirY * adjustedDist
                desiredZ = tz + dirZ * adjustedDist
            end
        end

        -- Smooth follow
        local cx, cy, cz = 0.0, 0.0, 0.0
        local px, py, pz = self:GetPosition()
        if type(px) == "table" then
            cx, cy, cz = px.x or 0.0, px.y or 0.0, px.z or 0.0
        else
            cx, cy, cz = px or 0.0, py or 0.0, pz or 0.0
        end

        local t = 1.0 - math.exp(-(self.followLerp or 10.0) * dt)
        local newX = cx + (desiredX - cx) * t
        local newY = cy + (desiredY - cy) * t
        local newZ = cz + (desiredZ - cz) * t
        self:SetPosition(newX, newY, newZ)

        -- Look at target: compute yaw/pitch, then convert to quaternion
        local fx, fy, fz = tx - newX, ty - newY, tz - newZ
        local flen = math.sqrt(fx*fx + fy*fy + fz*fz)
        if flen > 0.0001 then
            fx, fy, fz = fx/flen, fy/flen, fz/flen
            local yawDeg   = math.deg(atan2(fx, fz))
            local pitchDeg = -math.deg(math.asin(fy))
            local quat = eulerToQuat(pitchDeg, yawDeg, 0.0)
            self:SetRotation(quat.w, quat.x, quat.y, quat.z)
        end

        self.isDirty = true
    end,
}
