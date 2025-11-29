-- PlayerMovement.lua
-- Complete player movement with walking/jump animation, camera-relative rotation, and SFX

require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus
local Input = _G.Input

-- Reusable position table to avoid GC allocations
local _tempPos = { x = 0, y = 0, z = 0 }

--------------------------------------------------------------------------------
-- Helper Functions
--------------------------------------------------------------------------------

-- Get world position (optimized - reuses table)
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

-- Set world position
local function setWorldPosition(self, x, y, z)
    local ok = pcall(self.SetPosition, self, x, y, z)
    if not ok then
        pcall(function() self:SetPosition({ x = x, y = y, z = z }) end)
    end
end

-- Direction to quaternion (Y-axis rotation) - for facing movement direction
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

-- Quaternion lerp with normalization (for smooth rotation)
local function lerpQuaternion(w1, x1, y1, z1, w2, x2, y2, z2, t)
    -- Ensure shortest path
    if w1*w2 + x1*x2 + y1*y2 + z1*z2 < 0 then
        w2, x2, y2, z2 = -w2, -x2, -y2, -z2
    end
    
    -- Lerp components
    local w = w1 + (w2 - w1) * t
    local x = x1 + (x2 - x1) * t
    local y = y1 + (y2 - y1) * t
    local z = z1 + (z2 - z1) * t
    
    -- Normalize
    local invLen = 1.0 / math.sqrt(w*w + x*x + y*y + z*z + 0.0001)
    return w * invLen, x * invLen, y * invLen, z * invLen
end

-- Play random SFX from array
local function playRandomSFX(audio, clips)
    local count = clips and #clips or 0
    if count > 0 and audio then
        audio:PlayOneShot(clips[math.random(1, count)])
    end
end

--------------------------------------------------------------------------------
-- Component Definition
--------------------------------------------------------------------------------

return Component {
    mixins = { TransformMixin },

    ----------------------------------------------------------------------
    -- Inspector Fields
    ----------------------------------------------------------------------
    fields = {
        name = "PlayerMovement",
        
        -- Movement settings
        moveSpeed     = 2.0,
        jumpSpeed     = 5.0,
        gravity       = -9.8,
        rotationSpeed = 10.0,
        
        -- SFX clip arrays (populate in editor with audio GUIDs)
        footstepSFXClips = {},
        jumpSFXClips     = {},
        landingSFXClips  = {},
        
        -- SFX timing
        footstepInterval = 0.6,
        sfxVolume        = 0.5,
    },

    ----------------------------------------------------------------------
    -- Lifecycle: Awake
    ----------------------------------------------------------------------
    Awake = function(self)
        print("[LUA][PlayerMovement] Awake")
        
        -- Movement state
        self._velY       = 0
        self._groundY    = 0
        self._isGrounded = true
        self._wasGrounded = true  -- track previous frame for landing detection
        
        -- Animation state
        self._isWalking  = false
        self._isJumping  = false

                -- Animation clip indices (configurable in editor)
        self._idleAnimationClip = 0
        self._walkAnimationClip = 1
        self._jumpAnimationClip = 2
        
        -- Rotation state (quaternion: w, x, y, z)
        self._currentRotW = 1
        self._currentRotX = 0
        self._currentRotY = 0
        self._currentRotZ = 0
        
        -- Timers
        self._footstepTimer = 0
        
        -- Input tracking for key press detection
        self._prevInputW = false
        self._prevInputS = false
        self._prevInputA = false
        self._prevInputD = false
    end,

    ----------------------------------------------------------------------
    -- Lifecycle: Start
    ----------------------------------------------------------------------
    Start = function(self)
        print("[LUA][PlayerMovement] Start")
        
        -- Cache component references
        self._animator = self:GetComponent("AnimationComponent")
        self._audio    = self:GetComponent("AudioComponent")
        
        if self._animator then
            self._animator.enabled = true
            -- Start with idle animation
            self._animator:PlayClip(self._idleAnimationClip or 0, true)
        end
        
        if self._audio then
            self._audio.enabled = true
            self._audio:SetVolume(self.sfxVolume or 0.5)
        end
        
        -- Cache ground level from initial position
        local pos = getWorldPosition(self)
        self._groundY = pos.y

        -- Initial position broadcast for camera
        if event_bus and event_bus.publish then
            event_bus.publish("player_position", {
                x = pos.x,
                y = pos.y,
                z = pos.z,
            })
        end
    end,

    ----------------------------------------------------------------------
    -- Lifecycle: Update
    ----------------------------------------------------------------------
    Update = function(self, dt)
        -- Cache components locally for performance
        local animator = self._animator
        local audio    = self._audio
        
        --------------------------------------
        -- 1) Read WASD Input
        --------------------------------------
        local inputW = Input and Input.GetKey and Input.GetKey(Input.Key.W) or false
        local inputS = Input and Input.GetKey and Input.GetKey(Input.Key.S) or false
        local inputA = Input and Input.GetKey and Input.GetKey(Input.Key.A) or false
        local inputD = Input and Input.GetKey and Input.GetKey(Input.Key.D) or false
        
        -- Raw input direction (before camera transformation)
        local rawMoveX, rawMoveZ = 0.0, 0.0
        if inputW then rawMoveZ = rawMoveZ + 1.0 end  -- forward (+Z)
        if inputS then rawMoveZ = rawMoveZ - 1.0 end  -- backward (-Z)
        if inputA then rawMoveX = rawMoveX + 1.0 end  -- left (+X)
        if inputD then rawMoveX = rawMoveX - 1.0 end  -- right (-X)

        -- Normalize diagonal movement
        local len = math.sqrt(rawMoveX * rawMoveX + rawMoveZ * rawMoveZ)
        if len > 0.0001 then
            rawMoveX = rawMoveX / len
            rawMoveZ = rawMoveZ / len
        end
        
        local hasMovementInput = (len > 0.0001)
        
        -- Use raw movement direction (world-space WASD)
        local moveX, moveZ = rawMoveX, rawMoveZ

        local speed = self.moveSpeed or 5.0
        local dx = moveX * speed * dt
        local dz = moveZ * speed * dt

        local isMoving = (moveX ~= 0 or moveZ ~= 0)

        --------------------------------------
        -- 2) Detect Key Press for Footstep SFX
        --------------------------------------
        local keyJustPressed = (inputW and not self._prevInputW) or
                               (inputS and not self._prevInputS) or
                               (inputA and not self._prevInputA) or
                               (inputD and not self._prevInputD)
        
        self._prevInputW = inputW
        self._prevInputS = inputS
        self._prevInputA = inputA
        self._prevInputD = inputD

        --------------------------------------
        -- 3) Jump + Gravity
        --------------------------------------
        local pos = getWorldPosition(self)
        local wasInAir = not self._isGrounded
        self._wasGrounded = self._isGrounded

        -- Jump input (Space key)
        local jumpPressed = Input and Input.GetKeyDown and Input.GetKeyDown(Input.Key.Space)
        if self._isGrounded and jumpPressed then
            self._velY       = self.jumpSpeed or 5.0
            self._isGrounded = false
            self._isJumping  = true
            
            -- Play jump animation
            if animator then
                animator:PlayClip(self._jumpAnimationClip, false)
            end
            
            -- Play jump SFX
            playRandomSFX(audio, self.jumpSFXClips)
            
            print("[LUA][PlayerMovement] Jump!")
        end

        -- Apply gravity when in air
        if not self._isGrounded then
            self._velY = self._velY + (self.gravity) * dt
        end

        local dy   = self._velY * dt
        local newY = pos.y + dy

        -- Ground collision check
        if newY <= self._groundY then
            newY       = self._groundY
            self._velY = 0.0
            
            -- Landing detection
            if not self._isGrounded then
                self._isGrounded = true
                
                -- Play landing SFX if we were jumping
                if self._isJumping then
                    playRandomSFX(audio, self.landingSFXClips)
                    self._isJumping = false
                    print("[LUA][PlayerMovement] Landed!")
                    
                    -- Return to idle/walk animation after landing
                    if animator then
                        if hasMovementInput then
                            animator:PlayClip(self._walkAnimationClip or 1, true)
                        else
                            animator:PlayClip(self._idleAnimationClip or 0, true)
                        end
                    end
                end
            end
        end

        --------------------------------------
        -- 4) Apply Movement to Transform
        --------------------------------------
        local newX = pos.x + dx
        local newZ = pos.z + dz
        setWorldPosition(self, newX, newY, newZ)

        --------------------------------------
        -- 5) Rotation: Face Movement Direction
        --------------------------------------
        if hasMovementInput and self.SetRotation then
            -- Calculate target rotation quaternion (face the actual movement direction)
            local targetW, targetX, targetY, targetZ = directionToQuaternion(moveX, moveZ)
            
            -- Smooth rotation via lerp
            local t = math.min((self.rotationSpeed or 10.0) * dt, 1.0)
            local newW, newX2, newY2, newZ2 = lerpQuaternion(
                self._currentRotW, self._currentRotX, self._currentRotY, self._currentRotZ,
                targetW, targetX, targetY, targetZ,
                t
            )
            
            self._currentRotW = newW
            self._currentRotX = newX2
            self._currentRotY = newY2
            self._currentRotZ = newZ2
            
            -- Apply rotation (quaternion: w, x, y, z)
            pcall(self.SetRotation, self, newW, newX2, newY2, newZ2)
        end

        --------------------------------------
        -- 6) Walking Animation & Footstep SFX
        --------------------------------------
        local isWalkingNow = hasMovementInput and self._isGrounded

        -- Animation state change (only when grounded and not jumping)
        if isWalkingNow ~= self._isWalking and not self._isJumping then
            self._isWalking = isWalkingNow
            
            if animator then
                if isWalkingNow then
                    animator:PlayClip(self._walkAnimationClip or 1, true)
                    print("[LUA][PlayerMovement] Walking")
                else
                    animator:PlayClip(self._idleAnimationClip or 0, true)
                    print("[LUA][PlayerMovement] Idle")
                end
            end
        end

        -- Footstep SFX: play on key press + timer-based while walking
        if isWalkingNow then
            -- Play immediately on key press
            if keyJustPressed then
                playRandomSFX(audio, self.footstepSFXClips)
                self._footstepTimer = 0
            end
            
            -- Timer-based footsteps while holding key
            self._footstepTimer = self._footstepTimer + dt
            if self._footstepTimer >= (self.footstepInterval or 0.8) then
                playRandomSFX(audio, self.footstepSFXClips)
                self._footstepTimer = 0
            end
        else
            self._footstepTimer = 0
        end

        --------------------------------------
        -- 7) Broadcast Position for Camera
        --------------------------------------
        if event_bus and event_bus.publish then
            local p = getWorldPosition(self)
            event_bus.publish("player_position", {
                x = p.x,
                y = p.y,
                z = p.z,
            })
        end

    ----------------------------------------------------------------------
    -- Lifecycle: OnDisable
    ----------------------------------------------------------------------
    OnDisable = function(self)
        print("[LUA][PlayerMovement] OnDisable")
    end,
}
