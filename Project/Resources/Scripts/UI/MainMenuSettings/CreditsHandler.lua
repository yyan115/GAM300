require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        scrollSpeed = 100,
        fastScrollSpeed = 400,
        startY = -1550,
        endY = 3500,
        fadeDuration = 1.0,
    },

    Start = function(self)
        self._isScrolling = false
        self._isFading = false
        self._fadeTimer = 0
        self._wasActive = false

        print("[CreditsHandler] Start called")

        self._creditsTextEntity = Engine.GetEntityByName("CreditsFullText")
        self._creditsBGEntity = Engine.GetEntityByName("CreditsBG")
        self._creditsUIEntity = Engine.GetEntityByName("CreditsUI")

        if self._creditsTextEntity then
            self._creditsTextTransform = GetComponent(self._creditsTextEntity, "Transform")

            if self._creditsTextTransform then
                local pos = self._creditsTextTransform.localPosition
                self._creditsTextInitX = pos.x
                self._creditsTextInitY = pos.y
            end

            self._creditsTextSprite = GetComponent(self._creditsTextEntity, "SpriteRenderComponent")
        end

        if self._creditsBGEntity then
            self._creditsBGSprite = GetComponent(self._creditsBGEntity, "SpriteRenderComponent")
        end

        if self._creditsUIEntity then
            self._creditsUIActive = GetComponent(self._creditsUIEntity, "ActiveComponent")
        end

        -- global reference for close button
        _G.CreditsHandler = self
    end,

    BeginClose = function(self)
        if self._isFading then return end
        self._isScrolling = false
        self._isFading = true
        self._fadeTimer = 0
    end,

    Update = function(self, dt)

        -- detect open
        if self._creditsUIActive then
            local isActive = self._creditsUIActive.isActive

            if isActive and not self._wasActive then
                self:_resetCredits()
                self._isScrolling = true
                self._isFading = false

                local mainButtons = {"PlayGame", "Credits", "ExitGame", "Settings"}
                for _, name in ipairs(mainButtons) do
                    local ent = Engine.GetEntityByName(name)
                    if ent then
                        local button = GetComponent(ent, "ButtonComponent")
                        if button then
                            button.interactable = false
                        end
                    end
                end
            end

            self._wasActive = isActive
        end

        -- scrolling
        if self._isScrolling and self._creditsTextTransform then
            local pos = self._creditsTextTransform.localPosition
            local speed = Input.IsPointerPressed() and self.fastScrollSpeed or self.scrollSpeed
            local newY = pos.y + (speed * dt)

            if newY >= self.endY then
                newY = self.endY
                self._isScrolling = false
                self._isFading = true
                self._fadeTimer = 0
            end

            pos.y = newY
            self._creditsTextTransform.isDirty = true
        end

        -- fade
        if self._isFading then
            print("inshallah") -- NOW THIS WILL PRINT

            self._fadeTimer = self._fadeTimer + dt
            local fadeProgress = math.min(self._fadeTimer / self.fadeDuration, 1.0)
            local alpha = 1.0 - fadeProgress

            if self._creditsBGSprite then
                self._creditsBGSprite.alpha = alpha
            end

            if self._creditsTextSprite then
                self._creditsTextSprite.alpha = alpha
            end

            if fadeProgress >= 1.0 then
                self._isFading = false
                self:_closeCreditsUI()
            end
        end
    end,

    _resetCredits = function(self)
        if self._creditsTextTransform then
            local pos = self._creditsTextTransform.localPosition
            pos.x = self._creditsTextInitX
            pos.y = self._creditsTextInitY
            self._creditsTextTransform.isDirty = true
        end

        if self._creditsBGSprite then
            self._creditsBGSprite.alpha = 1.0
        end

        if self._creditsTextSprite then
            self._creditsTextSprite.alpha = 1.0
        end

        self._fadeTimer = 0
    end,

    _closeCreditsUI = function(self)

        if self._creditsUIActive then
            self._creditsUIActive.isActive = false
        end

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

        self._isScrolling = false
        self._isFading = false
        self._wasActive = false
    end,
}