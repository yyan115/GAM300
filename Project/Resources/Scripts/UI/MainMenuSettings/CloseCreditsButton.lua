require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local event_bus = _G.event_bus

return Component {
    fields = {
        fadeDuration = 0.5,   -- Duration for fade out when manually closing
        -- Sprite GUIDs array: [1] = normal sprite, [2] = hover sprite
        -- Drag-drop textures from editor (recognized via "sprite" in field name)
        spriteGUIDs = {},
        HoverSFX = {},
    },

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        self._sprite = self:GetComponent("SpriteRenderComponent")
        self._button = self:GetComponent("ButtonComponent")

        self._isHovered = false
        self._isFading = false
        self._fadeTimer = 0
        self._wasCreditsActive = false

        -- Start non-interactable; enabled only when credits is open
        if self._button then
            self._button.interactable = false
        end

        -- Cache entity references
        self._creditsTextEntity = Engine.GetEntityByName("CreditsFullText")
        self._creditsBGEntity = Engine.GetEntityByName("CreditsBG")
        self._creditsUIEntity = Engine.GetEntityByName("CreditsUI")

        -- Cache component references
        if self._creditsTextEntity then
            self._creditsTextTransform = GetComponent(self._creditsTextEntity, "Transform")
            -- Cache the initial position for reset
            if self._creditsTextTransform then
                local pos = self._creditsTextTransform.localPosition
                self._creditsTextInitX = pos.x
                self._creditsTextInitY = pos.y
            end
        end

        if self._creditsBGEntity then
            self._creditsBGSprite = GetComponent(self._creditsBGEntity, "SpriteRenderComponent")
        end

        if self._creditsUIEntity then
            self._creditsUIActive = GetComponent(self._creditsUIEntity, "ActiveComponent")
        end
    end,

    Update = function(self, dt)
        -- Check current active state
        local isActive = self._creditsUIActive and self._creditsUIActive.isActive

        -- Detect rising edge (CreditsUI just became active) - reset state
        if isActive and not self._wasCreditsActive then
            self._isFading = false
            self._fadeTimer = 0
            self._isHovered = false
            -- Reset to normal sprite in case it was left on hover sprite
            if self._sprite and self.spriteGUIDs and self.spriteGUIDs[1] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[1])
            end
            if self._button then
                self._button.interactable = true
            end
        end

        if isActive and not self._isFading then
            self._sprite.isVisible = true
        end

        -- Update previous state
        self._wasCreditsActive = isActive

        -- Early exit if CreditsUI is not active
        if not isActive then
            return
        end

        -- Handle hover detection and sprite swapping
        self:_updateHover()

        -- Handle fade out when manually closing
        if self._isFading and self._creditsBGSprite then
            self._sprite.isVisible = false
            self._fadeTimer = self._fadeTimer + dt
            local fadeProgress = math.min(self._fadeTimer / self.fadeDuration, 1.0)

            -- Fade out the background (alpha from 1 to 0)
            self._creditsBGSprite.alpha = 1.0 - fadeProgress

            -- Once fade is complete, disable CreditsUI
            if fadeProgress >= 1.0 then
                self._isFading = false
                self:_finishClose()
            end
        end
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
            if event_bus and event_bus.publish then
                event_bus.publish("main_menu.hover", {})
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

    OnClickCloseCreditsButton = function(self)
        -- Only process if credits is actually active
        local isActive = self._creditsUIActive and self._creditsUIActive.isActive
        if not isActive then return end

        if event_bus and event_bus.publish then
            event_bus.publish("main_menu.click", {})
        end

        -- Disable button immediately so it can't be clicked again during fade
        if self._button then
            self._button.interactable = false
        end

        -- Start fade out
        self._isFading = true
        self._fadeTimer = 0
    end,

    -- Reset credits text position to start
    _resetCreditsPosition = function(self)
        if self._creditsTextTransform then
            local pos = self._creditsTextTransform.localPosition
            pos.x = self._creditsTextInitX
            pos.y = self._creditsTextInitY
            self._creditsTextTransform.isDirty = true
        end
    end,

    -- Complete the close operation after fade
    _finishClose = function(self)
        -- Disable CreditsUI
        if self._creditsUIActive then
            self._creditsUIActive.isActive = false
        end

        -- Reset credits text position for next time
        self:_resetCreditsPosition()

        -- Reset background alpha for next time
        if self._creditsBGSprite then
            self._creditsBGSprite.alpha = 1.0
        end

        -- Reset tracking so rising edge detection works on next open
        self._wasCreditsActive = false

        -- Re-enable main menu buttons
        local targetButtons = {"PlayGame", "Credits", "ExitGame", "Settings"}
        for _, buttonName in ipairs(targetButtons) do
            local entity = Engine.GetEntityByName(buttonName)
            if entity then
                local button = GetComponent(entity, "ButtonComponent")
                if button then
                    button.interactable = true
                end
            end
        end

        -- Re-enable button text entities
        local targetTexts = {"PlayGameText", "SettingText", "CreditsText", "ExitGameText"}
        for _, textName in ipairs(targetTexts) do
            local textEntity = Engine.GetEntityByName(textName)
            if textEntity then
                local textActive = GetComponent(textEntity, "ActiveComponent")
                if textActive then
                    textActive.isActive = true
                end
            end
        end
    end,
}
