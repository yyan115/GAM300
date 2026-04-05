-- SplashScreenAnimator.lua
-- Sequence:
--   1. DigiPen logo  – fade in 0.5s → hold 2s → fade out 0.5s
--   2. FMOD logo     – fade in 0.5s → hold 2s
--   3. TIME SKIP     – JoJo-style: effects build ON the visible FMOD logo,
--                      then chaos, then hard-cut black silence (1.1s total)
--   4. Team logo     – SNAPS in at full alpha, scale slams from 140% with heavy
--                      elastic overshoot + CA afterimage (0.6s)
--   5. Team logo     – hold 2s → fade out 0.5s → load next scene

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

-- ── Easing helpers ─────────────────────────────────────────────────────────────
local function clamp01(t) return math.max(0.0, math.min(1.0, t)) end

local function smoothstep(t)
    t = clamp01(t)
    return t * t * (3.0 - 2.0 * t)
end

local function easeOutCubic(t)
    t = clamp01(t)
    return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t)
end

-- Heavy elastic overshoot (c1=2.5 gives ~15% overshoot)
local function easeOutBack(t)
    t = clamp01(t)
    local c1 = 2.5
    local c3 = c1 + 1.0
    local u  = t - 1.0
    return 1.0 + c3 * u * u * u + c1 * u * u
end

local function lerp(a, b, t) return a + (b - a) * clamp01(t) end

-- ── Phase durations (seconds) ─────────────────────────────────────────────────
local D_BLACK    = 0.3    -- initial black hold
local D_FADE_IN  = 0.5    -- logo fade in
local D_HOLD     = 2.0    -- logo hold
local D_FADE_OUT = 0.5    -- logo fade out / DigiPen only (FMOD has no separate fade)
local D_GAP      = 0.2    -- black gap between DigiPen and FMOD
local D_SKIP     = 1.1    -- JoJo time skip (starts while FMOD still visible)
local D_SLAM     = 0.6    -- team logo slam-in after the black
local D_TEAM_OUT  = 0.9    -- team logo anime exit (longer for more drama)
local D_END_BLACK = 1.1    -- hold on black before loading next scene

-- ── Cumulative timestamps ─────────────────────────────────────────────────────
local T1  = D_BLACK
local T2  = T1  + D_FADE_IN       -- DigiPen in
local T3  = T2  + D_HOLD          -- DigiPen hold done
local T4  = T3  + D_FADE_OUT      -- DigiPen out
local T5  = T4  + D_GAP           -- gap done
local T6  = T5  + D_FADE_IN       -- FMOD in
local T7  = T6  + D_HOLD          -- FMOD hold done → TIME SKIP STARTS IMMEDIATELY
local T8  = T7  + D_SKIP          -- skip done → team logo slams in
local T9  = T8  + D_SLAM          -- slam done
local T10 = T9  + D_HOLD          -- team hold done
local T11 = T10 + D_TEAM_OUT      -- team out
local T12 = T11 + D_END_BLACK     -- 1.1s black hold → load next scene

-- ── Skip sub-phase boundaries (as fractions of D_SKIP) ───────────────────────
local SKIP_A_END = 0.50   -- "THE WORLD" – effects build on visible FMOD
local SKIP_B_END = 0.78   -- "ZA WARUDO" – hard-cut chaos while FMOD fades
local SKIP_C_END = 1.00   -- "SILENCE"   – full black, dead quiet

return Component {

    fields = {
        nextScene = "Resources/Scenes/01_MainMenu.scene",
    },

    -- ──────────────────────────────────────────────────────────────────────────
    Start = function(self)
        local camEntity  = Engine.GetEntityByName("Main Camera")
        local digiEntity = Engine.GetEntityByName("Digipen")
        local fmodEntity = Engine.GetEntityByName("FmodLogo")
        local teamEntity = Engine.GetEntityByName("TeamLogo")
        local fadeEntity = Engine.GetEntityByName("SplashFadeScreen")

        self._cam        = camEntity  and GetComponent(camEntity,  "CameraComponent")       or nil
        self._digiSprite = digiEntity and GetComponent(digiEntity, "SpriteRenderComponent") or nil
        self._fmodSprite = fmodEntity and GetComponent(fmodEntity, "SpriteRenderComponent") or nil
        self._teamSprite = teamEntity and GetComponent(teamEntity, "SpriteRenderComponent") or nil
        self._teamTr     = teamEntity and GetComponent(teamEntity, "Transform")              or nil
        self._fadeSprite = fadeEntity and GetComponent(fadeEntity, "SpriteRenderComponent") or nil

        -- Capture team logo base scale for the punch
        if self._teamTr then
            self._teamBaseScaleX = self._teamTr.localScale.x
            self._teamBaseScaleY = self._teamTr.localScale.y
        else
            self._teamBaseScaleX = 960
            self._teamBaseScaleY = 540
        end

        -- All logos invisible, black overlay opaque
        if self._digiSprite then self._digiSprite.alpha = 0.0 end
        if self._fmodSprite then self._fmodSprite.alpha = 0.0 end
        if self._teamSprite then self._teamSprite.alpha = 0.0 end
        if self._fadeSprite then
            self._fadeSprite.isVisible = true
            self._fadeSprite.alpha     = 1.0
        end

        if self._cam then
            self._cam.blurEnabled                  = false
            self._cam.vignetteEnabled              = true
            self._cam.vignetteIntensity            = 0.4
            self._cam.vignetteSmoothness           = 0.5
            self._cam.chromaticAberrationEnabled   = false
            self._cam.chromaticAberrationIntensity = 0.0
            self._cam.dirBlurEnabled               = false
            self._cam.dirBlurIntensity             = 0.0
            self._cam.dirBlurStrength              = 12.0
            self._cam.dirBlurAngle                 = 0.0
        end

        self._timer = 0.0
        self._done  = false
    end,

    -- ──────────────────────────────────────────────────────────────────────────
    Update = function(self, dt)
        if self._done then return end
        self._timer = self._timer + dt
        local t = self._timer

        -- ── Phase 0: hold black ──────────────────────────────────────────────
        if t < T1 then
            -- nothing

        -- ── Phase 1: DigiPen fade in ─────────────────────────────────────────
        elseif t < T2 then
            local pt = (t - T1) / D_FADE_IN
            if self._digiSprite then self._digiSprite.alpha = easeOutCubic(pt) end
            if self._fadeSprite then self._fadeSprite.alpha = 1.0 - easeOutCubic(pt) end

        -- ── Phase 2: DigiPen hold ────────────────────────────────────────────
        elseif t < T3 then
            if self._digiSprite then self._digiSprite.alpha = 1.0 end
            if self._fadeSprite then self._fadeSprite.alpha = 0.0 end

        -- ── Phase 3: DigiPen fade out ────────────────────────────────────────
        elseif t < T4 then
            local pt = (t - T3) / D_FADE_OUT
            if self._digiSprite then self._digiSprite.alpha = 1.0 - smoothstep(pt) end
            if self._fadeSprite then self._fadeSprite.alpha = smoothstep(pt) end

        -- ── Phase 4: black gap ───────────────────────────────────────────────
        elseif t < T5 then
            if self._digiSprite then self._digiSprite.alpha = 0.0 end
            if self._fadeSprite then self._fadeSprite.alpha = 1.0 end

        -- ── Phase 5: FMOD fade in ────────────────────────────────────────────
        elseif t < T6 then
            local pt = (t - T5) / D_FADE_IN
            if self._fmodSprite then self._fmodSprite.alpha = easeOutCubic(pt) end
            if self._fadeSprite then self._fadeSprite.alpha = 1.0 - easeOutCubic(pt) end

        -- ── Phase 6: FMOD hold ───────────────────────────────────────────────
        elseif t < T7 then
            if self._fmodSprite then self._fmodSprite.alpha = 1.0 end
            if self._fadeSprite then self._fadeSprite.alpha = 0.0 end

        -- ── Phase 7: JoJo TIME SKIP ──────────────────────────────────────────
        --
        --  Sub-A "THE WORLD STOPS" (0 → 50%):
        --    FMOD logo is FULLY VISIBLE — all effects act on it.
        --    Vignette tunnels in, CA builds from 0→5, directional blur activates.
        --    FMOD itself starts flickering (rapid alpha oscillation).
        --
        --  Sub-B "ZA WARUDO" (50% → 78%):
        --    6 hard-cut frame alternations (black / FMOD visible).
        --    On the visible frames: CA=6, dirBlur maxed → extremely distorted logo.
        --    FMOD fades out over this period.
        --
        --  Sub-C "SILENCE" (78% → 100%):
        --    Full black. No effects. Dead silence before the slam.
        --
        elseif t < T8 then
            local pt = (t - T7) / D_SKIP

            if pt < SKIP_A_END then
                -- ─ Sub-A: effects building on visible FMOD ─────────────────
                local apt = pt / SKIP_A_END   -- 0 → 1

                -- FMOD flickers with increasing intensity (logo is still clear)
                local flicker = 0.5 + 0.5 * math.sin(apt * math.pi * 18.0)
                local fmodA   = lerp(1.0, flicker * 0.6 + 0.2, apt * apt)
                if self._fmodSprite then self._fmodSprite.alpha = fmodA end

                -- CA ramps 0 → 5.0 (visible as RGB split on the logo)
                local ca = apt * apt * 5.0
                if self._cam then
                    self._cam.chromaticAberrationEnabled   = true
                    self._cam.chromaticAberrationIntensity = ca
                    -- Vignette tunnels in hard
                    self._cam.vignetteIntensity = lerp(0.4, 1.0, apt * apt)
                    -- Directional blur kicks in at 40%
                    local blurPt = math.max(0.0, (apt - 0.4) / 0.6)
                    self._cam.dirBlurEnabled   = blurPt > 0.0
                    self._cam.dirBlurIntensity = blurPt * 0.9
                    self._cam.dirBlurStrength  = 12.0
                    self._cam.dirBlurAngle     = 0.0
                end
                if self._fadeSprite then self._fadeSprite.alpha = 0.0 end

            elseif pt < SKIP_B_END then
                -- ─ Sub-B: hard-cut chaos while FMOD fades ──────────────────
                local bpt = (pt - SKIP_A_END) / (SKIP_B_END - SKIP_A_END)  -- 0 → 1

                -- 6 hard-cut alternations
                local cutIndex = math.floor(bpt * 6.0)
                local isBlack  = (cutIndex % 2 == 0)

                -- FMOD fades out over this window (still flickers when visible)
                local fmodA = (1.0 - bpt) * (isBlack and 0.0 or 1.0)
                if self._fmodSprite then self._fmodSprite.alpha = math.max(0.0, fmodA) end

                if self._cam then
                    -- Visible frames: CA maxed + dirBlur = intense distortion on logo
                    self._cam.chromaticAberrationEnabled   = not isBlack
                    self._cam.chromaticAberrationIntensity = isBlack and 0.0 or 6.5
                    self._cam.vignetteIntensity = 1.0
                    self._cam.dirBlurEnabled   = not isBlack
                    self._cam.dirBlurIntensity = isBlack and 0.0 or 1.0
                end
                if self._fadeSprite then
                    self._fadeSprite.alpha = isBlack and 1.0 or 0.0
                end

            else
                -- ─ Sub-C: full black silence ────────────────────────────────
                if self._fmodSprite then self._fmodSprite.alpha = 0.0 end
                if self._fadeSprite then self._fadeSprite.alpha = 1.0 end
                if self._cam then
                    self._cam.chromaticAberrationEnabled = false
                    self._cam.chromaticAberrationIntensity = 0.0
                    self._cam.dirBlurEnabled = false
                    self._cam.vignetteIntensity = 1.0
                end
            end

        -- ── Phase 8: Team logo SLAM ──────────────────────────────────────────
        --   Logo SNAPS to full alpha immediately (no fade ramp — it just APPEARS).
        --   Black overlay drops in 0.12s.
        --   CA afterimage (4.5 → 0) burns off in first half.
        --   Vignette eases from 1.0 → 0.4.
        --   Scale hammers in from 140% with heavy elastic bounce.
        elseif t < T9 then
            local pt = (t - T8) / D_SLAM

            -- Logo appears instantly
            if self._teamSprite then self._teamSprite.alpha = 1.0 end

            -- Black drops away in the first 20%
            if self._fadeSprite then
                self._fadeSprite.alpha = math.max(0.0, 1.0 - pt / 0.20)
            end

            -- CA afterimage burns off by 50%
            local afterCA = lerp(4.5, 0.0, clamp01(pt / 0.5))
            if self._cam then
                self._cam.chromaticAberrationEnabled   = afterCA > 0.1
                self._cam.chromaticAberrationIntensity = afterCA
                self._cam.vignetteIntensity            = lerp(1.0, 0.4, easeOutCubic(pt))
                -- Brief settling dirBlur in first 25%
                local settleBlur = math.max(0.0, 1.0 - pt / 0.25) * 0.6
                self._cam.dirBlurEnabled   = settleBlur > 0.05
                self._cam.dirBlurIntensity = settleBlur
            end

            -- Scale slam: 140% → elastic snap to 100%
            if self._teamTr then
                local s = easeOutBack(pt)
                self._teamTr.localScale.x = lerp(self._teamBaseScaleX * 1.4, self._teamBaseScaleX, s)
                self._teamTr.localScale.y = lerp(self._teamBaseScaleY * 1.4, self._teamBaseScaleY, s)
                self._teamTr.isDirty = true
            end

        -- ── Phase 9: TeamLogo hold ───────────────────────────────────────────
        elseif t < T10 then
            if self._teamSprite then self._teamSprite.alpha = 1.0 end
            if self._fadeSprite then self._fadeSprite.alpha = 0.0 end
            if self._teamTr then
                self._teamTr.localScale.x = self._teamBaseScaleX
                self._teamTr.localScale.y = self._teamBaseScaleY
                self._teamTr.isDirty = true
            end
            if self._cam then
                self._cam.chromaticAberrationEnabled   = false
                self._cam.chromaticAberrationIntensity = 0.0
                self._cam.dirBlurEnabled               = false
                self._cam.vignetteIntensity            = 0.4
            end

        -- ── Phase 10: TeamLogo ANIME EXIT ───────────────────────────────────────
        --
        --  Mirror of the time skip — effects MUST act on a VISIBLE logo.
        --
        --  Sub-A (0 – 45%) "CHARGE UP":
        --    Logo stays at alpha 1.0. CA builds 0 → 5.0 on the fully opaque logo
        --    (RGB split is clearly visible). Vignette tunnels in. dirBlur activates.
        --
        --  Sub-B (45 – 80%) "HARD CUTS":
        --    5 rapid black/logo alternations identical to the time skip chaos.
        --    On visible frames: logo at 1.0, CA=6.5, dirBlur maxed.
        --    On black frames: full black overlay, CA off.
        --
        --  Sub-C (80 – 100%) "BLACKOUT":
        --    Full black. All effects off. Scene loads.
        --
        elseif t < T11 then
            local pt = (t - T10) / D_TEAM_OUT

            if pt < 0.45 then
                -- ─ Sub-A: build effects on the fully visible logo ────────────
                local apt = pt / 0.45   -- 0 → 1

                if self._teamSprite then self._teamSprite.alpha = 1.0 end
                if self._fadeSprite then self._fadeSprite.alpha = 0.0 end

                -- CA ramps 0 → 5.0 — clearly visible on an opaque sprite
                local ca = apt * apt * 5.0
                -- dirBlur kicks in at 55%
                local blurPt = math.max(0.0, (apt - 0.55) / 0.45)

                if self._cam then
                    self._cam.chromaticAberrationEnabled   = ca > 0.05
                    self._cam.chromaticAberrationIntensity = ca
                    self._cam.vignetteIntensity            = lerp(0.4, 1.0, apt * apt)
                    self._cam.dirBlurEnabled               = blurPt > 0.0
                    self._cam.dirBlurIntensity             = blurPt * 0.85
                    self._cam.dirBlurStrength              = 12.0
                    self._cam.dirBlurAngle                 = 0.0
                end

            elseif pt < 0.80 then
                -- ─ Sub-B: hard-cut chaos — same pattern as time skip ────────
                local bpt = (pt - 0.45) / 0.35   -- 0 → 1

                -- 5 alternating cuts
                local cutIndex = math.floor(bpt * 5.0)
                local isBlack  = (cutIndex % 2 == 0)

                if self._teamSprite then self._teamSprite.alpha = isBlack and 0.0 or 1.0 end

                if self._cam then
                    self._cam.chromaticAberrationEnabled   = not isBlack
                    self._cam.chromaticAberrationIntensity = isBlack and 0.0 or 6.5
                    self._cam.vignetteIntensity            = 1.0
                    self._cam.dirBlurEnabled               = not isBlack
                    self._cam.dirBlurIntensity             = isBlack and 0.0 or 1.0
                end
                if self._fadeSprite then
                    self._fadeSprite.alpha = isBlack and 1.0 or 0.0
                end

            else
                -- ─ Sub-C: full black silence ─────────────────────────────────
                if self._teamSprite then self._teamSprite.alpha = 0.0 end
                if self._cam then
                    self._cam.chromaticAberrationEnabled   = false
                    self._cam.chromaticAberrationIntensity = 0.0
                    self._cam.dirBlurEnabled               = false
                    self._cam.vignetteIntensity            = 1.0
                end
                if self._fadeSprite then
                    self._fadeSprite.isVisible = true
                    self._fadeSprite.alpha     = 1.0
                end
            end

        -- ── Black hold before scene load ─────────────────────────────────────
        elseif t < T12 then
            -- stay on black, nothing to update

        -- ── Done: load next scene ────────────────────────────────────────────
        else
            self._done = true
            if Scene and Scene.Load then
                Scene.Load(self.nextScene)
            end
        end
    end,
}
