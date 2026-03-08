-- SplashScreenAnimator.lua
-- Drives the 00_SplashScreen sequence:
--   Phase 1 – Fade in from black (DigiPen logo appears)
--   Phase 2 – DigiPen holds at centre
--   Phase 3 – TeamLogo slides in from right, pushing DigiPen left
--              (brief chromatic-aberration impact flash)
--   Phase 4 – Both logos hold
--   Phase 5 – Fade to black → load next scene
-- Camera post-processing (blur + vignette) used throughout for polish.

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

-- ── Easing helpers ────────────────────────────────────────────────────────────
local function clamp01(t) return math.max(0.0, math.min(1.0, t)) end

local function smoothstep(t)
    t = clamp01(t)
    return t * t * (3.0 - 2.0 * t)
end

local function easeOutCubic(t)
    t = clamp01(t)
    return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t)
end

local function lerp(a, b, t) return a + (b - a) * clamp01(t) end

-- ── Screen-space layout (pixels, 1920 × 1080) ────────────────────────────────
local SCREEN_CX = 960
local SCREEN_CY = 540

-- DigiPen: centre → pushed left   (width ~700 px, so [70 … 770] when at x=420)
local DIGI_CENTER_X  = SCREEN_CX
local DIGI_PUSHED_X  = 570

-- TeamLogo: off-screen right → right-centre  (width ~800 px)
local TEAM_OFFSCREEN_X = 2800
local TEAM_FINAL_X     = 1380

-- ── Phase durations (seconds) ─────────────────────────────────────────────────
local D_HOLD_BLACK = 0.5
local D_FADE_IN    = 1.5
local D_DIGI_HOLD  = 1.5
local D_PUSH       = 1.8
local D_BOTH_HOLD  = 1.5
local D_FADE_OUT   = 1.2

-- ── Cumulative thresholds ─────────────────────────────────────────────────────
local T1 = D_HOLD_BLACK
local T2 = T1 + D_FADE_IN
local T3 = T2 + D_DIGI_HOLD
local T4 = T3 + D_PUSH
local T5 = T4 + D_BOTH_HOLD
local T6 = T5 + D_FADE_OUT     -- total duration

-- ── Camera blur values ────────────────────────────────────────────────────────
local BLUR_HIGH = 2.5
local BLUR_RADIUS = 3.0

return Component {

    fields = {
        nextScene = "Resources/Scenes/01_MainMenu.scene",
    },

    -- ──────────────────────────────────────────────────────────────────────────
    Start = function(self)
        -- Locate entities
        local camEntity  = Engine.GetEntityByName("Main Camera")
        local digiEntity = Engine.GetEntityByName("Digipen")
        local teamEntity = Engine.GetEntityByName("TeamLogo")
        local fadeEntity = Engine.GetEntityByName("SplashFadeScreen")

        -- Cache components
        self._cam        = camEntity  and GetComponent(camEntity,  "CameraComponent")        or nil
        self._digiTr     = digiEntity and GetComponent(digiEntity, "Transform")               or nil
        self._teamTr     = teamEntity and GetComponent(teamEntity, "Transform")               or nil
        self._digiSprite = digiEntity and GetComponent(digiEntity, "SpriteRenderComponent")   or nil
        self._teamSprite = teamEntity and GetComponent(teamEntity, "SpriteRenderComponent")   or nil
        self._fadeSprite = fadeEntity and GetComponent(fadeEntity, "SpriteRenderComponent")   or nil

        -- FadeScreen: fully opaque black covers everything at the start
        if self._fadeSprite then
            self._fadeSprite.isVisible = true
            self._fadeSprite.alpha     = 1.0
        end

        -- DigiPen: position at screen centre (stays there for Phase 1-2)
        if self._digiTr then
            self._digiTr.localPosition.x = DIGI_CENTER_X
            self._digiTr.localPosition.y = SCREEN_CY
            self._digiTr.isDirty = true
        end

        -- TeamLogo: placed off-screen right, invisible
        if self._teamTr then
            self._teamTr.localPosition.x = TEAM_OFFSCREEN_X
            self._teamTr.localPosition.y = SCREEN_CY
            self._teamTr.isDirty = true
        end
        if self._teamSprite then
            self._teamSprite.alpha = 0.0
        end

        -- Camera post-processing: dreamy blurred intro with cinematic vignette
        if self._cam then
            self._cam.blurEnabled              = true
            self._cam.blurIntensity            = BLUR_HIGH
            self._cam.blurRadius               = BLUR_RADIUS
            self._cam.vignetteEnabled          = true
            self._cam.vignetteIntensity        = 0.7
            self._cam.vignetteSmoothness       = 0.4
            self._cam.chromaticAberrationEnabled   = false
            self._cam.chromaticAberrationIntensity = 0.0
        end

        self._timer = 0.0
        self._done  = false
    end,

    -- ──────────────────────────────────────────────────────────────────────────
    Update = function(self, dt)
        if self._done then return end

        self._timer = self._timer + dt
        local t = self._timer

        -- ── Phase 0: hold black ───────────────────────────────────────────────
        if t < T1 then
            -- Nothing to animate; FadeScreen stays opaque.

        -- ── Phase 1: fade in from black ───────────────────────────────────────
        elseif t < T2 then
            local pt   = (t - T1) / D_FADE_IN
            local ease = easeOutCubic(pt)

            -- FadeScreen fades out, revealing DigiPen
            if self._fadeSprite then
                self._fadeSprite.alpha = 1.0 - ease
            end

            -- Camera blur dissolves; vignette softens to resting level
            if self._cam then
                self._cam.blurEnabled   = true
                self._cam.blurIntensity = lerp(BLUR_HIGH, 0.0, ease)
                self._cam.vignetteIntensity = lerp(0.7, 0.35, ease)
                if pt >= 1.0 then
                    self._cam.blurEnabled = false
                end
            end

        -- ── Phase 2: DigiPen hold ─────────────────────────────────────────────
        elseif t < T3 then
            if self._fadeSprite then self._fadeSprite.alpha = 0.0 end
            if self._cam then
                self._cam.blurEnabled       = false
                self._cam.vignetteIntensity = 0.35
            end

        -- ── Phase 3: push ─────────────────────────────────────────────────────
        --   TeamLogo slides in from right (ease-out deceleration).
        --   DigiPen starts moving at pt=0.25 (ease-in-out acceleration).
        --   Chromatic aberration flares at the "impact" moment (pt ≈ 0.38).
        elseif t < T4 then
            local pt = (t - T3) / D_PUSH       -- 0 → 1

            -- TeamLogo: ease-out slide
            local teamEase = easeOutCubic(pt)
            local teamX    = lerp(TEAM_OFFSCREEN_X, TEAM_FINAL_X, teamEase)
            if self._teamTr then
                self._teamTr.localPosition.x = teamX
                self._teamTr.isDirty = true
            end

            -- TeamLogo fades in during first 60 % of the push
            if self._teamSprite then
                self._teamSprite.alpha = math.min(pt / 0.6, 1.0)
            end

            -- DigiPen: delayed ease-in-out push (starts at pt=0.25)
            local digiPt   = math.max(0.0, (pt - 0.25) / 0.75)
            local digiEase = smoothstep(digiPt)
            local digiX    = lerp(DIGI_CENTER_X, DIGI_PUSHED_X, digiEase)
            if self._digiTr then
                self._digiTr.localPosition.x = digiX
                self._digiTr.isDirty = true
            end

            -- Chromatic aberration flare at impact peak (pt ≈ 0.38, width ±0.18)
            if self._cam then
                local dist    = math.abs(pt - 0.38)
                local impactT = math.max(0.0, 1.0 - dist / 0.18)
                self._cam.chromaticAberrationEnabled   = (impactT > 0.0)
                self._cam.chromaticAberrationIntensity = impactT * 0.9
            end

        -- ── Phase 4: both logos hold ──────────────────────────────────────────
        elseif t < T5 then
            -- Snap to final positions (in case of floating-point drift)
            if self._digiTr then
                self._digiTr.localPosition.x = DIGI_PUSHED_X
                self._digiTr.isDirty = true
            end
            if self._teamTr then
                self._teamTr.localPosition.x = TEAM_FINAL_X
                self._teamTr.isDirty = true
            end
            if self._teamSprite then self._teamSprite.alpha = 1.0 end
            if self._cam then
                self._cam.chromaticAberrationEnabled   = false
                self._cam.chromaticAberrationIntensity = 0.0
                self._cam.vignetteIntensity            = 0.35
            end

        -- ── Phase 5: fade out to black ────────────────────────────────────────
        elseif t < T6 then
            local pt   = (t - T5) / D_FADE_OUT
            local ease = smoothstep(pt)

            -- Both logos fade out
            if self._digiSprite  then self._digiSprite.alpha  = 1.0 - ease end
            if self._teamSprite  then self._teamSprite.alpha  = 1.0 - ease end

            -- FadeScreen fades back in (black out)
            if self._fadeSprite then
                self._fadeSprite.isVisible = true
                self._fadeSprite.alpha     = ease
            end

            -- Camera blur re-engages to match the black-out feel
            if self._cam then
                self._cam.blurEnabled   = (pt > 0.0)
                self._cam.blurIntensity = lerp(0.0, BLUR_HIGH, ease)
            end

        -- ── Done: load next scene ─────────────────────────────────────────────
        else
            self._done = true
            if Scene and Scene.Load then
                Scene.Load(self.nextScene)
            end
        end
    end,
}
