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
        self._isHovered = false
        self._SettingsUIEntity = Engine.GetEntityByName("SettingsUI")
        if self._SettingsUIEntity then
            self._settingsUIActive = GetComponent(self._SettingsUIEntity, "ActiveComponent")
        end
    end,

    Update = function(self, dt)
        if not self._settingsUIActive or not self._settingsUIActive.isActive then
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
            else
                print("[CloseSettingButton] Cannot switch to hover sprite - sprite: " .. tostring(self._sprite) .. ", spriteGUIDs: " .. tostring(self.spriteGUIDs) .. ", spriteGUIDs[2]: " .. tostring(self.spriteGUIDs and self.spriteGUIDs[2]))
            end
        elseif not isHovering and self._isHovered then
            self._isHovered = false
            -- Switch back to normal sprite
            if self._sprite and self.spriteGUIDs and self.spriteGUIDs[1] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[1])
            else
                print("[CloseSettingButton] Cannot switch to normal sprite - sprite: " .. tostring(self._sprite) .. ", spriteGUIDs: " .. tostring(self.spriteGUIDs) .. ", spriteGUIDs[1]: " .. tostring(self.spriteGUIDs and self.spriteGUIDs[1]))
            end
        end
    end,

    OnClickCloseButton = function(self)
            if self._audio and self.HoverSFX and self.HoverSFX[2] then
                self._audio:PlayOneShot(self.HoverSFX[2])
            end

        -- Save settings when closing menu (only writes if dirty)
        GameSettings.SaveIfDirty()

        -- BUTTONS TO ENABLE
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

        -- CLOSE SETTINGS UI
        local settingUIEntity = Engine.GetEntityByName("SettingsUI")
        if settingUIEntity and settingUIEntity ~= -1 then
            local activeComp = GetComponent(settingUIEntity, "ActiveComponent")
            if activeComp then
                activeComp.isActive = false
            end
        end
    end,
}
