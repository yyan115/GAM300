-- UI/CrosshairController.lua
-- Manages the crosshair sprite: swaps texture and tints color based on
-- chain aim mode and whether the crosshair is over an enemy (aim assist
-- lock-on OR camera raycast hitting an Enemy/Boss tagged entity).
--
-- Attach this script to the Crosshair UI entity that has a SpriteRenderComponent.
-- Set DefaultSpriteGUID and AimSpriteGUID in the editor to the GUIDs from the
-- .png.meta files for CrosshairDefault and CrosshairAim respectively.

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

return Component {
    fields = {
        -- Texture GUIDs (copy from .png.meta files)
        DefaultSpriteGUID = "006dfc96b345cd00-000474f3c0007229",  -- CrosshairDefault
        AimSpriteGUID     = "006dfc96f87b9ff3-000474f2ae005605",  -- CrosshairAim
        PulseScale        = 1.5,   -- max scale multiplier during pulse (e.g. 1.5 = 150%)
        PulseDuration     = 0.25,  -- seconds for one full pulse
    },

    Start = function(self)
        self._sprite = self:GetComponent("SpriteRenderComponent")
        self._transform = self:GetComponent("Transform")
        self._isAiming       = false
        self._onEnemy        = false
        self._wasOnEnemy     = false

        -- Pulse animation state
        self._pulseTimer     = 0
        self._isPulsing      = false

        -- Store base scale from Transform
        if self._transform then
            self._baseScaleX = self._transform.localScale.x
            self._baseScaleY = self._transform.localScale.y
        end

        if event_bus and event_bus.subscribe then
            self._aimSub = event_bus.subscribe("chain.aim_camera", function(payload)
                if not payload then return end
                self._isAiming = payload.active or false
                if not self._isAiming then
                    self._onEnemy = false
                end
            end)

            self._enemySub = event_bus.subscribe("chain.crosshair_on_enemy", function(payload)
                if not payload then return end
                self._onEnemy = payload.active or false
            end)
        end
    end,

    Update = function(self, dt)
        if not self._sprite then return end

        -- Also read global in case event_bus subscription failed
        local aiming = self._isAiming or _G.CHAIN_AIM_ACTIVE

        -- Trigger pulse when crosshair first lands on enemy while aiming
        if aiming and self._onEnemy and not self._wasOnEnemy then
            self._isPulsing  = true
            self._pulseTimer = 0
        end
        self._wasOnEnemy = self._onEnemy

        -- Cancel pulse immediately when leaving aim mode
        if not aiming and self._isPulsing then
            self._isPulsing  = false
            self._pulseTimer = 0
        end

        -- Advance pulse animation
        if self._isPulsing then
            self._pulseTimer = self._pulseTimer + dt
            if self._pulseTimer >= self.PulseDuration then
                self._isPulsing  = false
                self._pulseTimer = 0
            end
        end

        -- Apply scale on Transform (pulse uses a sine curve: 0 -> 1 -> 0)
        if self._transform and self._baseScaleX then
            local scaleMult = 1.0
            if self._isPulsing then
                local t = self._pulseTimer / self.PulseDuration   -- 0..1
                scaleMult = 1.0 + (self.PulseScale - 1.0) * math.sin(t * math.pi)
            end
            self._transform.localScale.x = self._baseScaleX * scaleMult
            self._transform.localScale.y = self._baseScaleY * scaleMult
            self._transform.isDirty = true
        end

        if aiming then
            self._sprite:SetTextureFromGUID(self.AimSpriteGUID)
            if self._onEnemy then
                -- Tint red when crosshair is over enemy/boss
                self._sprite.color.x = 1.0
                self._sprite.color.y = 0.0
                self._sprite.color.z = 0.0
            else
                self._sprite.color.x = 1.0
                self._sprite.color.y = 1.0
                self._sprite.color.z = 1.0
            end
        else
            self._sprite:SetTextureFromGUID(self.DefaultSpriteGUID)
            self._sprite.color.x = 1.0
            self._sprite.color.y = 1.0
            self._sprite.color.z = 1.0
        end
    end,

    OnDestroy = function(self)
        if event_bus and event_bus.unsubscribe then
            if self._aimSub then
                pcall(function() event_bus.unsubscribe(self._aimSub) end)
            end
            if self._enemySub then
                pcall(function() event_bus.unsubscribe(self._enemySub) end)
            end
        end
    end,
}
