require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

-- One of the quit prompt's two buttons. The engine puts the prompt up, from
-- Alt+F4 or from the main menu's Exit button, holds the game still under it
-- and takes it down again, so all a button does is answer it: Yes closes the
-- game and No dismisses the prompt.
return Component {
    fields = {
        -- [1] = Normal Sprite, [2] = Highlighted Sprite
        spriteGUIDs = {},
        -- [1] = Hover SFX, [2] = Click SFX
        SFX = {},
    },

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        self._sprite = self:GetComponent("SpriteRenderComponent")
        self._isHovered = false

        -- Force the sprite to its normal (non-hover) state on first creation
        -- so the prefab default doesn't show the hover texture.
        if self._sprite and self.spriteGUIDs and self.spriteGUIDs[1] then
            self._sprite:SetTextureFromGUID(self.spriteGUIDs[1])
        end

        -- The prompt only exists while it is being answered.
        local button = self:GetComponent("ButtonComponent")
        if button then
            button.interactable = true
        end

        -- Sounds play through the AudioComponent on the prompt's root, which is
        -- on the UI bus and so stays audible while the game's sound is paused.
        local root = Engine.GetParentEntity(self.entityId)
        self._audio = root and GetComponent(root, "AudioComponent")
    end,

    Update = function(self, dt)
        self:_updateHoverState()
    end,

    _playSFX = function(self, index)
        local clip = self.SFX and self.SFX[index]
        if self._audio and clip then
            self._audio:PlayOneShot(clip)
        end
    end,

    _updateHoverState = function(self)
        if not self._transform then return end

        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return end

        local mouseCoord = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        local inputX, inputY = mouseCoord[1], mouseCoord[2]

        local pos = self._transform.worldPosition
        local scale = self._transform.localScale

        -- AABB Detection
        local isHovering = inputX >= pos.x - (scale.x / 2) and
                           inputX <= pos.x + (scale.x / 2) and
                           inputY >= pos.y - (scale.y / 2) and
                           inputY <= pos.y + (scale.y / 2)

        -- State Change: Enter Hover
        if isHovering and not self._isHovered then
            self._isHovered = true
            self:_playSFX(1)

            -- Swap to Highlighted Sprite
            if self._sprite and self.spriteGUIDs[2] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[2])
            end

        -- State Change: Exit Hover
        elseif not isHovering and self._isHovered then
            self._isHovered = false

            -- Swap back to Normal Sprite
            if self._sprite and self.spriteGUIDs[1] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[1])
            end
        end
    end,

    ClosePrompt = function(self)
        self:_playSFX(2)
        Screen.CancelQuit()
    end,

    ExitGame = function(self)
        self:_playSFX(2)
        Screen.RequestClose()
    end
}
