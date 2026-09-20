require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

local FADE_IN_SPEED = 5.0     -- alpha per second while the icon appears
local BLINK_SECONDS = 0.5     -- the ready blink, at the end of the wait
local FADE_SECONDS  = 0.2     -- the icon going, at the very end of the wait
local BLINK_HZ      = 6.0     -- blinks per second during the ready blink

-- The icon runs on the dash's own cooldown, handed to it when the dash is
-- taken: the arc fills, then it blinks, then it goes, and it goes exactly as
-- the dash comes back. It used to start counting when the dash movement
-- ended, half a second late, so the dash was usable while the icon still
-- said otherwise.
return Component {
    mixins = { TransformMixin },

    Awake = function(self)
        self._remaining = 0
        self._cooldown = 0
        self._state = "hidden"    -- hidden | fadein | counting

        if event_bus and event_bus.subscribe then
            self._dashStartedSub = event_bus.subscribe("dash_cooldown_started", function(payload)
                local cooldown = payload and tonumber(payload.cooldown)
                if not cooldown or cooldown <= 0 then return end
                self._cooldown = cooldown
                self._remaining = cooldown
                self._state = "fadein"
                if self._spriteRender then
                    self._spriteRender.alpha = 0.0
                    self._spriteRender.fillValue = 0.0
                end
            end)

            -- The dash coming back is what ends the icon, whatever the clock
            -- says, so the two can never drift apart.
            self._dashReadySub = event_bus.subscribe("dash_ready", function()
                self._remaining = 0
            end)
        end
    end,

    Start = function(self)
        self._spriteRender = self:GetComponent("SpriteRenderComponent")
        self._spriteRender.fillValue = 1.0
        self._spriteRender.alpha = 0.0
    end,

    Update = function(self, dt)
        if self._state == "hidden" then return end
        self._spriteRender = self:GetComponent("SpriteRenderComponent")
        dt = dt or 0

        if self._state == "fadein" then
            local alpha = self._spriteRender.alpha + FADE_IN_SPEED * dt
            if alpha >= 1.0 then
                alpha = 1.0
                self._state = "counting"
            end
            self._spriteRender.alpha = alpha
        end

        self._remaining = self._remaining - dt
        if self._remaining <= 0 then
            self._remaining = 0
            self._state = "hidden"
            self._spriteRender.alpha = 0.0
            self._spriteRender.fillValue = 1.0
            return
        end

        -- The wait, back to front: the arc fills, the last half second blinks,
        -- and the last fifth of a second fades out.
        local arcSeconds = math.max(0.05, self._cooldown - BLINK_SECONDS - FADE_SECONDS)
        if self._remaining > BLINK_SECONDS + FADE_SECONDS then
            local left = self._remaining - BLINK_SECONDS - FADE_SECONDS
            self._spriteRender.fillValue = 1.0 - (left / arcSeconds)
        elseif self._remaining > FADE_SECONDS then
            self._spriteRender.fillValue = 1.0
            if self._state == "counting" then
                local blink = 0.55 + 0.45 * math.sin(self._remaining * BLINK_HZ * 2 * math.pi)
                self._spriteRender.alpha = blink
            end
        else
            self._spriteRender.fillValue = 1.0
            self._spriteRender.alpha = self._remaining / FADE_SECONDS
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe then
            for _, key in ipairs({ "_dashStartedSub", "_dashReadySub" }) do
                if self[key] then
                    pcall(function() event_bus.unsubscribe(self[key]) end)
                    self[key] = nil
                end
            end
        end
    end,
}
