require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        fadeDuration = 1.0,
        fadeScreenName = "MenuFadeScreen",
        targetScene = "Resources/Scenes/M3_Gameplay.scene"
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
                self._fadeSprite.alpha = 0
            end
        end

        self._pendingScene = self.targetScene
    end,

    -- Quit Button Function
    OnClickQuitButton = function(self)
        -- Play click SFX
        self:_playClickSFX("ExitGame")
        Screen.RequestClose()
    end,

    -- Settings Button Function
    OnClickSettingButton = function(self)
        -- Play click SFX
        self:_playClickSFX("Settings")

        local settingUIEntity = Engine.GetEntityByName("SettingsUI")
        local settingUI = GetComponent(settingUIEntity, "ActiveComponent")
        settingUI.isActive = true

        -- Disable buttons when settings menu is active
        local targetButtons = {"PlayGame", "Credits", "ExitGame", "Settings"}
        for _, buttonName in ipairs(targetButtons) do
            local entity = Engine.GetEntityByName(buttonName)
            local button = GetComponent(entity, "ButtonComponent")
            button.interactable = false
        end
    end,

    -- Credits Button Function
    OnClickCreditButton = function(self)
        -- Play click SFX
        self:_playClickSFX("Credits")
        -- Add credits logic here
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
                    Scene.Load(self._pendingScene)
                    self._pendingScene = nil
                end
            end
        end
    end
}
