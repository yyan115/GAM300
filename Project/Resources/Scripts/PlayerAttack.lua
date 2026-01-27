-- PlayerAttack.lua
require("extension.engine_bootstrap")
local Component      = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus
local Input     = _G.Input
_G.player_is_attacking = _G.player_is_attacking or false

local function clamp(x, minv, maxv)
    if x < minv then return minv end
    if x > maxv then return maxv end
    return x
end

return Component {
    mixins = { TransformMixin },

    ----------------------------------------------------------------------
    -- Inspector fields
    ----------------------------------------------------------------------
    fields = {
        -- Damage / hitbox
        damage        = 10,
        hitboxRadius  = 1.5,
        hitboxForward = 1.5,   -- how far in front of player the hitbox is

        -- Combo config
        maxComboSteps   = 3,
        comboResetTime  = 1.3, -- currently not heavily used but kept for tuning
        attackCooldown  = 2.0, -- minimum time between attacks to prevent spam

        -- Step 1 timings
        attack1Duration  = 2.0,
        attack1HitStart  = 1.4,
        attack1HitEnd    = 1.7,

        -- Step 2 timings
        attack2Duration  = 2.5,
        attack2HitStart  = 1.7,
        attack2HitEnd    = 2.0,

        -- Step 3 timings
        attack3Duration  = 1.8,
        attack3HitStart  = 1.0,
        attack3HitEnd    = 1.3, 

        -- SFX clips (populate in editor)
        attackSFXClips = {},

        -- Chain attack config (Right Mouse)
        chainDuration     = 4.6,   -- total time for full chain cycle (throw + pull)
        chainPullDelay    = 2.45,   -- time after throw before we pull back (your requirement)
        chainCooldown     = 3.0,   -- separate cooldown for chain
        chainRange        = 6.0,   -- how far the chain travels (for VFX / hit logic)
        chainAnimThrowClip = 6,    -- animation index for throwing chain (set in editor)
        chainAnimPullClip  = 7,    -- animation index for pulling chain back (set in editor)
        chainSFXClips     = {},    -- optional: separate SFX for chain
    },

    ----------------------------------------------------------------------
    -- Lifecycle
    ----------------------------------------------------------------------
    Awake = function(self)
        print("[LUA][PlayerAttack] Awake")

        self._time             = 0.0
        self._attackTimer      = 0.0
        self._cooldownTimer    = 0.0  -- prevents spam clicking
        self._comboIndex       = 0         -- 0 = idle, 1..3 = which step
        self._maxComboSteps    = self.maxComboSteps

        self._hitboxActive     = false
        self._queuedNextSwing  = false
        self._prevLeftDown     = false
        self._lastClickTime    = -999.0

        -- Chain attack state (right-click)
        self._isChainAttack      = false
        self._chainState         = "idle"   -- "idle", "flying", "returning"
        self._chainTimer         = 0.0
        self._chainCooldownTimer = 0.0

        self._prevRightDown      = false


        -- Unique IDs so each swing only hits an enemy once
        self._hitEventIdCounter = 0
        self._currentHitEventId = nil

        -- Animation clip indices
         self._attackAnimClip1 = 3
         self._attackAnimClip2 = 4
         self._attackAnimClip3 = 5

        -- Cache components
        self._animator = nil
        self._audio    = nil
    end,

    Start = function(self)
        self._animator = self:GetComponent("AnimationComponent")
        self._audio    = self:GetComponent("AudioComponent")
    end,

    OnDisable = function(self)
        -- Clean up state when disabled
        self._hitboxActive      = false
        self._currentHitEventId = nil
        self._comboIndex        = 0
        self._cooldownTimer     = 0.0

        _G.player_is_attacking = false
    end,

    ----------------------------------------------------------------------
    -- Main update
    ----------------------------------------------------------------------
        Update = function(self, dt)
        self._time = self._time + dt

        -- ---- Input handling (unified input system) ----
        local leftDown,  leftPressed  = false, false
        local rightDown, rightPressed = false, false

        if Input and Input.IsActionPressed then
            leftDown  = Input.IsActionPressed("Attack")
            rightDown = Input.IsActionPressed("ChainAttack")
        end

        -- Use IsActionJustPressed for edge detection
        if Input and Input.IsActionJustPressed then
            leftPressed  = Input.IsActionJustPressed("Attack")
            rightPressed = Input.IsActionJustPressed("ChainAttack")
        end

        if leftPressed then
            self._lastClickTime = self._time
        end

        self._prevLeftDown  = leftDown
        self._prevRightDown = rightDown

        -- ---- Chain attack (right click) ----
        self:_updateChainAttack(dt, rightPressed)

        -- If chain is active, we don't allow normal combo attacks
        if not self._isChainAttack then
            -- ---- Combo logic (left click) ----
            self:_updateCombo(dt, leftPressed)
        end
    end,



    ----------------------------------------------------------------------
    -- Combo state machine
    ----------------------------------------------------------------------
    _updateCombo = function(self, dt, leftPressed)
        -- Update cooldown timer
        if self._cooldownTimer > 0 then
            self._cooldownTimer = self._cooldownTimer - dt
            if self._cooldownTimer < 0 then
                self._cooldownTimer = 0
            end
        end

        -- If no attack is active
        if self._comboIndex == 0 then
            -- Only start new attack if cooldown is over and player clicked
            if leftPressed and self._cooldownTimer <= 0 then
                self:_startAttackStep(1)
            end
            return
        end

        -- There is an active attack step
        self._attackTimer = self._attackTimer + dt

        -- Only allow queuing the next swing while the hit is actually active
        local hitStart, hitEnd = self:_getHitWindow(self._comboIndex)
        local t = self._attackTimer

        local canQueueNext =
            (t >= hitStart) and
            (t <= hitEnd) and
            (not self._queuedNextSwing) and
            (self._comboIndex < self._maxComboSteps)

        if leftPressed and canQueueNext then
            self._queuedNextSwing = true
            print("[LUA][PlayerAttack] Queued next swing (step " .. tostring(self._comboIndex + 1) .. ")")
        end

        -- Update hitbox for this frame
        self:_updateHitbox(dt)

        -- Check if current step is finished (animation duration completed)
        local duration = self:_getAttackDuration(self._comboIndex)
        if self._attackTimer >= duration then
            if self._queuedNextSwing and self._comboIndex < self._maxComboSteps then
                -- Chain to next step - only NOW play the next animation/SFX
                self:_startAttackStep(self._comboIndex + 1)
            else
                -- Combo fully finished - start cooldown to prevent spam
                print("[LUA][PlayerAttack] Combo finished")
                self._comboIndex        = 0
                self._attackTimer       = 0.0
                self._queuedNextSwing   = false
                self._hitboxActive      = false
                self._currentHitEventId = nil
                self._cooldownTimer     = self.attackCooldown or 3.0

                _G.player_is_attacking = false
            end
        end
    end,

    ----------------------------------------------------------------------
    -- Chain attack (right-click): throw chain, then pull back after delay
    ----------------------------------------------------------------------
    _updateChainAttack = function(self, dt, rightPressed)
        -- Update chain cooldown
        if self._chainCooldownTimer and self._chainCooldownTimer > 0 then
            self._chainCooldownTimer = self._chainCooldownTimer - dt
            if self._chainCooldownTimer < 0 then
                self._chainCooldownTimer = 0
            end
        end

        -- If no chain attack currently active, check for starting one
        if not self._isChainAttack then
            -- Only start if:
            --  - right click pressed
            --  - chain cooldown ready
            --  - normal melee combo not currently happening
            if rightPressed
                and (self._chainCooldownTimer or 0) <= 0
                and self._comboIndex == 0
            then
                self:_startChainAttack()
            end
            return
        end

        -- Chain is active
        self._chainTimer = self._chainTimer + dt

        local pullDelay    = self.chainPullDelay or 2.0
        local totalDuration = self.chainDuration or (pullDelay + 0.3)

        -- After pullDelay seconds, start pulling the chain back
        if self._chainState == "flying" and self._chainTimer >= pullDelay then
            self._chainState = "returning"
            self:_playChainAnimation("pull")
            self:_fireChainPullEvent()
            print("[LUA][PlayerAttack] Chain pull-back started")
        end

        -- End of chain attack
        if self._chainTimer >= totalDuration then
            print("[LUA][PlayerAttack] Chain attack finished")

            self._isChainAttack      = false
            self._chainState         = "idle"
            self._chainTimer         = 0.0
            self._chainCooldownTimer = self.chainCooldown or 3.0

            _G.player_is_attacking = false
        end
    end,

    _startChainAttack = function(self)
        print("[LUA][PlayerAttack] Chain attack started")

        self._isChainAttack = true
        self._chainState    = "flying"
        self._chainTimer    = 0.0

        _G.player_is_attacking = true

        self:_playChainAnimation("throw")
        self:_playChainSFX()
        self:_fireChainThrowEvent()
    end,

    _startAttackStep = function(self, stepIndex)
        self._comboIndex      = stepIndex
        self._attackTimer     = 0.0
        self._queuedNextSwing = false
        self._hitboxActive    = false

        -- New unique ID for this swing
        self._hitEventIdCounter = (self._hitEventIdCounter or 0) + 1
        self._currentHitEventId = self._hitEventIdCounter

        print("[LUA][PlayerAttack] Start attack step " .. tostring(stepIndex))

        _G.player_is_attacking = true

        -- 🔧 FIXED: use ':' not '->'
        self:_playSlashAnimation(stepIndex)
        self:_playAttackSFX()
    end,

    ----------------------------------------------------------------------
    -- Timings
    ----------------------------------------------------------------------
    _getAttackDuration = function(self, stepIndex)
        if stepIndex == 1 then
            return self.attack1Duration or 0.40
        elseif stepIndex == 2 then
            return self.attack2Duration or 0.45
        elseif stepIndex == 3 then
            return self.attack3Duration or 0.55
        end
        return 0.40
    end,

    _getHitWindow = function(self, stepIndex)
        if stepIndex == 1 then
            return (self.attack1HitStart or 0.15), (self.attack1HitEnd or 0.30)
        elseif stepIndex == 2 then
            return (self.attack2HitStart or 0.18), (self.attack2HitEnd or 0.35)
        elseif stepIndex == 3 then
            return (self.attack3HitStart or 0.20), (self.attack3HitEnd or 0.40)
        end
        return 0.0, 0.0
    end,

    ----------------------------------------------------------------------
    -- Hitbox handling
    ----------------------------------------------------------------------
    _updateHitbox = function(self, dt)
        if self._comboIndex == 0 then
            self._hitboxActive = false
            return
        end

        local hitStart, hitEnd = self:_getHitWindow(self._comboIndex)
        local t = self._attackTimer

        local activeNow = (t >= hitStart and t <= hitEnd)

        if activeNow and not self._hitboxActive then
            self._hitboxActive = true
            print("[LUA][PlayerAttack] Hitbox ON (step " .. tostring(self._comboIndex) .. ")")
        elseif not activeNow and self._hitboxActive then
            self._hitboxActive = false
            print("[LUA][PlayerAttack] Hitbox OFF (step " .. tostring(self._comboIndex) .. ")")
            return
        end

        if not activeNow then
            return
        end

        if not (self.GetPosition and self.GetRotation) then
            return
        end

        local px, py, pz = self:GetPosition()
        local rx, ry, rz = self:GetRotation()  -- assumed Euler degrees (x, y, z)

        local yawRad   = math.rad(ry or 0.0)
        local forwardX = math.sin(yawRad)
        local forwardZ = math.cos(yawRad)

        local forwardDist = self.hitboxForward or 1.5
        local centerX     = px + forwardX * forwardDist
        local centerY     = py
        local centerZ     = pz + forwardZ * forwardDist

        local radius = self.hitboxRadius or 1.5

        self:_fireHitboxEvent(centerX, centerY, centerZ, radius)
    end,

    _fireHitboxEvent = function(self, centerX, centerY, centerZ, radius)
        if not event_bus or not event_bus.publish then
            return
        end

        local payload = {
            center = { x = centerX, y = centerY, z = centerZ },
            radius = radius,
            damage = self.damage or 10,
            hitId  = self._currentHitEventId, -- same for entire swing
        }

        event_bus.publish("player_attack_hitbox", payload)
    end,

        _fireChainThrowEvent = function(self)
        if not (event_bus and event_bus.publish) then return end
        if not (self.GetPosition and self.GetRotation) then return end

        local px, py, pz = self:GetPosition()
        local rx, ry, rz = self:GetRotation()  -- assume y is yaw in degrees
        local yawRad     = math.rad(ry or 0.0)

        local forwardX = math.sin(yawRad)
        local forwardZ = math.cos(yawRad)

        local payload = {
            origin    = { x = px, y = py, z = pz },
            direction = { x = forwardX, y = 0.0, z = forwardZ },
            range     = self.chainRange or 6.0,
        }

        event_bus.publish("player_chain_throw", payload)
    end,

    _fireChainPullEvent = function(self)
        if not (event_bus and event_bus.publish) then return end

        -- You can expand this with more data if needed (e.g., hooked enemy info)
        event_bus.publish("player_chain_pull", {})
    end,


    ----------------------------------------------------------------------
    -- Animation hook
    ----------------------------------------------------------------------
    _playSlashAnimation = function(self, stepIndex)
        local animator = self._animator
        if not animator then return end

        local clipIndex = 3  -- default
        if stepIndex == 1 then
            clipIndex = self._attackAnimClip1 or 3
        elseif stepIndex == 2 then
            clipIndex = self._attackAnimClip2 or 4
        elseif stepIndex == 3 then
            clipIndex = self._attackAnimClip3 or 5
        end

        print("[LUA][PlayerAttack] Play animation clip " .. tostring(clipIndex))
        animator:PlayClip(clipIndex, false)
    end,

    ----------------------------------------------------------------------
    -- SFX hook
    ----------------------------------------------------------------------
    _playAttackSFX = function(self)
        local audio = self._audio
        local clips = self.attackSFXClips
        if not audio or not clips or #clips == 0 then return end

        local clipIndex = math.random(1, #clips)
        audio:PlayOneShot(clips[clipIndex])
    end,

    _playChainAnimation = function(self, phase)
        local animator = self._animator
        if not animator then return end

        local clipIndex = nil

        if phase == "throw" then
            clipIndex = self.chainAnimThrowClip
        elseif phase == "pull" then
            clipIndex = self.chainAnimPullClip
        end

        if not clipIndex then return end

        print("[LUA][PlayerAttack] Play chain animation (" .. tostring(phase) ..
              ") clip " .. tostring(clipIndex))
        animator:PlayClip(clipIndex, false)
    end,

    _playChainSFX = function(self)
        local audio = self._audio
        local clips = self.chainSFXClips or self.attackSFXClips
        if not audio or not clips or #clips == 0 then return end

        local idx = math.random(1, #clips)
        audio:PlayOneShot(clips[idx])
    end,

}
