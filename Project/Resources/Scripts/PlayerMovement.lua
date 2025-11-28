-- PlayerMovement.lua
-- Optimized player movement with animation and SFX support

require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

-- Cache frequently used math functions (CPU optimization)
local sqrt = math.sqrt
local atan = math.atan
local cos = math.cos
local sin = math.sin
local min = math.min
local random = math.random
local pi = math.pi

local event_bus = _G.event_bus

-- Reusable position table to avoid GC allocations
local _tempPos = { x = 0, y = 0, z = 0 }

-- Helper: get world position (optimized - reuses table)
local function getWorldPosition(self, outPos)
    outPos = outPos or _tempPos
    local ok, a, b, c = pcall(self.GetPosition, self)
    if not ok then
        outPos.x, outPos.y, outPos.z = 0, 0, 0
        return outPos
    end
    
    if type(a) == "table" then
        outPos.x = a.x or a[1] or 0
        outPos.y = a.y or a[2] or 0
        outPos.z = a.z or a[3] or 0
    else
        outPos.x = a or 0
        outPos.y = b or 0
        outPos.z = c or 0
    end
    return outPos
end

-- Helper: set world position
local function setWorldPosition(self, x, y, z)
    local ok = pcall(self.SetPosition, self, x, y, z)
    if not ok then
        pcall(function() self:SetPosition({ x = x, y = y, z = z }) end)
    end
end

-- Helper: direction to quaternion (Y-axis rotation)
local function directionToQuaternion(dx, dz)
    local angle
    if dx == 0 and dz == 0 then
        angle = 0
    elseif dz > 0 then
        angle = atan(dx / dz)
    elseif dz < 0 then
        angle = atan(dx / dz) + (dx >= 0 and pi or -pi)
    else
        angle = dx > 0 and (pi * 0.5) or (-pi * 0.5)
    end
    
    local halfAngle = angle * 0.5
    return cos(halfAngle), 0, sin(halfAngle), 0
end

-- Helper: quaternion lerp with normalization
local function lerpQuaternion(w1, x1, y1, z1, w2, x2, y2, z2, t)
    -- Shortest path
    if w1*w2 + x1*x2 + y1*y2 + z1*z2 < 0 then
        w2, x2, y2, z2 = -w2, -x2, -y2, -z2
    end
    
    -- Lerp
    local w = w1 + (w2 - w1) * t
    local x = x1 + (x2 - x1) * t
    local y = y1 + (y2 - y1) * t
    local z = z1 + (z2 - z1) * t
    
    -- Normalize
    local invLen = 1.0 / sqrt(w*w + x*x + y*y + z*z + 0.0001)
    return w * invLen, x * invLen, y * invLen, z * invLen
end

-- Helper: play random SFX from array
local function playRandomSFX(audio, clips)
    local count = clips and #clips or 0
    if count > 0 and audio then
        audio:PlayOneShot(clips[random(1, count)])
    end
end

return Component {
    mixins = { TransformMixin },

    fields = {
        name = "PlayerMovement",
        
        moveSpeed = 2.0,
        jumpSpeed = 5.0,
        gravity = -9.8,
        rotationSpeed = 10.0,

        attackDuration = 0.5,

        footstepSFXClips = {},
        jumpSFXClips = {},
        landingSFXClips = {},
        attackSFXClips = {},
        
        footstepInterval = 0.4,
        sfxVolume = 0.5,
    },

    Awake = function(self)
        -- Movement state
        self._velY = 0
        self._groundY = 0
        self._isGrounded = true
        
        -- Action states
        self._isWalking = false
        self._isJumping = false
        self._isAttacking = false
        
        self._idleAnimationClip = 0
        self._walkAnimationClip = 1
        self._jumpAnimationClip = 2
        self._attackAnimationClip = 3

        -- Timers
        self._footstepTimer = 0
        self._attackTimer = 0
        
        -- Previous movement input (packed into single value for efficiency)
        self._prevInput = 0  -- bits: W=1, S=2, A=4, D=8
    end,

    Start = function(self)
        -- Cache component references
        self._animator = self:GetComponent("AnimationComponent")
        self._audio = self:GetComponent("AudioComponent")
        
        if self._animator then
            self._animator.enabled = true
        end
        
        if self._audio then
            self._audio.enabled = true
            self._audio:SetVolume(self.sfxVolume)
        end
        
        -- Cache ground level
        local pos = getWorldPosition(self)
        self._groundY = pos.y

        -- initial broadcast
        if event_bus and event_bus.publish then
            event_bus.publish("player_position", {
                x = pos.x,
                y = pos.y,
                z = pos.z,
            })
        end
    end,

    Update = function(self, dt)
        --------------------------------------
        -- 1) Horizontal input (WASD)
        --------------------------------------
        local moveX, moveZ = 0.0, 0.0

        if Input.GetKey(Input.Key.W) then
            moveZ = moveZ + 1.0   -- forward (+Z)
        end
        if Input.GetKey(Input.Key.S) then
            moveZ = moveZ - 1.0   -- backward (-Z)
        end
        if Input.GetKey(Input.Key.A) then
            moveX = moveX + 1.0   -- left
        end
        if Input.GetKey(Input.Key.D) then
            moveX = moveX - 1.0   -- right
        end

        local len = math.sqrt(moveX * moveX + moveZ * moveZ)
        if len > 0.0001 then
            moveX = moveX / len
            moveZ = moveZ / len
        end

        local speed = self.moveSpeed or 5.0
        local dx = moveX * speed * dt
        local dz = moveZ * speed * dt

        --------------------------------------
        -- 2) Jump + simple gravity
        --------------------------------------
        local pos = getWorldPosition(self)

        -- NOTE: if this errors with nil, try Input.Key.SPACE
        if self._isGrounded and Input.GetKeyDown(Input.Key.Space) then
            self._velY       = self.jumpSpeed or 5.0
            self._isGrounded = false
            print("[LUA][PlayerMovement] Jump!")
        end

        if not self._isGrounded then
            self._velY = self._velY + (self.gravity or -20.0) * dt
        end

        local dy    = self._velY * dt
        local newY  = pos.y + dy

        if newY <= self._groundY then
            newY          = self._groundY
            self._velY    = 0.0
            self._isGrounded = true
        end

        --------------------------------------
        -- 3) Apply movement to Transform
        --------------------------------------
        local newX = pos.x + dx
        local newZ = pos.z + dz

        setWorldPosition(self, newX, newY, newZ)

        -- broadcast player world position for the camera
        if event_bus and event_bus.publish then
            local p = getWorldPosition(self)
            event_bus.publish("player_position", {
                x = p.x,
                y = p.y,
                z = p.z,
            })
        end
    end,

    OnDisable = function(self)
        -- Minimal cleanup
    end,
}
