require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {

    fields = {
        fadeDuration = 0.5,
        spriteGUIDs = {},
        HoverSFX = {},
    },

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        self._audio = self:GetComponent("AudioComponent")
        self._sprite = self:GetComponent("SpriteRenderComponent")
        self._button = self:GetComponent("ButtonComponent")

        self._isHovered = false
        self._wasCreditsActive = false

        if self._button then
            self._button.interactable = false
        end

        self._creditsUIEntity = Engine.GetEntityByName("CreditsUI")
        self._creditsUIActive = self._creditsUIEntity and GetComponent(self._creditsUIEntity, "ActiveComponent")
    end,

    Update = function(self, dt)

        local isActive = self._creditsUIActive and self._creditsUIActive.isActive

        if isActive and not self._wasCreditsActive then
            if self._button then
                self._button.interactable = true
            end

            if self._sprite then
                self._sprite.isVisible = true
            end
        end

        self._wasCreditsActive = isActive

        if not isActive then return end

        self:_updateHover()
    end,

    OnClickCloseCreditsButton = function(self)

        local isActive = self._creditsUIActive and self._creditsUIActive.isActive
        if not isActive then return end

        if self._audio and self.HoverSFX and self.HoverSFX[2] then
            self._audio:PlayOneShot(self.HoverSFX[2])
        end

        if self._button then
            self._button.interactable = false
        end

        if self._sprite then
            self._sprite.isVisible = false
        end

        if _G.CreditsHandler then
            _G.CreditsHandler:BeginClose()
        end
    end,

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
            if self._sprite and self.spriteGUIDs[2] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[2])
            end

        elseif not isHovering and self._isHovered then
            self._isHovered = false
            if self._sprite and self.spriteGUIDs[1] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[1])
            end
        end
    end,
}