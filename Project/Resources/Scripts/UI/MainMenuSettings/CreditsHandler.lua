require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        scrollSpeed = 100,           -- Pixels per second
        startY = -2150,              -- Starting Y position
        endY = 3500,                 -- Target Y position (when scrolling ends)
        fadeDuration = 1.0,          -- Duration for fade out
    },

    Start = function(self)
        self._isScrolling = false
        self._isFading = false
        self._fadeTimer = 0
        self._wasActive = false
        print("[CreditsHandler] Start called")

        -- Cache entity references
        self._creditsTextEntity = Engine.GetEntityByName("CreditsFullText")
        self._creditsBGEntity = Engine.GetEntityByName("CreditsBG")
        self._creditsUIEntity = Engine.GetEntityByName("CreditsUI")

        print("[CreditsHandler] CreditsText entity: " .. tostring(self._creditsTextEntity))
        print("[CreditsHandler] CreditsBG entity: " .. tostring(self._creditsBGEntity))
        print("[CreditsHandler] CreditsUI entity: " .. tostring(self._creditsUIEntity))

        -- Cache component references
        if self._creditsTextEntity then
            self._creditsTextTransform = GetComponent(self._creditsTextEntity, "Transform")
            print("[CreditsHandler] CreditsText Transform: " .. tostring(self._creditsTextTransform))
        end

        if self._creditsBGEntity then
            self._creditsBGSprite = GetComponent(self._creditsBGEntity, "SpriteRenderComponent")
            print("[CreditsHandler] CreditsBG Sprite: " .. tostring(self._creditsBGSprite))
        end

        if self._creditsUIEntity then
            self._creditsUIActive = GetComponent(self._creditsUIEntity, "ActiveComponent")
            print("[CreditsHandler] CreditsUI Active: " .. tostring(self._creditsUIActive))
        end
    end,

    Update = function(self, dt)
        -- Check if CreditsUI just became active
        if self._creditsUIActive then
            local isActive = self._creditsUIActive.isActive

            -- Detect when CreditsUI becomes active (rising edge)
            if isActive and not self._wasActive then
                print("[CreditsHandler] CreditsUI activated! Starting scroll...")
                self:_resetCredits()
                self._isScrolling = true
                self._isFading = false
                print("[CreditsHandler] _isScrolling set to: " .. tostring(self._isScrolling))
            end

            self._wasActive = isActive
        end

        -- Handle scrolling
        if self._isScrolling and self._creditsTextTransform then
            local pos = self._creditsTextTransform.localPosition
            local newY = pos.y + (self.scrollSpeed * dt)

            -- Check if we've reached the end position
            if newY >= self.endY then
                newY = self.endY
                self._isScrolling = false
                self._isFading = true
                self._fadeTimer = 0
                print("[CreditsHandler] Reached end position, starting fade")
            end

            -- Update position (modify in-place)
            pos.y = newY
            self._creditsTextTransform.isDirty = true
        end

        -- Handle fade out
        if self._isFading and self._creditsBGSprite then
            self._fadeTimer = self._fadeTimer + dt
            local fadeProgress = math.min(self._fadeTimer / self.fadeDuration, 1.0)

            -- Fade out the background (alpha from 1 to 0)
            self._creditsBGSprite.alpha = 1.0 - fadeProgress

            -- Once fade is complete, disable CreditsUI
            if fadeProgress >= 1.0 then
                self._isFading = false
                self:_closeCreditsUI()
            end
        end
    end,

    -- Reset credits to initial state
    _resetCredits = function(self)
        -- Reset text position to start
        if self._creditsTextTransform then
            local pos = self._creditsTextTransform.localPosition
            pos.y = self.startY
            self._creditsTextTransform.isDirty = true
        end

        -- Reset background alpha
        if self._creditsBGSprite then
            self._creditsBGSprite.alpha = 1.0
        end

        self._fadeTimer = 0
    end,

    -- Close the credits UI and re-enable main menu buttons
    _closeCreditsUI = function(self)
        -- Disable CreditsUI
        if self._creditsUIActive then
            self._creditsUIActive.isActive = false
        end

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

        -- Reset state
        self._isScrolling = false
        self._isFading = false
    end,
}
