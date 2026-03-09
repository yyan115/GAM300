require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

return Component {
    fields = {
        -- Sprite GUIDs array: [1] = normal sprite, [2] = hover sprite
        -- Drag-drop textures from editor (recognized via "sprite" in field name)
        spriteGUIDs = {},
        HoverSFX = {},
    },

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        self._audio = self:GetComponent("AudioComponent")
        self._sprite = self:GetComponent("SpriteRenderComponent")
        self._isHovered = false
    end,

    Update = function(self, dt)
        self:_updateHover()
    end,

    -- Simple hover detection and sprite swap
    _updateHover = function(self)
        if not self._transform then return end

        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return end

        local mouseCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        local inputX = mouseCoordinate[1]
        local inputY = mouseCoordinate[2]

        local pos = self._transform.localPosition
        local scale = self._transform.localScale
        local minX = pos.x - (scale.x / 2)
        local maxX = pos.x + (scale.x / 2)
        local minY = pos.y - (scale.y / 2)
        local maxY = pos.y + (scale.y / 2)

        local isHovering = inputX >= minX and inputX <= maxX and inputY >= minY and inputY <= maxY

        if isHovering and not self._isHovered then
            self._isHovered = true
            if self._audio and self.HoverSFX and self.HoverSFX[1] then
                self._audio:PlayOneShot(self.HoverSFX[1])
            end
            -- Switch to hover sprite
            if self._sprite and self.spriteGUIDs and self.spriteGUIDs[2] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[2])
            else
                print("[ResetSettingsButton] Cannot switch to hover - sprite: " .. tostring(self._sprite) .. ", spriteGUIDs[2]: " .. tostring(self.spriteGUIDs and self.spriteGUIDs[2]))
            end
        elseif not isHovering and self._isHovered then
            self._isHovered = false
            -- Switch back to normal sprite
            if self._sprite and self.spriteGUIDs and self.spriteGUIDs[1] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[1])
            else
                print("[ResetSettingsButton] Cannot switch to normal - sprite: " .. tostring(self._sprite) .. ", spriteGUIDs[1]: " .. tostring(self.spriteGUIDs and self.spriteGUIDs[1]))
            end
        end
    end,

    -- Reset all settings to defaults
    OnClickResetButton = function(self)
        if self._audio and self.HoverSFX and self.HoverSFX[2] then
            self._audio:PlayOneShot(self.HoverSFX[2])
        end

        -- Initialize GameSettings (safe to call multiple times)
        GameSettings.Init()

        -- Reset to defaults (this also applies and saves via C++)
        GameSettings.ResetToDefaults()

        -- Notify all sliders to update their positions
        if event_bus then
            event_bus.publish("settings_reset", {})
        end

        print("[ResetSettings] All settings reset to defaults")
    end,
}
