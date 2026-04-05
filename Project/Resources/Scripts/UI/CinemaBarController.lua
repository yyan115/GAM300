-- UI/CinemaBarController.lua
-- Attach to the CinemaBar entity.
-- Animates TopBar and BtmBar when cinematic mode is active,
-- matching the camera's transitionDuration. Hides PlayerHUD during cinematic.

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

-- Set Y on a localPosition (handles both userdata and table)
local function setLocalY(tr, y)
    if not tr then return end
    local pos = tr.localPosition
    if pos then
        pos.y = y
        tr.isDirty = true
    end
end

return Component {

    fields = {
        topBarName         = "TopBar",
        btmBarName         = "BtmBar",
        playerHUDName      = "PlayerHUD",

        -- Resting positions (bars fully off-screen)
        topBarRestY        = 14.2,
        btmBarRestY        = -14.2,

        -- Target positions when cinematic is active (how far bars slide in)
        topBarTargetY      = 10.0,
        btmBarTargetY      = -10.0,

        -- Seconds for bars to slide IN when cinematic starts (0 = instant)
        transitionInDuration  = 3.0,
        -- Seconds for bars to slide OUT when cinematic ends (0 = instant)
        transitionOutDuration = 0.0,
    },

    Start = function(self)
        self._topTr  = Engine.FindTransformByName(self.topBarName)
        self._btmTr  = Engine.FindTransformByName(self.btmBarName)

        -- Cache PlayerHUD ActiveComponent for bulk hide/show
        local hudEnt = Engine.GetEntityByName(self.playerHUDName)
        self._hudActive = hudEnt and GetComponent(hudEnt, "ActiveComponent")

        if not self._topTr then
            --print("[CinemaBarController] WARNING: TopBar transform not found")
        end
        if not self._btmTr then
            --print("[CinemaBarController] WARNING: BtmBar transform not found")
        end
        if not self._hudActive then
            --print("[CinemaBarController] WARNING: PlayerHUD ActiveComponent not found")
        end

        -- State
        self._cinematicOn = false
        self._animating   = false
        self._timer       = 0.0
        self._direction   = 1   -- 1 = closing in, -1 = opening out

        -- Ensure bars start at their resting positions
        setLocalY(self._topTr, self.topBarRestY)
        setLocalY(self._btmTr, self.btmBarRestY)

        if event_bus and event_bus.subscribe then
            self._sub = event_bus.subscribe("cinematic.active", function(active)
                if active and not self._cinematicOn then
                    -- Cinematic turning ON
                    self._cinematicOn = true
                    if self._hudActive then self._hudActive.isActive = false end
                    if self.transitionInDuration <= 0 then
                        -- Instant
                        setLocalY(self._topTr, self.topBarTargetY)
                        setLocalY(self._btmTr, self.btmBarTargetY)
                        self._animating = false
                        --print("[CinemaBarController] Cinematic ON - bars snapped in, HUD hidden")
                    else
                        self._direction = 1
                        self._timer     = 0.0
                        self._animating = true
                        --print("[CinemaBarController] Cinematic ON - bars animating in, HUD hidden")
                    end

                elseif not active and self._cinematicOn then
                    -- Cinematic turning OFF
                    self._cinematicOn = false
                    if self._hudActive then self._hudActive.isActive = true end
                    if self.transitionOutDuration <= 0 then
                        -- Instant
                        setLocalY(self._topTr, self.topBarRestY)
                        setLocalY(self._btmTr, self.btmBarRestY)
                        self._animating = false
                        --print("[CinemaBarController] Cinematic OFF - bars snapped out, HUD restored")
                    else
                        self._direction = -1
                        self._timer     = self.transitionOutDuration
                        self._animating = true
                        --print("[CinemaBarController] Cinematic OFF - bars animating out, HUD restored")
                    end
                end
            end)
        end

        --print("[CinemaBarController] Initialized")
    end,

    Update = function(self, dt)
        if not self._animating then return end

        local dur = self._direction == 1 and self.transitionInDuration or self.transitionOutDuration

        -- Advance timer in the correct direction
        if self._direction == 1 then
            self._timer = math.min(self._timer + dt, dur)
        else
            self._timer = math.max(self._timer - dt, 0.0)
        end

        local t  = self._timer / dur
        local st = t * t * (3.0 - 2.0 * t)  -- smoothstep

        local topY = self.topBarRestY + (self.topBarTargetY - self.topBarRestY) * st
        local btmY = self.btmBarRestY + (self.btmBarTargetY - self.btmBarRestY) * st

        setLocalY(self._topTr, topY)
        setLocalY(self._btmTr, btmY)

        -- Stop when animation is complete
        if self._direction == 1 and self._timer >= dur then
            self._animating = false
            --print("[CinemaBarController] Bars fully closed")
        elseif self._direction == -1 and self._timer <= 0.0 then
            self._animating = false
            --print("[CinemaBarController] Bars fully open")
        end
    end,
}
