-- UI/CrosshairController.lua
-- Manages the crosshair sprite: swaps texture and tints color based on
-- chain aim mode and whether aim assist is locked onto an enemy.
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
    },

    Start = function(self)
        self._sprite = self:GetComponent("SpriteRenderComponent")
        self._isAiming    = false
        self._isLocked    = false

        if event_bus and event_bus.subscribe then
            -- Chain aim mode on/off
            self._aimSub = event_bus.subscribe("chain.aim_camera", function(payload)
                if not payload then return end
                self._isAiming = payload.active or false
                if not self._isAiming then
                    self._isLocked = false
                end
            end)

            -- Aim assist locked onto an enemy
            self._lockSub = event_bus.subscribe("chain.aim_assist_locked", function(payload)
                if not payload then return end
                self._isLocked = payload.locked or false
            end)
        end
    end,

    Update = function(self, dt)
        if not self._sprite then return end

        if self._isAiming then
            -- Switch to aim crosshair
            self._sprite:SetTextureFromGUID(self.AimSpriteGUID)
            if self._isLocked then
                -- Tint red when locked onto enemy
                self._sprite.color.x = 1.0
                self._sprite.color.y = 0.0
                self._sprite.color.z = 0.0
            else
                -- White when aiming but not locked
                self._sprite.color.x = 1.0
                self._sprite.color.y = 1.0
                self._sprite.color.z = 1.0
            end
        else
            -- Default dot crosshair
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
            if self._lockSub then
                pcall(function() event_bus.unsubscribe(self._lockSub) end)
            end
        end
    end,
}
