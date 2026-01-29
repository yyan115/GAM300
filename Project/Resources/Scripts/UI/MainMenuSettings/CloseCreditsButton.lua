require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        fadeDuration = 0.5,   -- Duration for fade out when manually closing
    },

    Start = function(self)
        local closeEntity = Engine.GetEntityByName("CloseCreditsButton")
        if closeEntity then
            self._audio = GetComponent(closeEntity, "AudioComponent")
            self._transform = GetComponent(closeEntity, "Transform")
        end

        self._isHovered = false
        self._isFading = false
        self._fadeTimer = 0
        self._wasCreditsActive = false  -- Track previous active state for rising edge detection

        -- Cache entity references
        self._creditsTextEntity = Engine.GetEntityByName("CreditsFullText")
        self._creditsBGEntity = Engine.GetEntityByName("CreditsBG")
        self._creditsUIEntity = Engine.GetEntityByName("CreditsUI")

        -- Cache component references
        if self._creditsTextEntity then
            self._creditsTextTransform = GetComponent(self._creditsTextEntity, "Transform")
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
        end

        -- Update previous state
        self._wasCreditsActive = isActive

        -- Early exit if CreditsUI is not active
        if not isActive then
            return
        end

        -- Handle hover sound
        if self._transform then
            local pointerPos = Input.GetPointerPosition()
            if pointerPos then
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
                    if self._audio then
                        self._audio:Play()
                    end
                elseif not isHovering then
                    self._isHovered = false
                end
            end
        end

        -- Handle fade out when manually closing
        if self._isFading and self._creditsBGSprite then
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

    OnClickCloseCreditsButton = function(self)
        if self._audio then
            self._audio:Play()
        end

        -- Start fade out
        self._isFading = true
        self._fadeTimer = 0
    end,

    -- Reset credits text position to start
    _resetCreditsPosition = function(self)
        if self._creditsTextTransform then
            local pos = self._creditsTextTransform.localPosition
            -- Reset to starting position (1030, -2150)
            pos.x = 1030
            pos.y = -2150
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
    end,
}
