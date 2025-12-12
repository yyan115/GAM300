require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

-- Animation States
local IDLE = 0
local RUN  = 1
local JUMP = 2


local JumpHeight = 4

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
        Speed          = 10,
        },

    Awake = function(self)
        self._currentRotW = 1
        self._currentRotX = 0
        self._currentRotY = 0
        self._currentRotZ = 0
    end,

    Start = function(self)
        self._collider = self:GetComponent("ColliderComponent")
        self._animator = self:GetComponent("AnimationComponent")
        self._transform = self:GetComponent("Transform")

        self._controller = CharacterController.new()
        CharacterController.Initialise(self._controller, self._collider, self._transform)
        self._animator:PlayClip(IDLE, true)

        self._isRunning = false
        self._isJumping = false
        self.rotationSpeed = 10.0  -- adjust for smoothness
    end,

    Update = function(self, dt)
        if not self._collider or not self._transform or not self._controller then
            print("Missing component/controller")
            return
        end

        CharacterController.Update(self._controller, dt)

        -- MOVEMENT INPUT
        local moveX, moveY, moveZ = 0, 0, 0
        if Input.GetKey(Input.Key.W) then moveZ = moveZ + 1 end
        if Input.GetKey(Input.Key.S) then moveZ = moveZ - 1 end
        if Input.GetKey(Input.Key.A) then moveX = moveX + 1 end
        if Input.GetKey(Input.Key.D) then moveX = moveX - 1 end

        -- normalize diagonal movement
        local length = math.sqrt(moveX*moveX + moveZ*moveZ)
        if length > 1 then
            moveX = moveX / length
            moveZ = moveZ / length
        end

        local isGrounded = CharacterController.IsGrounded(self._controller)
        local isJumping = false
        if Input.GetKeyDown(Input.Key.Space) and isGrounded then 
            CharacterController.Jump(self._controller, JumpHeight)
            isJumping = true
        end

        local isMoving = (moveX ~= 0 or moveZ ~= 0)

        -- APPLY MOVEMENT
        if not isJumping and isMoving then
            CharacterController.Move(self._controller, moveX * self.Speed, moveY, moveZ * self.Speed)
        end
        
        -- Detect landing
        local wasJumping = self._isJumping
        if wasJumping and isGrounded then
            -- Landed
            self._animator:PlayClip(IDLE, true)
            self._isJumping = false
            self._isRunning = false
        end

        -- Jump start
        if isJumping then
            self._animator:PlayClip(JUMP, false)
            self._isJumping = true
            self._isRunning = false
            return  -- Don't override animation this frame
        end

        -- Running
        if isMoving and not self._isJumping then
            if not self._isRunning then
                self._animator:PlayClip(RUN, true)
                self._isRunning = true
            end
        else
            -- Idle
            if not self._isJumping and self._isRunning then
                self._animator:PlayClip(IDLE, true)
                self._isRunning = false
            end
        end


        -- ROTATION: face movement direction
        if isMoving and self.SetRotation then
            local targetW, targetX, targetY, targetZ = directionToQuaternion(moveX, moveZ)
            local t = math.min(self.rotationSpeed * dt, 1.0)
            local newW, newX, newY, newZ = lerpQuaternion(
                self._currentRotW, self._currentRotX, self._currentRotY, self._currentRotZ,
                targetW, targetX, targetY, targetZ,
                t
            )

            self._currentRotW = newW
            self._currentRotX = newX
            self._currentRotY = newY
            self._currentRotZ = newZ

            pcall(self.SetRotation, self, newW, newX, newY, newZ)
        end

        -- UPDATE POSITION
        local position = CharacterController.GetPosition(self._controller)
        if position then
            self:SetPosition(position.x, position.y, position.z)
        else
            print("[LUA WARNING] GetPosition returned nil")
        end
    end
}
