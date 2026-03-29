require("extension.engine_bootstrap")
local event_bus = _G.event_bus
local Component = require("extension.mono_helper")

-- [CRITICAL FIX] Ensure we call Component with { } to return a TABLE, not the function.
return Component {
    fields = {
        -- Audio: [1] = hover SFX, [2] = click SFX
        ButtonSFX = {},
        -- Sprite GUIDs: [1] = normal, [2] = hover
        ContinueSpriteGUIDs = {},
        ControlsSpriteGUIDs = {},
        SettingSpriteGUIDs = {},
        MainMenuSpriteGUIDs = {},
    },

    Start = function(self)
        -- Cache audio component from PauseMenuUI entity
        self._audio = self:GetComponent("AudioComponent")
        self._pageWasActive = false

        -- Detect platform
        local isAndroid = Platform and Platform.IsAndroid and Platform.IsAndroid()

        -- On Android: hide Controls button and shift Settings + MainMenu up into its slot
        if isAndroid then
            local controlsEnt  = Engine.GetEntityByName("ControlsButton")
            local settingsEnt  = Engine.GetEntityByName("SettingsButton")
            local mainMenuEnt  = Engine.GetEntityByName("MainMenuButton")

            local controlsTransform = controlsEnt  and GetComponent(controlsEnt,  "Transform")
            local settingsTransform = settingsEnt  and GetComponent(settingsEnt,  "Transform")
            local mainMenuTransform = mainMenuEnt  and GetComponent(mainMenuEnt,  "Transform")

            if controlsTransform and settingsTransform and mainMenuTransform then
                local controlsY = controlsTransform.localPosition.y
                local settingsY  = settingsTransform.localPosition.y

                -- Shift Settings up to where Controls was
                local sPos = settingsTransform.localPosition
                if type(sPos) == "userdata" then
                    sPos.y = controlsY
                else
                    settingsTransform.localPosition = { x = sPos.x, y = controlsY, z = sPos.z }
                end
                settingsTransform.isDirty = true

                -- Shift MainMenu up to where Settings was
                local mPos = mainMenuTransform.localPosition
                if type(mPos) == "userdata" then
                    mPos.y = settingsY
                else
                    mainMenuTransform.localPosition = { x = mPos.x, y = settingsY, z = mPos.z }
                end
                mainMenuTransform.isDirty = true
            end

            -- Hide Controls button
            if controlsEnt then
                local activeComp = GetComponent(controlsEnt, "ActiveComponent")
                if activeComp then activeComp.isActive = false end
            end
        end

        -- Setup button data with sprite swapping support
        self._buttonData = {}
        local buttonMapping = {
            { base = "ContinueButton", spriteGUIDs = self.ContinueSpriteGUIDs },
            { base = "ControlsButton", spriteGUIDs = self.ControlsSpriteGUIDs },
            { base = "SettingsButton", spriteGUIDs = self.SettingSpriteGUIDs },
            { base = "MainMenuButton", spriteGUIDs = self.MainMenuSpriteGUIDs },
        }

        for index, config in ipairs(buttonMapping) do
            -- Skip Controls button on Android
            if isAndroid and config.base == "ControlsButton" then
                -- do nothing
            else
                local baseEnt = Engine.GetEntityByName(config.base)

                if baseEnt then
                    local transform = GetComponent(baseEnt, "Transform")
                    local sprite = GetComponent(baseEnt, "SpriteRenderComponent")

                    -- Guard against missing components
                    if transform and sprite then
                        local pos = transform.localPosition
                        local scale = transform.localScale

                        self._buttonData[index] = {
                            name = config.base,
                            sprite = sprite,
                            spriteGUIDs = config.spriteGUIDs,
                            minX = pos.x - (scale.x / 2),
                            maxX = pos.x + (scale.x / 2),
                            minY = pos.y - (scale.y / 2),
                            maxY = pos.y + (scale.y / 2),
                            wasHovered = false
                        }
                    else
                        print("[PauseMenuButtonHandler] Warning: Missing Transform or Sprite on " .. config.base)
                    end
                else
                    print("[PauseMenuButtonHandler] Warning: Missing entity " .. config.base)
                end
            end
        end
    end,

    Update = function(self, dt)
        if not self._buttonData then return end

        local pageEntity = Engine.GetEntityByName("PauseMenuUI")
        local pageComp = pageEntity and GetComponent(pageEntity, "ActiveComponent")
        local pageIsActive = pageComp and pageComp.isActive

        if not pageIsActive then
            for _, data in pairs(self._buttonData) do
                if data.wasHovered then
                    if data.sprite and data.spriteGUIDs and data.spriteGUIDs[1] then
                        data.sprite:SetTextureFromGUID(data.spriteGUIDs[1])
                    end
                    data.wasHovered = false
                end
            end
            self._pageWasActive = false
            return
        end

        local justBecameActive = not self._pageWasActive
        self._pageWasActive = true

        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return end

        local mouseCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        local inputX, inputY = mouseCoordinate[1], mouseCoordinate[2]

        for _, data in pairs(self._buttonData) do
            local isHovering = inputX >= data.minX and inputX <= data.maxX and
                               inputY >= data.minY and inputY <= data.maxY

            if justBecameActive then
                -- First frame page is visible: init state without triggering hover enter
                data.wasHovered = isHovering
            else
                -- Handle hover enter
                if isHovering and not data.wasHovered then
                    -- Play hover sound
                    if self._audio and self.ButtonSFX and self.ButtonSFX[1] then
                        self._audio:PlayOneShot(self.ButtonSFX[1])
                    end
                    -- Switch to hover sprite
                    if data.sprite and data.spriteGUIDs and data.spriteGUIDs[2] then
                        data.sprite:SetTextureFromGUID(data.spriteGUIDs[2])
                    end
                -- Handle hover exit
                elseif not isHovering and data.wasHovered then
                    -- Switch back to normal sprite
                    if data.sprite and data.spriteGUIDs and data.spriteGUIDs[1] then
                        data.sprite:SetTextureFromGUID(data.spriteGUIDs[1])
                    end
                end

                data.wasHovered = isHovering
            end
        end
    end,

    OnClickContinueButton = function(self)
        local btn = Engine.GetEntityByName("ContinueButton")
        if btn then
            local audiocomp = GetComponent(btn, "AudioComponent")
            if audiocomp then audiocomp:Play() end
        end
        
        if Screen then Screen.SetCursorLocked(true) end

        -- Hide pause menu
        local pauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        if pauseUIEntity then
            local pauseComp = GetComponent(pauseUIEntity, "ActiveComponent")
            if pauseComp then pauseComp.isActive = false end
        end

        -- Unpause all game audio
        Audio.SetBusPaused("BGM", false)
        Audio.SetBusPaused("SFX", false)

        Time.SetPaused(false)

        if event_bus and event_bus.publish then
            event_bus.publish("game_paused", false)
            event_bus.publish("uiButtonPressed", true)
        end
    end,

    OnClickControlsButton = function(self)
        local btn = Engine.GetEntityByName("ControlsButton")
        if btn then
            local audiocomp = GetComponent(btn, "AudioComponent")
            if audiocomp then audiocomp:Play() end
        end

        local PauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        local ControlsUIEntity = Engine.GetEntityByName("ControlsUI")

        if PauseUIEntity then
            local comp = GetComponent(PauseUIEntity, "ActiveComponent")
            if comp then comp.isActive = false end
        end
        if ControlsUIEntity then
            local comp = GetComponent(ControlsUIEntity, "ActiveComponent")
            if comp then comp.isActive = true end
        end
    end,
    
    OnClickSettingButton = function(self)
        local btn = Engine.GetEntityByName("SettingsButton")
        if btn then
            local audiocomp = GetComponent(btn, "AudioComponent")
            if audiocomp then audiocomp:Play() end
        end

        local PauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        local SettingsUIEntity = Engine.GetEntityByName("SettingsUI")

        if PauseUIEntity then
            local comp = GetComponent(PauseUIEntity, "ActiveComponent")
            if comp then comp.isActive = false end
        end
        if SettingsUIEntity then
            local comp = GetComponent(SettingsUIEntity, "ActiveComponent")
            if comp then comp.isActive = true end
        end
    end,

    OnClickMainMenuButton = function(self)
        local btn = Engine.GetEntityByName("MainMenuButton")
        if btn then
            local audiocomp = GetComponent(btn, "AudioComponent")
            if audiocomp then audiocomp:Play() end
        end

        local pauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        if pauseUIEntity then
            local pauseComp = GetComponent(pauseUIEntity, "ActiveComponent")
            if pauseComp then pauseComp.isActive = false end
        end

        local confirmUIEntity = Engine.GetEntityByName("ConfirmationPromptUI")
        if confirmUIEntity then
            local confirmComp = GetComponent(confirmUIEntity, "ActiveComponent")
            if confirmComp then confirmComp.isActive = true end
        end
    end,
}