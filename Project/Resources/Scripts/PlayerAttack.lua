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

        -- Step 1 timings
        attack1Duration  = 2.0,
        attack1HitStart  = 1.1,
        attack1HitEnd    = 1.4,

        -- Step 2 timings
        attack2Duration  = 2.0,
        attack2HitStart  = 1.1,
        attack2HitEnd    = 1.4,

        -- Step 3 timings
        attack3Duration  = 0.55,
        attack3HitStart  = 0.20,
        attack3HitEnd    = 0.40, 

        -- SFX clips (populate in editor)
        attackSFXClips = {},
    },

    ----------------------------------------------------------------------
    -- Lifecycle
    ----------------------------------------------------------------------
    Awake = function(self)
        print("[LUA][PlayerAttack] Awake")

        self._time             = 0.0
        self._attackTimer      = 0.0
        self._comboIndex       = 0         -- 0 = idle, 1..3 = which step
        self._maxComboSteps    = self.maxComboSteps or 3

        self._hitboxActive     = false
        self._queuedNextSwing  = false
        self._prevLeftDown     = false
        self._lastClickTime    = -999.0

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

        _G.player_is_attacking = false
    end,

    ----------------------------------------------------------------------
    -- Main update
    ----------------------------------------------------------------------
    Update = function(self, dt)
        self._time = self._time + dt

        -- ---- Input handling ----
        local leftDown    = false
        local leftPressed = false

        if Input and Input.GetMouseButton then
            leftDown = Input.GetMouseButton(Input.MouseButton.Left)
        end

        -- Edge detect: emulate GetMouseButtonDown
        leftPressed = (leftDown and not self._prevLeftDown)

        if leftPressed then
            self._lastClickTime = self._time
        end

        self._prevLeftDown = leftDown

        -- ---- Combo logic ----
        self:_updateCombo(dt, leftPressed)
    end,


    ----------------------------------------------------------------------
    -- Combo state machine
    ----------------------------------------------------------------------
    _updateCombo = function(self, dt, leftPressed)
        -- If no attack is active
        if self._comboIndex == 0 then
            if leftPressed then
                -- Start combo at step 1
                self:_startAttackStep(1)
            end
            return
        end

        -- There is an active attack step
        self._attackTimer = self._attackTimer + dt

        -- Queue next swing if player clicks during current step
        if leftPressed then
            self._queuedNextSwing = true
        end

        -- Update hitbox for this frame
        self:_updateHitbox(dt)

        -- Check if current step is finished
        local duration = self:_getAttackDuration(self._comboIndex)
        if self._attackTimer >= duration then
            if self._queuedNextSwing and self._comboIndex < self._maxComboSteps then
                -- Chain to next step
                self:_startAttackStep(self._comboIndex + 1)
            else
                -- Combo fully finished
                print("[LUA][PlayerAttack] Combo finished")
                self._comboIndex        = 0
                self._attackTimer       = 0.0
                self._queuedNextSwing   = false
                self._hitboxActive      = false
                self._currentHitEventId = nil

                _G.player_is_attacking = false
            end
        end
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
}
