require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        -- Sprite GUIDs array: [1] = normal sprite, [2] = hover sprite
        -- Drag-drop textures from editor (recognized via "sprite" in field name)
        spriteGUIDs = {},
        -- SFX GUIDs: [1] = hover SFX, [2] = click SFX
        HoverSFX = {},
        -- BGM fade settings
        bgmFadeDuration = 1.0,  -- How long to fade out BGM when skipping
    },

    Start = function(self)
        -- Get components on this entity
        self._transform = self:GetComponent("Transform")
        self._sprite = self:GetComponent("SpriteRenderComponent")
        self._audio = self:GetComponent("AudioComponent")

        -- Calculate button bounds from transform
        if self._transform then
            local pos = self._transform.localPosition
            local scale = self._transform.localScale
            self.minX = pos.x - (scale.x / 2)
            self.maxX = pos.x + (scale.x / 2)
            self.minY = pos.y - (scale.y / 2)
            self.maxY = pos.y + (scale.y / 2)
        end

        -- Get video component reference for skip functionality
        local cutsceneEntity = Engine.GetEntityByName("Cutscene")
        if cutsceneEntity then
            self.videoComp = GetComponent(cutsceneEntity, "VideoComponent")
        end

        -- Get BGM audio component from dedicated BGM entity
        local bgmEntity = Engine.GetEntityByName("BGM")
        if bgmEntity then
            self._bgmAudio = GetComponent(bgmEntity, "AudioComponent")
        end

        self._isHovered = false
        self._isSkipTriggered = false
        self._isFadingBGM = false
        self._bgmFadeTimer = 0
        self._originalBGMVolume = 1.0
    end,

    Update = function(self, dt)
        if not self._sprite then
            return
        end

        -- Handle BGM fade out
        if self._isFadingBGM and self._bgmAudio then
            self._bgmFadeTimer = self._bgmFadeTimer + dt
            local fadeProgress = math.min(self._bgmFadeTimer / self.bgmFadeDuration, 1.0)

            -- Fade volume from original to 0
            local newVolume = self._originalBGMVolume * (1.0 - fadeProgress)
            self._bgmAudio:SetVolume(newVolume)

            if fadeProgress >= 1.0 then
                self._isFadingBGM = false
                self._bgmAudio:Stop()
            end
        end

        -- Don't process hover/click if skip already triggered
        if self._isSkipTriggered then
            return
        end

        -- GET GAME COORDINATE FOR MOUSE
        local mousePos = Input.GetPointerPosition()
        if not mousePos then return end

        local mouseCoordinate = Engine.GetGameCoordinate(mousePos.x, mousePos.y)
        if not mouseCoordinate or not mouseCoordinate[1] or not mouseCoordinate[2] then
            return
        end

        local inputX = mouseCoordinate[1]
        local inputY = mouseCoordinate[2]

        -- CHECK IF HOVERING OVER THIS BUTTON
        local isHovering = inputX >= self.minX and inputX <= self.maxX and
                           inputY >= self.minY and inputY <= self.maxY

        -- Handle hover state change
        if isHovering and not self._isHovered then
            -- Just started hovering
            self._isHovered = true

            -- Play hover SFX
            if self._audio and self.HoverSFX and self.HoverSFX[1] then
                self._audio:PlayOneShot(self.HoverSFX[1])
            end

            -- Switch to hover sprite
            if self._sprite and self.spriteGUIDs and self.spriteGUIDs[2] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[2])
            end

        elseif not isHovering and self._isHovered then
            -- Stopped hovering
            self._isHovered = false

            -- Switch back to normal sprite
            if self._sprite and self.spriteGUIDs and self.spriteGUIDs[1] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[1])
            end
        end
    end,

    OnSkipClicked = function(self)
        if self._isSkipTriggered then
            return
        end

        self._isSkipTriggered = true

        -- Play click SFX
        if self._audio and self.HoverSFX and self.HoverSFX[2] then
            self._audio:PlayOneShot(self.HoverSFX[2])
        end

        -- Start BGM fade out
        if self._bgmAudio then
            self._isFadingBGM = true
            self._bgmFadeTimer = 0
        end

        -- Hide the skip button sprite
        if self._sprite then
            self._sprite.isVisible = false
        end

        -- Trigger cutscene end through VideoComponent
        -- The C++ VideoSystem will detect this and start the skip fade
        if self.videoComp then
            self.videoComp.cutsceneEnded = true
        end
    end,
}
