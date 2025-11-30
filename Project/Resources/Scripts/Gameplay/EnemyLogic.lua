require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

--CONFIGURATIONS
local PLAYER_NAME = "Player"      -- Change this to match your player's name

-- Animation States
local IDLE          = 0
local ATTACK        = 1
local TAKE_DAMAGE   = 2
local DEATH         = 3

-- Play random SFX from array
local function playRandomSFX(audio, clips)
    local count = clips and #clips or 0
    if count > 0 and audio then
        audio:PlayOneShot(clips[math.random(1, count)])
    end
end

-- Helper function to create a rotation quaternion from Euler angles (degrees)
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

-- Helper: Convert quaternion to forward direction (Z-axis)
local function quatToForward(qw, qx, qy, qz)
    -- Forward vector from quaternion (assuming Z-forward)
    local fx = 2.0 * (qx * qz + qw * qy)
    local fy = 2.0 * (qy * qz - qw * qx)
    local fz = 1.0 - 2.0 * (qx * qx + qy * qy)
    return fx, fy, fz
end

return Component {
    mixins = { TransformMixin },

    fields = {
        Health          = 2,            -- Two hits to kill (first hit = hurt, second hit = death)
        Damage          = 1,
        Attack_Speed    = 1.0,
        Attack_Cooldown = 1.0,
        detectionRange  = 2.0,          -- Range to detect and attack player
        attackRange     = 2.5,          -- Range within which player can hit this enemy

        -- SFX clip arrays (populate in editor with audio GUIDs)
        attackSFXClips = {},
        hurtSFXClips = {},

        -- SFX settings
        sfxVolume        = 0.5,
        attackSoundInterval = 3.3,      -- Play attack sound every N seconds while attacking
    },

    Start = function(self)
        print("[EnemyLogic] Start - Caching components...")

        -- Cache components
        self._animator = self:GetComponent("AnimationComponent")
        self._collider = self:GetComponent("ColliderComponent")
        self._audio = self:GetComponent("AudioComponent")

        -- State tracking
        self._currentState = IDLE
        self._isDead = false
        self._hasRotated = false
        self._wasPlayerAttacking = false  -- Track attack state to detect new attacks

        -- SFX timers
        self._attackSoundTimer = 0.0

        print("[EnemyLogic] Animator:", self._animator)
        print("[EnemyLogic] Collider:", self._collider)
        print("[EnemyLogic] Audio:", self._audio)

        -- Initialize animator
        if self._animator then
            self._animator.enabled = true
            self._animator:PlayClip(IDLE, true)
            print("[EnemyLogic] Started IDLE animation")
        else
            print("[EnemyLogic ERROR] No AnimationComponent found!")
        end

        -- Initialize audio
        if self._audio then
            self._audio.enabled = true
            self._audio:SetVolume(self.sfxVolume or 0.5)
        end

        print("[EnemyLogic] Started - Idle state, waiting for player")
    end,

    -- Get enemy position helper
    _getEnemyPosition = function(self)
        if not self.GetPosition then return 0, 0, 0 end

        local ex, ey, ez = self:GetPosition()
        if type(ex) == "table" then
            return ex.x or 0, ex.y or 0, ex.z or 0
        end
        return ex or 0, ey or 0, ez or 0
    end,

    -- Get player position helper
    _getPlayerPosition = function(self)
        local playerTr = Engine.FindTransformByName(PLAYER_NAME)
        if not playerTr then return nil end

        local playerPos = Engine.GetTransformPosition(playerTr)
        if not playerPos then return nil end

        return playerPos[1] or 0, playerPos[2] or 0, playerPos[3] or 0
    end,

    -- Get player rotation (quaternion) helper
    _getPlayerRotation = function(self)
        local playerTr = Engine.FindTransformByName(PLAYER_NAME)
        if not playerTr then return nil end

        local playerRot = Engine.GetTransformRotation(playerTr)
        if not playerRot then return nil end

        -- Returns w, x, y, z
        return playerRot[1] or 1, playerRot[2] or 0, playerRot[3] or 0, playerRot[4] or 0
    end,

    -- Check if player is within detection range (for enemy to attack)
    _isPlayerInRange = function(self)
        local ex, ey, ez = self:_getEnemyPosition()
        local px, py, pz = self:_getPlayerPosition()
        if not px then return false end

        local dx = px - ex
        local dy = py - ey
        local dz = pz - ez
        local distance = math.sqrt(dx*dx + dy*dy + dz*dz)

        return distance < (self.detectionRange or 2.0)
    end,

    -- Check if player is close enough AND facing this enemy to hit it
    _canPlayerHitMe = function(self)
        local ex, ey, ez = self:_getEnemyPosition()
        local px, py, pz = self:_getPlayerPosition()
        if not px then return false end

        -- Check distance
        local dx = ex - px
        local dy = ey - py
        local dz = ez - pz
        local distance = math.sqrt(dx*dx + dy*dy + dz*dz)

        if distance > (self.attackRange or 2.5) then
            return false
        end

        -- Normalize direction from player to enemy
        local dirX = dx / distance
        local dirZ = dz / distance

        -- Get player's forward direction from rotation
        local qw, qx, qy, qz = self:_getPlayerRotation()
        if not qw then return false end

        local fx, fy, fz = quatToForward(qw, qx, qy, qz)

        -- Normalize forward (just XZ for horizontal facing)
        local fLen = math.sqrt(fx*fx + fz*fz)
        if fLen < 0.001 then return false end
        fx = fx / fLen
        fz = fz / fLen

        -- Dot product: how much is player facing the enemy
        local dot = dirX * fx + dirZ * fz

        -- Player is facing enemy if dot > 0.3 (roughly 70 degree cone in front)
        return dot > 0.3
    end,

    -- Rotate to face the player
    _rotateTowardsPlayer = function(self)
        if not self.SetRotation then return end

        local ex, _, ez = self:_getEnemyPosition()
        local px, _, pz = self:_getPlayerPosition()
        if not px then return end

        local dx = px - ex
        local dz = pz - ez

        local angle = math.deg(math.atan(dx, dz))
        local quat = eulerToQuat(0, angle, 0)

        self:SetRotation(quat.w, quat.x, quat.y, quat.z)
    end,

    -- Handle being hit
    _onHit = function(self)
        if self._isDead then return end

        self.Health = self.Health - 1
        print("[EnemyLogic] Hit by player attack! Health remaining: " .. self.Health)

        if self.Health <= 0 then
            -- Die
            self._isDead = true
            self._currentState = DEATH

            if self._animator then
                self._animator:PlayClip(DEATH, false)
            end

            print("[EnemyLogic] Enemy died!")
        else
            -- Take damage but survive - play hurt animation
            self._currentState = TAKE_DAMAGE

            if self._animator then
                self._animator:PlayClip(TAKE_DAMAGE, false)
            end

            -- Play hurt SFX
            playRandomSFX(self._audio, self.hurtSFXClips)

            print("[EnemyLogic] Enemy hurt! Playing hurt animation.")
        end
    end,

    -- Change to a new state with animation
    _changeState = function(self, newState)
        if newState == self._currentState then return end

        local oldState = self._currentState
        self._currentState = newState

        if self._animator then
            local loop = (newState == IDLE or newState == ATTACK)
            self._animator:PlayClip(newState, loop)
        end

        print("[EnemyLogic] State changed: " .. oldState .. " -> " .. newState)

        -- Play appropriate SFX when entering new state
        if newState == ATTACK then
            playRandomSFX(self._audio, self.attackSFXClips)
            self._attackSoundTimer = 0.0  -- Reset timer on state entry
        end
    end,

    Update = function(self, dt)
        -- If dead, do nothing
        if self._isDead then return end

        -- Safety check
        if not self._animator then return end

        -- Check for player attack (using global flag from PlayerAttack.lua)
        local playerAttacking = (_G.player_is_attacking == true)

        -- Detect rising edge: player just started attacking
        if playerAttacking and not self._wasPlayerAttacking then
            -- Player just started an attack, check if we get hit
            if self:_canPlayerHitMe() then
                self:_onHit()
            end
        end
        self._wasPlayerAttacking = playerAttacking

        -- If we died from the hit, stop here
        if self._isDead then return end

        local newState = self._currentState

        -- Handle state transitions
        if self._currentState == TAKE_DAMAGE then
            -- Check if damage animation is still playing
            if not self._animator.isPlay then
                if self:_isPlayerInRange() then
                    newState = ATTACK
                else
                    newState = IDLE
                end
            end
        elseif self._currentState == ATTACK then
            if not self:_isPlayerInRange() then
                newState = IDLE
                self._hasRotated = false
            end
        elseif self._currentState == IDLE then
            if self:_isPlayerInRange() then
                newState = ATTACK
            end
        end

        -- Handle rotation when attacking
        if newState == ATTACK and not self._hasRotated then
            self:_rotateTowardsPlayer()
            self._hasRotated = true
        end

        if newState ~= ATTACK then
            self._hasRotated = false
        end

        -- Apply state change
        if newState ~= self._currentState then
            self:_changeState(newState)
        end

        -- Play periodic attack sounds while in ATTACK state
        if self._currentState == ATTACK then
            self._attackSoundTimer = self._attackSoundTimer + dt
            if self._attackSoundTimer >= (self.attackSoundInterval or 3.3) then
                playRandomSFX(self._audio, self.attackSFXClips)
                self._attackSoundTimer = 0.0
            end
        end
    end
}

--  Animation clip indices:
--  0 = IDLE
--  1 = ATTACK
--  2 = TAKE_DAMAGE
--  3 = DEATH
