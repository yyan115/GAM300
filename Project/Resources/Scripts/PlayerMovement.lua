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
        
        -- Initial position broadcast
        if event_bus and event_bus.publish then
            event_bus.publish("player_position", pos)
        end
    end,

    Update = function(self, dt)
        -- Cache frequently accessed values
        local audio = self._audio
        local animator = self._animator
        local isGrounded = self._isGrounded
        
        ----------------------------------------
        -- 1) Input processing
        ----------------------------------------
        local moveX, moveZ = 0, 0
        local currInput = 0
        
        if Input.GetKey(Input.Key.W) then moveZ = -1; currInput = currInput + 1 end
        if Input.GetKey(Input.Key.S) then moveZ = moveZ + 1; currInput = currInput + 2 end
        if Input.GetKey(Input.Key.A) then moveX = -1; currInput = currInput + 4 end
        if Input.GetKey(Input.Key.D) then moveX = moveX + 1; currInput = currInput + 8 end
        
        -- Detect new key press
        local prevInput = self._prevInput
        local newPress = currInput > 0 and currInput ~= prevInput
        self._prevInput = currInput
        
        -- Normalize movement
        local lenSq = moveX * moveX + moveZ * moveZ
        local hasInput = lenSq > 0.0001
        if hasInput then
            local invLen = 1.0 / sqrt(lenSq)
            moveX, moveZ = moveX * invLen, moveZ * invLen
        end
        
        local speed = self.moveSpeed
        local dx, dz = moveX * speed * dt, moveZ * speed * dt
        
        ----------------------------------------
        -- 2) Rotation (only when moving)
        ----------------------------------------
        if hasInput then
            local tw, tx, ty, tz = directionToQuaternion(moveX, moveZ)
            local cw, cx, cy, cz = self:GetRotation()
            if cw then
                local t = min(1.0, self.rotationSpeed * dt)
                self:SetRotation(lerpQuaternion(cw, cx, cy, cz, tw, tx, ty, tz, t))
            else
                self:SetRotation(tw, tx, ty, tz)
            end
        end
        
        ----------------------------------------
        -- 3) Walking state & animation
        ----------------------------------------
        local isWalking = hasInput and isGrounded
        local wasWalking = self._isWalking
        local isAttacking = self._isAttacking
        local isJumping = self._isJumping
        
        if not isAttacking and not isJumping then
            if isWalking and not wasWalking then
                if animator then animator:PlayClip(self._walkAnimationClip, true) end
                self._footstepTimer = 0
            elseif not isWalking and wasWalking then
                if animator then animator:PlayClip(self._idleAnimationClip, true) end
            end
        end
        self._isWalking = isWalking
        
        ----------------------------------------
        -- 4) Footstep SFX
        ----------------------------------------
        if isGrounded and audio then
            if newPress and hasInput then
                playRandomSFX(audio, self.footstepSFXClips)
                self._footstepTimer = 0
            elseif isWalking then
                self._footstepTimer = self._footstepTimer + dt
                if self._footstepTimer >= self.footstepInterval then
                    playRandomSFX(audio, self.footstepSFXClips)
                    self._footstepTimer = 0
                end
            end
        end
        
        ----------------------------------------
        -- 5) Jump & Gravity
        ----------------------------------------
        local pos = getWorldPosition(self)
        
        if isGrounded and Input.GetKeyDown(Input.Key.Space) then
            self._velY = self.jumpSpeed
            self._isGrounded = false
            self._isJumping = true
            isGrounded = false
            
            if animator and not isAttacking then
                animator:PlayOnce(self._jumpAnimationClip)
            end
            playRandomSFX(audio, self.jumpSFXClips)
        end
        
        if not isGrounded then
            self._velY = self._velY + self.gravity * dt
        end
        
        local newY = pos.y + self._velY * dt
        
        -- Landing check
        if newY <= self._groundY then
            newY = self._groundY
            self._velY = 0
            self._isGrounded = true
            
            if self._isJumping then
                self._isJumping = false
                playRandomSFX(audio, self.landingSFXClips)
                
                if not isAttacking and animator then
                    animator:PlayClip(isWalking and self._walkAnimationClip or self._idleAnimationClip, true)
                end
            end
        end
        
        ----------------------------------------
        -- 6) Attack
        ----------------------------------------
        if isAttacking then
            self._attackTimer = self._attackTimer - dt
            if self._attackTimer <= 0 then
                self._isAttacking = false
                if animator and self._isGrounded then
                    animator:PlayClip(self._isWalking and self._walkAnimationClip or self._idleAnimationClip, true)
                end
            end
        end
        
        if Input.GetMouseButtonDown(0) and not isAttacking then
            self._isAttacking = true
            self._attackTimer = self.attackDuration
            if animator then animator:PlayOnce(self._attackAnimationClip) end
            playRandomSFX(audio, self.attackSFXClips)
        end
        
        ----------------------------------------
        -- 7) Apply movement
        ----------------------------------------
        setWorldPosition(self, pos.x + dx, newY, pos.z + dz)
        
        -- Position broadcast
        if event_bus and event_bus.publish then
            event_bus.publish("player_position", getWorldPosition(self))
        end
    end,

    OnDisable = function(self)
        -- Minimal cleanup
    end,
}
