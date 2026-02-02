require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        fadeDuration = 1.0,
        fadeScreenName = "MenuFadeScreen",
        targetScene = "Resources/Scenes/IntroCutScene.scene"
    },
    _pendingScene = nil,
    _isFading = false,
    _fadeAlpha = 0,
    _fadeTimer = 0,

    Start = function(self)
        -- Reset state for scene reloads
        self._pendingScene = nil
        self._isFading = false
        self._fadeAlpha = 0
        self._fadeTimer = 0

        local fadeEntity = Engine.GetEntityByName(self.fadeScreenName)
        if fadeEntity then
            local fadeActive = GetComponent(fadeEntity, "ActiveComponent")
            local fadeSprite = GetComponent(fadeEntity, "SpriteRenderComponent")
            if fadeActive then
                fadeActive.isActive = false
            end
            if fadeSprite then
                -- Set color to black for fade-to-black effect
                fadeSprite.color.x = 0
                fadeSprite.color.y = 0
                fadeSprite.color.z = 0
                fadeSprite.alpha = 0
            end
        end
    end,

    -- Play Button Function
    OnClickPlayButton = function(self)
        -- Play click SFX
        self:_playClickSFX("PlayGame")

        -- Start fade transition
        self._isFading = true
        self._fadeTimer = 0
        self._fadeAlpha = 0

        -- Enable and setup the fade screen
        local fadeEntity = Engine.GetEntityByName(self.fadeScreenName)
        if fadeEntity then
            self._fadeActive = GetComponent(fadeEntity, "ActiveComponent")
            self._fadeSprite = GetComponent(fadeEntity, "SpriteRenderComponent")
            if self._fadeActive then
                self._fadeActive.isActive = true
            end
            if self._fadeSprite then
                -- Set color to black for fade-to-black effect
                self._fadeSprite.color.x = 0
                self._fadeSprite.color.y = 0
                self._fadeSprite.color.z = 0
                self._fadeSprite.alpha = 0
            end
        end

        self._pendingScene = self.targetScene
    end,

    -- Quit Button Function
    OnClickQuitButton = function(self)
        -- Play click SFX
        self:_playClickSFX("ExitGame")
        if Screen and Screen.RequestClose then
            Screen.RequestClose()
        else
            print("[MainMenuButtonHandler] Warning: Screen.RequestClose missing")
        end
    end,

    -- Settings Button Function
    OnClickSettingButton = function(self)
        -- Play click SFX
        self:_playClickSFX("Settings")

        local settingUIEntity = Engine.GetEntityByName("SettingsUI")
        if settingUIEntity then
            local settingUI = GetComponent(settingUIEntity, "ActiveComponent")
            if settingUI then
                settingUI.isActive = true
            else
                print("[MainMenuButtonHandler] Warning: SettingsUI ActiveComponent missing")
            end
        else
            print("[MainMenuButtonHandler] Warning: SettingsUI entity not found")
        end

        -- Disable buttons and hide text when settings menu is active
        local targetButtons = {"PlayGame", "Credits", "ExitGame", "Settings"}
        local targetTexts = {"PlayGameText", "SettingText", "CreditsText", "ExitGameText"}
        for _, buttonName in ipairs(targetButtons) do
            local entity = Engine.GetEntityByName(buttonName)
            if entity then
                local button = GetComponent(entity, "ButtonComponent")
                if button then
                    button.interactable = false
                else
                    print("[MainMenuButtonHandler] Warning: ButtonComponent missing on " .. buttonName)
                end
            else
                print("[MainMenuButtonHandler] Warning: Button entity " .. buttonName .. " not found")
            end
        end
        -- Hide button text to prevent z-order issues with SettingsUI overlay
        for _, textName in ipairs(targetTexts) do
            local textEntity = Engine.GetEntityByName(textName)
            if textEntity then
                local textActive = GetComponent(textEntity, "ActiveComponent")
                if textActive then
                    textActive.isActive = false
                end
            end
        end
    end,

    -- Credits Button Function
    OnClickCreditButton = function(self)
        -- Play click SFX
        self:_playClickSFX("Credits")

        -- Enable CreditsUI
        local creditsUIEntity = Engine.GetEntityByName("CreditsUI")
        if creditsUIEntity then
            local creditsUI = GetComponent(creditsUIEntity, "ActiveComponent")
            if creditsUI then
                creditsUI.isActive = true
            end
        end

        -- Disable main menu buttons and hide text when credits is active
        local targetButtons = {"PlayGame", "Credits", "ExitGame", "Settings"}
        local targetTexts = {"PlayGameText", "SettingText", "CreditsText", "ExitGameText"}
        for _, buttonName in ipairs(targetButtons) do
            local entity = Engine.GetEntityByName(buttonName)
            if entity then
                local button = GetComponent(entity, "ButtonComponent")
                if button then
                    button.interactable = false
                end
            end
        end
        -- Hide button text to prevent z-order issues with CreditsUI overlay
        for _, textName in ipairs(targetTexts) do
            local textEntity = Engine.GetEntityByName(textName)
            if textEntity then
                local textActive = GetComponent(textEntity, "ActiveComponent")
                if textActive then
                    textActive.isActive = false
                end
            end
        end
    end,

    -- Helper function to play click SFX from a named button entity
    _playClickSFX = function(self, entityName)
        if not entityName then return end
        local entity = Engine.GetEntityByName(entityName)
        if not entity then return end

        local audio = GetComponent(entity, "AudioComponent")
        if audio then
            audio:Play()
        end
    end,

    -- Update handles fade transition and scene loading
    Update = function(self, dt)
        -- Handle fade transition
        if self._isFading and self._fadeSprite then
            self._fadeTimer = self._fadeTimer + dt
            local duration = self.fadeDuration or 1.0
            self._fadeAlpha = math.min(self._fadeTimer / duration, 1.0)
            self._fadeSprite.alpha = self._fadeAlpha

            -- Once fade is complete, load the scene
            if self._fadeAlpha >= 1.0 then
                self._isFading = false
                if self._pendingScene then
                    if Scene and Scene.Load then
                        Scene.Load(self._pendingScene)
                    else
                        print("[MainMenuButtonHandler] Warning: Scene.Load missing, cannot load " .. tostring(self._pendingScene))
                    end
                    self._pendingScene = nil
                end
            end
        end
    end
}
