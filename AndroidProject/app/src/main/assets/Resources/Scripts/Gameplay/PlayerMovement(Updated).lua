require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus
local Input = _G.Input

-- Animation States
local IDLE = 0
local RUN  = 1
local JUMP = 2

-- Helper: convert 2D movement vector to Y-axis quaternion
local function directionToQuaternion(dx, dz)
    local angle
    if dx == 0 and dz == 0 then
        angle = 0
    elseif dz > 0 then
        angle = math.atan(dx / dz)
    elseif dz < 0 then
        angle = math.atan(dx / dz) + (dx >= 0 and math.pi or -math.pi)
    else
        angle = dx > 0 and (math.pi * 0.5) or (-math.pi * 0.5)
    end

    local halfAngle = angle * 0.5
    return math.cos(halfAngle), 0, math.sin(halfAngle), 0
end

-- Helper: lerp quaternion for smooth rotation
local function lerpQuaternion(w1, x1, y1, z1, w2, x2, y2, z2, t)
    if w1*w2 + x1*x2 + y1*y2 + z1*z2 < 0 then
        w2, x2, y2, z2 = -w2, -x2, -y2, -z2
    end
    local w = w1 + (w2 - w1) * t
    local x = x1 + (x2 - x1) * t
    local y = y1 + (y2 - y1) * t
    local z = z1 + (z2 - z1) * t
    local invLen = 1.0 / math.sqrt(w*w + x*x + y*y + z*z + 0.0001)
    return w * invLen, x * invLen, y * invLen, z * invLen
end

return Component {
    mixins = { TransformMixin },

    fields = {
        Speed = 1.5,
        JumpHeight = 1.2,
    },

    Awake = function(self)
        self._currentRotW = 1
        self._currentRotX = 0
        self._currentRotY = 0
        self._currentRotZ = 0

        -- Camera state
        self._cameraYaw = 180.0
        self._cameraYawSub = nil

        if event_bus and event_bus.subscribe then
            print("[PlayerMovement] Subscribing to camera_yaw")
            self._cameraYawSub = event_bus.subscribe("camera_yaw", function(yaw)
                if yaw then
                    self._cameraYaw = yaw
                end
            end)
            print("[PlayerMovement] Subscription token: " .. tostring(self._cameraYawSub))
        else
            print("[PlayerMovement] ERROR: event_bus not available!")
        end
    end,

    Start = function(self)
        self._collider  = self:GetComponent("ColliderComponent")
        self._animator  = self:GetComponent("AnimationComponent")
        self._transform = self:GetComponent("Transform")

        print("transform here is ", self._transform.localPosition.x)
        self._controller = CharacterController.Create(self.entityId, self._collider, self._transform)

        -- Use PlayClip directly (state machine approach doesn't work)
        if self._animator then
            print("[PlayerMovement] Animator found, playing IDLE clip")
            self._animator:PlayClip(IDLE, true)
        else
            print("[PlayerMovement] ERROR: Animator is nil!")
        end

        self._isRunning = false
        self._isJumping = false
        self.rotationSpeed = 10.0
    end,

    Update = function(self, dt)
        if not self._collider or not self._transform or not self._controller then
            return
        end

        -- ===============================
        -- RAW INPUT (LOCAL SPACE) - Using new unified input system
        -- ===============================
        local axis = Input and Input.GetAxis and Input.GetAxis("Movement") or { x = 0, y = 0 }
        local rawX = -axis.x  -- Invert X to match old behavior (A=+1, D=-1)
        local rawZ = axis.y   -- Z is forward/back

        -- ===============================
        -- CAMERA-RELATIVE MOVEMENT (MERGED)
        -- ===============================
        -- Read camera yaw from global (set by camera_follow.lua) - bypasses event_bus
        local cameraYaw = _G.CAMERA_YAW or self._cameraYaw or 180.0

        local moveX, moveZ = 0, 0
        if rawX ~= 0 or rawZ ~= 0 then
            local yawRad = math.rad(cameraYaw)
            local sinYaw = math.sin(yawRad)
            local cosYaw = math.cos(yawRad)

            moveX = rawZ * (-sinYaw) - rawX * cosYaw
            moveZ = rawZ * (-cosYaw) + rawX * sinYaw
        end

        local isMoving = (moveX ~= 0 or moveZ ~= 0)

        -- ===============================
        -- JUMP
        -- ===============================
        local isGrounded = CharacterController.IsGrounded(self._controller)
        local isJumping = false

        if Input and Input.IsActionJustPressed and Input.IsActionJustPressed("Jump") and isGrounded then
            CharacterController.Jump(self._controller, self.JumpHeight)
            isJumping = true
        end

        -- ===============================
        -- APPLY MOVEMENT (WORLD UNITS)
        -- ===============================
        if not isJumping and isMoving then
            CharacterController.Move(
                self._controller,
                moveX * self.Speed,
                0,
                moveZ * self.Speed
            )
        end

        -- ANIMATION (using PlayClip directly)
        if not isGrounded then
            if not self._isJumping then
                -- Start jump animation
                print("[PlayerMovement] PlayClip(JUMP=" .. JUMP .. ")")
                self._animator:PlayClip(JUMP, false)
                self._isJumping = true
                self._isRunning = false
            end
        else
            -- Landed
            if self._isJumping then
                self._isJumping = false
                -- Resume proper state based on movement
                if isMoving then
                    print("[PlayerMovement] PlayClip(RUN=" .. RUN .. ")")
                    self._animator:PlayClip(RUN, true)
                    self._isRunning = true
                else
                    print("[PlayerMovement] PlayClip(IDLE=" .. IDLE .. ")")
                    self._animator:PlayClip(IDLE, true)
                    self._isRunning = false
                end
            elseif isMoving and not self._isRunning then
                print("[PlayerMovement] PlayClip(RUN=" .. RUN .. ")")
                self._animator:PlayClip(RUN, true)
                self._isRunning = true
            elseif not isMoving and self._isRunning then
                print("[PlayerMovement] PlayClip(IDLE=" .. IDLE .. ")")
                self._animator:PlayClip(IDLE, true)
                self._isRunning = false
            end
        end

        -- ===============================
        -- ROTATION
        -- ===============================
        if isMoving and self.SetRotation then
            local targetW, targetX, targetY, targetZ =
                directionToQuaternion(moveX, moveZ)

            local t = math.min(self.rotationSpeed * dt, 1.0)
            local newW, newX, newY, newZ =
                lerpQuaternion(
                    self._currentRotW, self._currentRotX,
                    self._currentRotY, self._currentRotZ,
                    targetW, targetX, targetY, targetZ,
                    t
                )

            self._currentRotW = newW
            self._currentRotX = newX
            self._currentRotY = newY
            self._currentRotZ = newZ

            pcall(self.SetRotation, self, newW, newX, newY, newZ)
        end

        -- ===============================
        -- POSITION SYNC + CAMERA BROADCAST
        -- ===============================
        local position = CharacterController.GetPosition(self._controller)
        if position then
            self:SetPosition(position.x, position.y, position.z)

            if event_bus and event_bus.publish then
                event_bus.publish("player_position", position)
            end
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe and self._cameraYawSub then
            event_bus.unsubscribe(self._cameraYawSub)
            self._cameraYawSub = nil
        end
    end,
}
