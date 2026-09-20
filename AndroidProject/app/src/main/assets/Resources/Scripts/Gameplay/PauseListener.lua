require("extension.engine_bootstrap")

local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

-- Pre-position the SettingsUI slider notches/fills using the current saved
-- GameSettings values. We run this on scene load (while SettingsUI is still
-- hidden) so when the user first opens the pause settings, the notches/fills
-- render at the correct position immediately. Without this, SettingsSlider's
-- Start runs a frame AFTER the entity becomes active (scripts on inactive
-- entities don't tick) and the slider visibly flashes from its prefab default
-- to the real value on first open.
local SLIDER_DEFS = {
    { prefix = "Master", getter = function() return GameSettings.GetMasterVolume() end, min = 0.0, max = 1.0 },
    { prefix = "BGM",    getter = function() return GameSettings.GetBGMVolume()    end, min = 0.0, max = 1.0 },
    { prefix = "SFX",    getter = function() return GameSettings.GetSFXVolume()    end, min = 0.0, max = 1.0 },
    { prefix = "Gamma",  getter = function() return GameSettings.GetGamma()        end, min = 1.0, max = 3.0 },
}

local function prePositionSettingsSliders()
    if not GameSettings then return end
    GameSettings.Init()
    for _, def in ipairs(SLIDER_DEFS) do
        local notchEnt = Engine.GetEntityByName(def.prefix .. "Notch")
        local fillEnt  = Engine.GetEntityByName(def.prefix .. "Fill")
        if notchEnt and fillEnt then
            local notchTr = GetComponent(notchEnt, "Transform")
            local fillTr  = GetComponent(fillEnt,  "Transform")
            local fillSp  = GetComponent(fillEnt,  "SpriteRenderComponent")
            if notchTr and fillTr then
                local offsetX = fillTr.localScale.x / 2.0
                local minX    = fillTr.localPosition.x - offsetX
                local maxX    = fillTr.localPosition.x + offsetX
                local value   = def.getter()
                local normalized = (value - def.min) / (def.max - def.min)
                if normalized < 0 then normalized = 0 end
                if normalized > 1 then normalized = 1 end
                notchTr.localPosition.x = minX + (normalized * (maxX - minX))
                notchTr.isDirty = true
                if fillSp then fillSp.fillValue = normalized end
            end
        end
    end
end

return Component {
    mixins = { TransformMixin },

    fields = {
    },

    Start = function(self)
        self._isGamePaused = false
        self._pauseTimer = 0  -- Initialize timer

        -- Pre-position settings sliders while SettingsUI is still hidden so
        -- the user's first open doesn't flash the prefab default position.
        prePositionSettingsSliders()

        local pauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        self._pauseComp = GetComponent(pauseUIEntity, "ActiveComponent")

        local settingsUIEntity = Engine.GetEntityByName("SettingsUI")
        self._settingsComp = GetComponent(settingsUIEntity, "ActiveComponent")

        local confirmUIEntity = Engine.GetEntityByName("ConfirmationPromptUI")
        self._confirmComp = GetComponent(confirmUIEntity, "ActiveComponent")

        local controlsUIEntity = Engine.GetEntityByName("ControlsUI")
        self._controlsComp = controlsUIEntity and GetComponent(controlsUIEntity, "ActiveComponent") or nil

        local blackScreenUIEntity = Engine.GetEntityByName("BlackScreen")
        self._blackScreenComp = GetComponent(blackScreenUIEntity, "ActiveComponent")

        -- Cache pause menu button components for direct control
        -- This ensures buttons are enabled in the same callback that opens the menu
        self._pauseButtons = {}
        local pauseButtonNames = {"ContinueButton", "ControlsButton", "SettingsButton", "MainMenuButton"}
        for _, name in ipairs(pauseButtonNames) do
            local buttonEntity = Engine.GetEntityByName(name)
            if buttonEntity then
                local buttonComp = GetComponent(buttonEntity, "ButtonComponent")
                if buttonComp then
                    self._pauseButtons[name] = buttonComp
                end
            end
        end

        -- Subscribe to player dead and player respawn events to prevent pausing when the player is dead.
        self._playerDead = false
        self._playerDeadSub = event_bus.subscribe("playerDead", function(dead)
            self._playerDead = true
        end)

        self._respawnPlayerSub = event_bus.subscribe("respawnPlayer", function(respawn)
            self._playerDead = false
        end)
    end,

    -- Opens the pause menu: the game stops, its audio stops, and the menu's
    -- buttons are made live in the same frame, since a button enabled a frame
    -- later cannot be clicked on the frame the menu appears.
    _OpenPauseMenu = function(self)
        self._pauseComp.isActive = true
        Time.SetPaused(true)

        for _, buttonComp in pairs(self._pauseButtons) do
            if buttonComp then
                buttonComp.interactable = true
            end
        end

        -- Pause all game audio (UI on "UI" bus still plays)
        Audio.SetBusPaused("BGM", true)
        Audio.SetBusPaused("SFX", true)

        if event_bus and event_bus.publish then
            event_bus.publish("game_paused", true)
        end
    end,

    -- True while any of the menus is up
    _AnyMenuOpen = function(self)
        return self._pauseComp.isActive or self._settingsComp.isActive
            or self._confirmComp.isActive
            or (self._controlsComp and self._controlsComp.isActive) or false
    end,

    Update = function(self, dt)
        if not self._pauseComp or not self._settingsComp or not self._confirmComp then
            return
        end

        -- Leaving a fullscreen game brings the pause menu up, so it comes back
        -- with the menu rather than dropping the player into a fight they were
        -- not watching. A windowed game is left alone: he plays it beside
        -- other work on purpose.
        local focused = true
        if Screen and Screen.IsFocused then focused = Screen.IsFocused() end
        local fullscreen = false
        if Screen and Screen.IsFullscreen then fullscreen = Screen.IsFullscreen() end
        if self._wasFocused == nil then self._wasFocused = focused end
        if self._wasFocused and not focused and fullscreen
           and not self:_AnyMenuOpen() and not self._playerDead then
            self:_OpenPauseMenu()
        end
        self._wasFocused = focused

        -- Use unscaled delta time for the cooldown timer so it works even when paused
        local unscaledDt = Time.GetUnscaledDeltaTime()
        if self._pauseTimer > 0 then
            self._pauseTimer = self._pauseTimer - unscaledDt
        end

        local isPressed = Input.IsActionPressed("Pause")
        -- Only fire if the button is pressed AND our cooldown has expired AND the player is not dead
        if isPressed and self._pauseTimer <= 0 and not self._playerDead then
            self._pauseTimer = 0.1  -- Cooldown to prevent double-fire

            local onSubPage = self._settingsComp.isActive or
                              (self._controlsComp and self._controlsComp.isActive)

            if onSubPage then
                if self._settingsComp.isActive then
                    if GameSettings then GameSettings.SaveIfDirty() end
                    self._settingsComp.isActive = false
                end
                if self._controlsComp then self._controlsComp.isActive = false end
                self._pauseComp.isActive = true
                for _, buttonComp in pairs(self._pauseButtons) do
                    if buttonComp then buttonComp.interactable = true end
                end
                if event_bus and event_bus.publish then
                    event_bus.publish("pause_menu.click", {})
                end
            else

            if self._confirmComp.isActive then
                self._confirmComp.isActive = false
                self._pauseComp.isActive = true  -- Go back to Pause menu
                -- Enable pause buttons immediately
                for _, buttonComp in pairs(self._pauseButtons) do
                    if buttonComp then buttonComp.interactable = true end
                end

            elseif self._pauseComp.isActive then
                -- Unpause game
                self._pauseComp.isActive = false
                Time.SetPaused(false)

                -- Re-lock cursor when unpausing via Esc
                if Screen then Screen.SetCursorLocked(true) end

                -- Unpause all game audio
                Audio.SetBusPaused("BGM", false)
                Audio.SetBusPaused("SFX", false)

                if event_bus and event_bus.publish then
                    event_bus.publish("game_paused", false)
                end
            else
                self:_OpenPauseMenu()
            end
            end  -- if not onSubPage
        end

        local isMenuActive = self._confirmComp.isActive or self._settingsComp.isActive or self._pauseComp.isActive or
                             (self._controlsComp and self._controlsComp.isActive)

        if isMenuActive then
            self._blackScreenComp.isActive = true
            if Screen and Screen.IsCursorLocked() then
                Screen.SetCursorLocked(false)
            end
        else
            self._blackScreenComp.isActive = false
        end
    end,
}
