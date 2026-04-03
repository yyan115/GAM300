require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

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
        self._button = self:GetComponent("ButtonComponent")
        self._isHovered = false
        self._wasSettingsActive = false

        self._SettingsUIEntity = Engine.GetEntityByName("SettingsUI")
        if self._SettingsUIEntity then
            self._settingsUIActive = GetComponent(self._SettingsUIEntity, "ActiveComponent")
        end

        -- Start non-interactable; enabled only when SettingsUI is open
        if self._button then
            self._button.interactable = false
        end
    end,

    Update = function(self, dt)
        local isActive = self._settingsUIActive and self._settingsUIActive.isActive

        -- Rising edge: SettingsUI just became active
        if isActive and not self._wasSettingsActive then
            self._isHovered = false
            -- Reset to normal sprite in case it was left on hover sprite
            if self._sprite and self.spriteGUIDs and self.spriteGUIDs[1] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[1])
            end
            if self._button then
                self._button.interactable = true
            end
        end

        self._wasSettingsActive = isActive

        -- Early exit if SettingsUI is not active
        if not isActive then
            return
        end

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
            end
        elseif not isHovering and self._isHovered then
            self._isHovered = false
            -- Switch back to normal sprite
            if self._sprite and self.spriteGUIDs and self.spriteGUIDs[1] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[1])
            end
        end
    end,

    OnClickCloseButton = function(self)
        -- Only process if settings is actually active
        local isActive = self._settingsUIActive and self._settingsUIActive.isActive
        if not isActive then return end

        if self._audio and self.HoverSFX and self.HoverSFX[2] then
            self._audio:PlayOneShot(self.HoverSFX[2])
        end

        -- Disable both buttons immediately so they can't be clicked again
        if self._button then
            self._button.interactable = false
        end
        local resetEntity = Engine.GetEntityByName("ResetButton")
        if resetEntity then
            local resetBtn = GetComponent(resetEntity, "ButtonComponent")
            if resetBtn then
                resetBtn.interactable = false
            end
        end

        -- Save settings when closing menu (only writes if dirty)
        GameSettings.SaveIfDirty()

        -- Re-enable main menu buttons
        local targetButtons = {
            "PlayGame",
            "Credits",
            "ExitGame",
            "Settings"
        }

        for index, value in ipairs(targetButtons) do
            local targetEntity = Engine.GetEntityByName(value)
            if targetEntity and targetEntity ~= -1 then
                local btnComp = GetComponent(targetEntity, "ButtonComponent")
                if btnComp then
                    btnComp.interactable = true
                end
            end
        end

        -- Re-enable button text entities
        local targetTexts = {"PlayGameText", "SettingText", "CreditsText", "ExitGameText"}
        for _, textName in ipairs(targetTexts) do
            local textEntity = Engine.GetEntityByName(textName)
            if textEntity and textEntity ~= -1 then
                local textActive = GetComponent(textEntity, "ActiveComponent")
                if textActive then
                    textActive.isActive = true
                end
            end
        end

        -- Close SettingsUI
        local settingUIEntity = Engine.GetEntityByName("SettingsUI")
        if settingUIEntity and settingUIEntity ~= -1 then
            local activeComp = GetComponent(settingUIEntity, "ActiveComponent")
            if activeComp then
                activeComp.isActive = false
            end
        end
    end,
}
