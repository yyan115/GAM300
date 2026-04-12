require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local event_bus = _G.event_bus

return Component {

    fields = {
        fadeDuration = 0.5, -- Duration for fade out when manually closing
        -- Sprite GUIDs array: [1] = normal sprite, [2] = hover sprite
        -- Drag-drop textures from editor (recognized via "sprite" in field name)
        spriteGUIDs = {},
    },

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        self._sprite = self:GetComponent("SpriteRenderComponent")
        self._button = self:GetComponent("ButtonComponent")

        self._isHovered = false
        self._wasCreditsActive = false

        -- Start non-interactable; enabled only when credits is open
        if self._button then
            self._button.interactable = false
        end

        -- Cache entity references
        self._creditsUIEntity = Engine.GetEntityByName("CreditsUI")
        self._creditsUIActive = self._creditsUIEntity and GetComponent(self._creditsUIEntity, "ActiveComponent")
    end,

    Update = function(self, dt)
        -- Check current active state
        local isActive = self._creditsUIActive and self._creditsUIActive.isActive

        -- Detect rising edge (CreditsUI just became active) - reset state
        if isActive and not self._wasCreditsActive then
            if self._button then
                self._button.interactable = true
            end

            if self._sprite then
                self._sprite.isVisible = true
                -- Reset to normal sprite so previous-session hover never lingers.
                if self.spriteGUIDs and self.spriteGUIDs[1] then
                    self._sprite:SetTextureFromGUID(self.spriteGUIDs[1])
                end
            end
            self._isHovered = false
        end

        -- Update previous state
        self._wasCreditsActive = isActive

        -- Early exit if CreditsUI is not active
        if not isActive then return end

        -- Handle hover detection and sprite swapping
        self:_updateHover()
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

        if self._sprite then
            self._sprite.isVisible = false
        end

        if _G.CreditsHandler then
            _G.CreditsHandler:BeginClose()
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