require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

local FADE_IN_SPEED  = 5.0   -- alpha per second during fade in
local FADE_OUT_DELAY = 1.0   -- seconds to stay visible after cooldown ends
local FADE_OUT_SPEED = 3.0   -- alpha per second during fade out
local GLOW_PULSE_SPEED = 6.0 -- speed of the ready glow pulse

return Component {
    mixins = { TransformMixin },

    Awake = function(self)
        self._dashCooldown = 0
        self._dashCooldownMax = 0
        self._fadeOutTimer = 0    -- counts down after cooldown ends
        self._state = "hidden"    -- hidden | fadein | cooling | ready | fading

        if event_bus and event_bus.subscribe then
            self._dashEndedSub = event_bus.subscribe("dash_ended", function(payload)
                if payload and payload.cooldown then
                    self._dashCooldownMax = payload.cooldown
                    self._dashCooldown = payload.cooldown
                    self._state = "fadein"

                    if self._spriteRender then
                        self._spriteRender.alpha = 0.0
                        self._spriteRender.fillValue = 0.0
                    end
                end
            end)
        end
    end,

    Start = function(self)
        self._spriteRender = self:GetComponent("SpriteRenderComponent")
        self._spriteRender.fillValue = 1.0
        self._spriteRender.alpha = 0.0
    end,

    Update = function(self, dt)
        if self._state == "hidden" then
            return
        end

        self._spriteRender = self:GetComponent("SpriteRenderComponent")
        if self._state == "fadein" then
            local alpha = self._spriteRender.alpha + FADE_IN_SPEED * dt
            if alpha >= 1.0 then
                alpha = 1.0
                self._state = "cooling"
            end
            self._spriteRender.alpha = alpha
            return
        end

        if self._state == "cooling" then
            self._dashCooldown = self._dashCooldown - dt
            if self._dashCooldown <= 0 then
                self._dashCooldown = 0
                self._spriteRender.fillValue = 1.0
                self._state = "ready"
                self._fadeOutTimer = FADE_OUT_DELAY
            else
                self._spriteRender.fillValue = 1.0 - (self._dashCooldown / self._dashCooldownMax)
            end
            return
        end

        if self._state == "ready" then
            -- Glow pulse to indicate dash is available
            local pulse = 0.6 + 0.4 * math.abs(math.sin(self._fadeOutTimer * GLOW_PULSE_SPEED))
            self._spriteRender.alpha = pulse

            self._fadeOutTimer = self._fadeOutTimer - dt
            if self._fadeOutTimer <= 0 then
                self._state = "fading"
            end
            return
        end

        if self._state == "fading" then
            local alpha = self._spriteRender.alpha - FADE_OUT_SPEED * dt
            if alpha <= 0 then
                alpha = 0
                self._state = "hidden"
            end
            self._spriteRender.alpha = alpha
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe then
            if self._dashEndedSub then
                event_bus.unsubscribe(self._dashEndedSub)
            end
        end
    end,
}