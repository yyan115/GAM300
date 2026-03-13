--[[
================================================================================
CAMERA EFFECTS
================================================================================
PURPOSE:
    Drives all camera post-processing and time dilation via events.
    Attach to the same entity as CameraComponent.

SINGLE RESPONSIBILITY: Apply visual effects. Nothing else.

EFFECTS MANAGED:
    Vignette, chromatic aberration, blur, motion blur, time scale.

ALL timers use Time.GetUnscaledDeltaTime() so effects survive time dilation.

EVENTS CONSUMED:
    fx_vignette            { intensity, smoothness?, duration? }                      nil duration = hold
    fx_chromatic           { intensity, duration? }                                   nil duration = hold
    fx_blur                { intensity, radius?, duration? }                          nil duration = hold
    fx_motion_blur         { intensity, angle? }                                      → driven each frame by camera_follow; angle = movement direction in degrees
    fx_color_grading       { brightness?, contrast?, saturation?, tint?, duration? }  nil duration = hold
    fx_time_scale          { scale, duration? }                                       nil duration = hold
    fx_vignette_clear      {}
    fx_chromatic_clear     {}
    fx_blur_clear          {}
    fx_color_grading_clear {}
    fx_time_scale_clear    {}
    fx_clear_all           {}    → cancel every active effect immediately
    fx_sequence            { name }  → run a named scripted sequence from SEQUENCES
    fx_sequence_cancel     {}        → abort the currently running sequence
    dodge_success          { attackType, payload }  → chromatic spike + freeze-frame slow-mo
    vault_jump             {}                        → chromatic spike + freeze-frame slow-mo on confirmed vault

EVENTS PUBLISHED:
    fx_time_scale_restored  {}  → fires when time scale returns to 1.0
    fx_sequence_done        { name }  → fires when a sequence reaches its last step

-- TO ADD a new effect:
--   1. Add fields under the relevant === section === in fields
--   2. Subscribe to its trigger event in Awake
--   3. Init current/target state in Start
--   4. Add an update block in Update that lerps current → target each frame
--   5. Write current value to CameraComponent property
--   6. Add its subscription handle to OnDisable's subs list

-- TO ADD a new scripted sequence:
--   1. Add a new key to the SEQUENCES table above the Component definition
--   2. Each step: { delay = <seconds after previous step>, event = "<event>", params = { … } }
--   3. Fire it at runtime via: event_bus.publish("fx_sequence", { name = "your_key" })

AUTHOR: Soh Wei Jie
VERSION: 1.1
================================================================================
--]]

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local event_bus = _G.event_bus

-- =============================================================================
-- SCRIPTED SEQUENCES
-- =============================================================================
-- Each entry is a named sequence of effect steps fired in order.
-- delay  = seconds to wait after the previous step before firing this one
--          (first step delay is seconds after the sequence is triggered)
-- event  = event_bus event name to publish
-- params = payload table forwarded to that event
--
-- Add new sequences here; trigger them with:
--   event_bus.publish("fx_sequence", { name = "sequence_key" })
-- =============================================================================
local SEQUENCES = {

    -- Fired when the player dies: freeze time → heavy vignette → chromatic burst
    death_cam = {
        { delay = 0.00, event = "fx_time_scale", params = { scale = 0.10, duration = 2.0 } },
        { delay = 0.05, event = "fx_vignette",   params = { intensity = 0.92, smoothness = 0.3, duration = 2.2 } },
        { delay = 0.08, event = "fx_chromatic",  params = { intensity = 0.85, duration = 1.8 } },
    },

    -- Short hit-impact flash: chromatic spike then vignette pulse
    hit_impact = {
        { delay = 0.00, event = "fx_chromatic", params = { intensity = 0.65, duration = 0.25 } },
        { delay = 0.04, event = "fx_vignette",  params = { intensity = 0.78, duration = 0.35 } },
    },

    -- Cinematic focus-in: blur fades out while vignette tightens, then slow-mo
    focus_in = {
        { delay = 0.00, event = "fx_blur",       params = { intensity = 0.8, radius = 4.0, duration = 0.6 } },
        { delay = 0.10, event = "fx_vignette",   params = { intensity = 0.80, smoothness = 0.25, duration = 0.8 } },
        { delay = 0.20, event = "fx_time_scale", params = { scale = 0.35, duration = 1.2 } },
    },
}

return Component {

    fields = {
        -- === Vignette ===
        BaselineVignetteIntensity  = 0.25,   -- always-on base intensity; events push above this
        BaselineVignetteSmoothness = 0.5,
        VignetteFadeSpeed          = 6.0,    -- lerp speed back to baseline

        -- === Chromatic Aberration ===
        ChromaticFadeSpeed = 8.0,

        -- === Blur ===
        BlurFadeSpeed     = 6.0,
        DefaultBlurRadius = 2.0,

        -- === Motion Blur ===
        -- Uses directional blur (dirBlur*), NOT gaussian blur, for correct motion-blur look.
        -- camera_follow should publish fx_motion_blur { intensity=0-1, angle=degrees }
        -- where angle is the camera movement direction (0=right, 90=up, etc).
        MotionBlurEnabled      = true,   -- master toggle; set false to disable motion blur entirely.
        MotionBlurMaxIntensity = 0.25,   -- peak dirBlurIntensity at full camera speed (0=off, 1=max).
        MotionBlurMaxStrength  = 3.5,    -- peak dirBlurStrength (pixel spread) at full camera speed.
        MotionBlurFadeSpeed    = 12.0,   -- lerp speed for motion blur response. Higher = snappier onset and decay.
        MotionBlurSamples      = 8,      -- dirBlurSamples quality (4–16).

        -- === Color Grading ===
        -- Baseline values always applied; fx_color_grading events push above/below these.
        BaselineCgBrightness = 0.0,    -- additive brightness offset. 0 = neutral.
        BaselineCgContrast   = 1.0,    -- contrast multiplier. 1 = neutral.
        BaselineCgSaturation = 1.0,    -- saturation multiplier. 1 = neutral, 0 = greyscale.
        BaselineCgTintR      = 1.0,    -- tint red channel. 1 = neutral.
        BaselineCgTintG      = 1.0,    -- tint green channel. 1 = neutral.
        BaselineCgTintB      = 1.0,    -- tint blue channel. 1 = neutral.
        ColorGradingFadeSpeed = 5.0,   -- lerp speed back to baseline after a timed event.

        -- === Time Scale ===
        TimeScaleRestoreSpeed = 4.0,   -- how fast time lerps back to 1.0 after a timed slow-mo

        -- === Dodge Success ===
        -- Chromatic spike + freeze-frame slow-mo that fire together on a confirmed dodge.
        -- The time scale briefly dips then snaps back, giving a "bullet-time parry" feel.
        DodgeChromaticIntensity = 0.90,  -- Chromatic aberration intensity at peak.
        DodgeChromaticDuration  = 0.35,  -- Seconds the chromatic spike is held before fading.
        DodgeTimeScale          = 0.15,  -- Time scale during the freeze-frame dip. Lower = more dramatic.
        DodgeTimeScaleDuration  = 0.45,  -- Seconds the time dilation is held before restoring.

        -- === Vault Jump ===
        -- Chromatic spike + freeze-frame slow-mo on a confirmed vault jump.
        -- Independent from Dodge Success — tune each separately to match their feel.
        VaultChromaticIntensity = 0.90,  -- Chromatic aberration intensity at peak.
        VaultChromaticDuration  = 0.35,  -- Seconds the chromatic spike is held before fading.
        VaultTimeScale          = 0.15,  -- Time scale during the freeze-frame dip. Lower = more dramatic.
        VaultTimeScaleDuration  = 0.45,  -- Seconds the time dilation is held before restoring.

        -- === Defaults ===
        DefaultVignetteSmoothness = 0.4,
    },

    -- ==========================================================================
    -- AWAKE
    -- ==========================================================================
    Awake = function(self)
        if not (event_bus and event_bus.subscribe) then
            print("[CameraEffects] ERROR: event_bus not available")
            return
        end

        -- === Vignette ===
        self._vignetteSub = event_bus.subscribe("fx_vignette", function(p)
            if not p then return end
            self._vignetteTarget       = p.intensity  or 0.6
            self._vignetteSmoothTarget = p.smoothness or self.DefaultVignetteSmoothness
            self._vignetteDuration     = p.duration
            self._vignetteTimer        = 0
            self._vignetteHeld         = (p.duration == nil)
        end)

        self._vignetteClearSub = event_bus.subscribe("fx_vignette_clear", function()
            self._vignetteTarget = self.BaselineVignetteIntensity
            self._vignetteHeld   = false
        end)

        -- === Chromatic Aberration ===
        self._chromaticSub = event_bus.subscribe("fx_chromatic", function(p)
            if not p then return end
            self._chromaticTarget   = p.intensity or 0.5
            self._chromaticDuration = p.duration
            self._chromaticTimer    = 0
            self._chromaticHeld     = (p.duration == nil)
        end)

        self._chromaticClearSub = event_bus.subscribe("fx_chromatic_clear", function()
            self._chromaticTarget = 0
            self._chromaticHeld   = false
        end)

        -- === Blur ===
        self._blurSub = event_bus.subscribe("fx_blur", function(p)
            if not p then return end
            self._blurTarget    = p.intensity or 0.5
            self._blurRadTarget = p.radius    or self.DefaultBlurRadius
            self._blurDuration  = p.duration
            self._blurTimer     = 0
            self._blurHeld      = (p.duration == nil)
        end)

        self._blurClearSub = event_bus.subscribe("fx_blur_clear", function()
            self._blurTarget = 0
            self._blurHeld   = false
        end)

        -- === Time Scale ===
        self._timeScaleSub = event_bus.subscribe("fx_time_scale", function(p)
            if not p then return end
            self._timeScaleTarget   = p.scale    or 0.3
            self._timeScaleDuration = p.duration
            self._timeScaleTimer    = 0
            self._timeScaleHeld     = (p.duration == nil)
            Time.SetTimeScale(self._timeScaleTarget)
        end)

        self._timeScaleClearSub = event_bus.subscribe("fx_time_scale_clear", function()
            self._timeScaleTarget = 1.0
            self._timeScaleHeld   = false
        end)

        -- === Motion Blur ===
        -- Driven every frame by camera_follow.
        -- intensity is pre-normalised (0-1); angle is movement direction in degrees.
        self._motionBlurSub = event_bus.subscribe("fx_motion_blur", function(p)
            if not p then return end
            self._motionBlurTarget       = (p.intensity or 0) * (self.MotionBlurMaxIntensity or 0.6)
            self._motionBlurStrengthTarget = (p.intensity or 0) * (self.MotionBlurMaxStrength or 8.0)
            if p.angle ~= nil then
                self._motionBlurAngle = p.angle
            end
        end)

        -- === Color Grading ===
        self._colorGradingSub = event_bus.subscribe("fx_color_grading", function(p)
            if not p then return end
            self._cgBrightnessTarget = p.brightness  ~= nil and p.brightness  or self.BaselineCgBrightness
            self._cgContrastTarget   = p.contrast    ~= nil and p.contrast    or self.BaselineCgContrast
            self._cgSaturationTarget = p.saturation  ~= nil and p.saturation  or self.BaselineCgSaturation
            if p.tint then
                self._cgTintTargetR = p.tint[1] or self.BaselineCgTintR
                self._cgTintTargetG = p.tint[2] or self.BaselineCgTintG
                self._cgTintTargetB = p.tint[3] or self.BaselineCgTintB
            end
            self._cgDuration = p.duration
            self._cgTimer    = 0
            self._cgHeld     = (p.duration == nil)
        end)

        self._colorGradingClearSub = event_bus.subscribe("fx_color_grading_clear", function()
            self._cgBrightnessTarget = self.BaselineCgBrightness
            self._cgContrastTarget   = self.BaselineCgContrast
            self._cgSaturationTarget = self.BaselineCgSaturation
            self._cgTintTargetR      = self.BaselineCgTintR
            self._cgTintTargetG      = self.BaselineCgTintG
            self._cgTintTargetB      = self.BaselineCgTintB
            self._cgHeld             = false
        end)

        -- === Clear All ===
        self._clearAllSub = event_bus.subscribe("fx_clear_all", function()
            self._vignetteTarget     = self.BaselineVignetteIntensity; self._vignetteHeld  = false
            self._chromaticTarget    = 0;                              self._chromaticHeld = false
            self._blurTarget         = 0;                              self._blurHeld      = false
            self._timeScaleTarget    = 1.0;                            self._timeScaleHeld = false
            self._motionBlurTarget         = 0
            self._motionBlurStrengthTarget = 0
            self._cgBrightnessTarget = self.BaselineCgBrightness
            self._cgContrastTarget   = self.BaselineCgContrast
            self._cgSaturationTarget = self.BaselineCgSaturation
            self._cgTintTargetR      = self.BaselineCgTintR
            self._cgTintTargetG      = self.BaselineCgTintG
            self._cgTintTargetB      = self.BaselineCgTintB
            self._cgHeld             = false
        end)

        -- === Dodge Success ===
        -- Fires a chromatic spike and a freeze-frame time dip simultaneously.
        -- Both durations are intentionally short — the effect should feel instant,
        -- not linger. Tune DodgeTimeScale closer to 1.0 to soften the dip.
        self._dodgeSuccessSub = event_bus.subscribe("dodge_success", function()
            self._chromaticTarget   = self.DodgeChromaticIntensity or 0.75
            self._chromaticDuration = self.DodgeChromaticDuration  or 0.35
            self._chromaticTimer    = 0
            self._chromaticHeld     = false

            self._timeScaleTarget   = self.DodgeTimeScale          or 0.15
            self._timeScaleDuration = self.DodgeTimeScaleDuration  or 0.12
            self._timeScaleTimer    = 0
            self._timeScaleHeld     = false
            Time.SetTimeScale(self._timeScaleTarget)
        end)

        -- === Vault Jump ===
        -- Fires a chromatic spike and a freeze-frame time dip on a confirmed vault jump.
        -- Tune VaultTimeScale closer to 1.0 to soften the dip.
        self._vaultJumpSub = event_bus.subscribe("vault_jump", function()
            self._chromaticTarget   = self.VaultChromaticIntensity or 0.90
            self._chromaticDuration = self.VaultChromaticDuration  or 0.35
            self._chromaticTimer    = 0
            self._chromaticHeld     = false

            self._timeScaleTarget   = self.VaultTimeScale          or 0.15
            self._timeScaleDuration = self.VaultTimeScaleDuration  or 0.45
            self._timeScaleTimer    = 0
            self._timeScaleHeld     = false
            Time.SetTimeScale(self._timeScaleTarget)
        end)

        -- === Scripted Sequences ===
        self._sequenceSub = event_bus.subscribe("fx_sequence", function(p)
            if not p or not p.name then return end
            local seq = SEQUENCES[p.name]
            if not seq then
                print("[CameraEffects] fx_sequence: unknown sequence '" .. tostring(p.name) .. "'")
                return
            end
            self._seqSteps       = seq
            self._seqName        = p.name
            self._seqStep        = 1
            self._seqTimer       = 0
            self._seqRunning     = true
        end)

        self._sequenceCancelSub = event_bus.subscribe("fx_sequence_cancel", function()
            self._seqRunning = false
            self._seqSteps   = nil
            self._seqName    = nil
        end)
    end,

    -- ==========================================================================
    -- START
    -- ==========================================================================
    Start = function(self)
        self._camera = self:GetComponent("CameraComponent")
        if not self._camera then
            print("[CameraEffects] ERROR: CameraComponent not found on this entity")
            return
        end

        -- === Vignette state ===
        self._vignetteCurrent       = self.BaselineVignetteIntensity
        self._vignetteTarget        = self.BaselineVignetteIntensity
        self._vignetteSmoothCurrent = self.BaselineVignetteSmoothness
        self._vignetteSmoothTarget  = self.BaselineVignetteSmoothness
        self._vignetteDuration      = nil
        self._vignetteTimer         = 0
        self._vignetteHeld          = false

        self._camera.vignetteEnabled    = true
        self._camera.vignetteIntensity  = self._vignetteCurrent
        self._camera.vignetteSmoothness = self._vignetteSmoothCurrent

        -- === Chromatic state ===
        self._chromaticCurrent  = 0
        self._chromaticTarget   = 0
        self._chromaticDuration = nil
        self._chromaticTimer    = 0
        self._chromaticHeld     = false

        self._camera.chromaticAberrationEnabled   = false
        self._camera.chromaticAberrationIntensity = 0

        -- === Blur state ===
        self._blurCurrent    = 0
        self._blurTarget     = 0
        self._blurRadCurrent = self.DefaultBlurRadius
        self._blurRadTarget  = self.DefaultBlurRadius
        self._blurDuration   = nil
        self._blurTimer      = 0
        self._blurHeld       = false

        self._camera.blurEnabled   = false
        self._camera.blurIntensity = 0
        self._camera.blurRadius    = self._blurRadCurrent

        -- === Motion blur state ===
        self._motionBlurTarget          = 0
        self._motionBlurCurrent         = 0
        self._motionBlurStrengthTarget  = 0
        self._motionBlurStrengthCurrent = 0
        self._motionBlurAngle           = 0

        self._camera.dirBlurEnabled   = false
        self._camera.dirBlurIntensity = 0
        self._camera.dirBlurStrength  = 0
        self._camera.dirBlurAngle     = 0
        self._camera.dirBlurSamples   = self.MotionBlurSamples or 8

        -- === Color grading state ===
        self._cgBrightnessCurrent = self.BaselineCgBrightness
        self._cgBrightnessTarget  = self.BaselineCgBrightness
        self._cgContrastCurrent   = self.BaselineCgContrast
        self._cgContrastTarget    = self.BaselineCgContrast
        self._cgSaturationCurrent = self.BaselineCgSaturation
        self._cgSaturationTarget  = self.BaselineCgSaturation
        self._cgTintCurrentR      = self.BaselineCgTintR
        self._cgTintCurrentG      = self.BaselineCgTintG
        self._cgTintCurrentB      = self.BaselineCgTintB
        self._cgTintTargetR       = self.BaselineCgTintR
        self._cgTintTargetG       = self.BaselineCgTintG
        self._cgTintTargetB       = self.BaselineCgTintB
        self._cgDuration          = nil
        self._cgTimer             = 0
        self._cgHeld              = false

        self._camera.colorGradingEnabled = true
        self._camera.cgBrightness        = self._cgBrightnessCurrent
        self._camera.cgContrast          = self._cgContrastCurrent
        self._camera.cgSaturation        = self._cgSaturationCurrent

        -- === Time scale state ===
        self._timeScaleTarget   = 1.0
        self._timeScaleDuration = nil
        self._timeScaleTimer    = 0
        self._timeScaleHeld     = false

        -- === Scripted sequence state ===
        self._seqRunning = false   -- true while a sequence is actively stepping
        self._seqSteps   = nil     -- reference to the active SEQUENCES entry
        self._seqName    = nil     -- key of the active sequence (for the done event)
        self._seqStep    = 1       -- index of the next step to fire
        self._seqTimer   = 0       -- time accumulated since the last step fired
    end,

    -- ==========================================================================
    -- UPDATE
    -- ==========================================================================
    Update = function(self, dt)
        if not self._camera then return end

        local udt = Time.GetUnscaledDeltaTime()

        -- === Scripted Sequences ===============================================
        -- Advances through the active sequence one step at a time.
        -- Each step waits for its own delay, then publishes its event.
        -- Sequence ends automatically after the final step.
        if self._seqRunning and self._seqSteps then
            self._seqTimer = self._seqTimer + udt
            local step = self._seqSteps[self._seqStep]

            if step and self._seqTimer >= step.delay then
                event_bus.publish(step.event, step.params)
                self._seqStep  = self._seqStep + 1
                self._seqTimer = 0

                if self._seqStep > #self._seqSteps then
                    -- All steps fired — sequence is complete
                    if event_bus and event_bus.publish then
                        event_bus.publish("fx_sequence_done", { name = self._seqName })
                    end
                    self._seqRunning = false
                    self._seqSteps   = nil
                    self._seqName    = nil
                end
            end
        end

        -- === Vignette =========================================================
        do
            if not self._vignetteHeld and self._vignetteDuration then
                self._vignetteTimer = self._vignetteTimer + udt
                if self._vignetteTimer >= self._vignetteDuration then
                    self._vignetteDuration = nil
                    self._vignetteTarget   = self.BaselineVignetteIntensity
                end
            end

            local t = math.min((self.VignetteFadeSpeed or 6.0) * udt, 1.0)
            self._vignetteCurrent       = self._vignetteCurrent       + (self._vignetteTarget       - self._vignetteCurrent)       * t
            self._vignetteSmoothCurrent = self._vignetteSmoothCurrent + (self._vignetteSmoothTarget - self._vignetteSmoothCurrent) * t

            self._camera.vignetteIntensity  = self._vignetteCurrent
            self._camera.vignetteSmoothness = self._vignetteSmoothCurrent
        end

        -- === Chromatic Aberration =============================================
        do
            if not self._chromaticHeld and self._chromaticDuration then
                self._chromaticTimer = self._chromaticTimer + udt
                if self._chromaticTimer >= self._chromaticDuration then
                    self._chromaticDuration = nil
                    self._chromaticTarget   = 0
                end
            end

            local t = math.min((self.ChromaticFadeSpeed or 8.0) * udt, 1.0)
            self._chromaticCurrent = self._chromaticCurrent + (self._chromaticTarget - self._chromaticCurrent) * t

            self._camera.chromaticAberrationEnabled   = self._chromaticCurrent > 0.001
            self._camera.chromaticAberrationIntensity = self._chromaticCurrent
        end

        -- === Gaussian Blur ====================================================
        -- fx_blur events only. Motion blur is handled separately via dirBlur below.
        do
            if not self._blurHeld and self._blurDuration then
                self._blurTimer = self._blurTimer + udt
                if self._blurTimer >= self._blurDuration then
                    self._blurDuration = nil
                    self._blurTarget   = 0
                end
            end

            local t = math.min((self.BlurFadeSpeed or 6.0) * udt, 1.0)
            self._blurCurrent    = self._blurCurrent    + (self._blurTarget    - self._blurCurrent)    * t
            self._blurRadCurrent = self._blurRadCurrent + (self._blurRadTarget - self._blurRadCurrent) * t

            self._camera.blurEnabled   = self._blurCurrent > 0.001
            self._camera.blurIntensity = self._blurCurrent
            self._camera.blurRadius    = self._blurRadCurrent
        end

        -- === Motion Blur (Directional) ========================================
        do
            local mbTarget    = (self.MotionBlurEnabled ~= false) and self._motionBlurTarget         or 0
            local mbStrTarget = (self.MotionBlurEnabled ~= false) and self._motionBlurStrengthTarget or 0
            local mt = math.min((self.MotionBlurFadeSpeed or 12.0) * udt, 1.0)
            self._motionBlurCurrent         = self._motionBlurCurrent         + (mbTarget    - self._motionBlurCurrent)         * mt
            self._motionBlurStrengthCurrent = self._motionBlurStrengthCurrent + (mbStrTarget - self._motionBlurStrengthCurrent) * mt

            local active = self._motionBlurCurrent > 0.001
            self._camera.dirBlurEnabled   = active
            self._camera.dirBlurIntensity = self._motionBlurCurrent
            self._camera.dirBlurStrength  = self._motionBlurStrengthCurrent
            self._camera.dirBlurAngle     = self._motionBlurAngle or 0
            self._camera.dirBlurSamples   = self.MotionBlurSamples or 8
        end

        -- === Color Grading ====================================================
        do
            if not self._cgHeld and self._cgDuration then
                self._cgTimer = self._cgTimer + udt
                if self._cgTimer >= self._cgDuration then
                    self._cgDuration         = nil
                    self._cgBrightnessTarget = self.BaselineCgBrightness
                    self._cgContrastTarget   = self.BaselineCgContrast
                    self._cgSaturationTarget = self.BaselineCgSaturation
                    self._cgTintTargetR      = self.BaselineCgTintR
                    self._cgTintTargetG      = self.BaselineCgTintG
                    self._cgTintTargetB      = self.BaselineCgTintB
                end
            end

            local t = math.min((self.ColorGradingFadeSpeed or 5.0) * udt, 1.0)
            self._cgBrightnessCurrent = self._cgBrightnessCurrent + (self._cgBrightnessTarget - self._cgBrightnessCurrent) * t
            self._cgContrastCurrent   = self._cgContrastCurrent   + (self._cgContrastTarget   - self._cgContrastCurrent)   * t
            self._cgSaturationCurrent = self._cgSaturationCurrent + (self._cgSaturationTarget - self._cgSaturationCurrent) * t
            self._cgTintCurrentR      = self._cgTintCurrentR      + (self._cgTintTargetR      - self._cgTintCurrentR)      * t
            self._cgTintCurrentG      = self._cgTintCurrentG      + (self._cgTintTargetG      - self._cgTintCurrentG)      * t
            self._cgTintCurrentB      = self._cgTintCurrentB      + (self._cgTintTargetB      - self._cgTintCurrentB)      * t

            self._camera.cgBrightness = self._cgBrightnessCurrent
            self._camera.cgContrast   = self._cgContrastCurrent
            self._camera.cgSaturation = self._cgSaturationCurrent
            self._camera:cgTint(self._cgTintCurrentR, self._cgTintCurrentG, self._cgTintCurrentB)
        end

        -- === Time Scale =======================================================
        do
            if not self._timeScaleHeld and self._timeScaleDuration then
                self._timeScaleTimer = self._timeScaleTimer + udt
                if self._timeScaleTimer >= self._timeScaleDuration then
                    self._timeScaleDuration = nil
                    self._timeScaleTarget   = 1.0
                end
            end

            local current = Time.GetTimeScale()
            local delta   = self._timeScaleTarget - current
            if math.abs(delta) > 0.001 then
                Time.SetTimeScale(current + delta * math.min((self.TimeScaleRestoreSpeed or 4.0) * udt, 1.0))
            elseif self._timeScaleTarget == 1.0 and math.abs(current - 1.0) > 0.001 then
                Time.SetTimeScale(1.0)
                if event_bus and event_bus.publish then
                    event_bus.publish("fx_time_scale_restored", {})
                end
            end
        end
    end,

    -- ==========================================================================
    -- ON DISABLE
    -- ==========================================================================
    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe then
            local subs = {
                "_vignetteSub", "_vignetteClearSub",
                "_chromaticSub", "_chromaticClearSub",
                "_blurSub", "_blurClearSub",
                "_motionBlurSub",
                "_colorGradingSub", "_colorGradingClearSub",
                "_timeScaleSub", "_timeScaleClearSub",
                "_clearAllSub",
                "_dodgeSuccessSub", "_vaultJumpSub",
                "_sequenceSub", "_sequenceCancelSub",
            }
            for _, key in ipairs(subs) do
                if self[key] then event_bus.unsubscribe(self[key]); self[key] = nil end
            end
        end

        Time.SetTimeScale(1.0)
        if self._camera then
            self._camera.blurEnabled                  = false
            self._camera.dirBlurEnabled               = false
            self._camera.chromaticAberrationEnabled   = false
            self._camera.vignetteIntensity            = self.BaselineVignetteIntensity
            self._camera.colorGradingEnabled          = true
            self._camera.cgBrightness                 = self.BaselineCgBrightness
            self._camera.cgContrast                   = self.BaselineCgContrast
            self._camera.cgSaturation                 = self.BaselineCgSaturation
        end
    end,
}