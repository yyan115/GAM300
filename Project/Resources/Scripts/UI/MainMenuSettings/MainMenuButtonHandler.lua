require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local MAIN_MENU_BUTTONS = {"PlayGame", "Credits", "ExitGame", "Settings"}
local MAIN_MENU_TEXTS   = {"PlayGameText", "SettingText", "CreditsText", "ExitGameText"}

local function setButtonsInteractable(interactable)
    for _, name in ipairs(MAIN_MENU_BUTTONS) do
        local e = Engine.GetEntityByName(name)
        if e then
            local btn = GetComponent(e, "ButtonComponent")
            if btn then btn.interactable = interactable end
        end
    end
end

local function setTextsActive(active)
    for _, name in ipairs(MAIN_MENU_TEXTS) do
        local e = Engine.GetEntityByName(name)
        if e then
            local comp = GetComponent(e, "ActiveComponent")
            if comp then comp.isActive = active end
        end
    end
end

-- Pre-position the SettingsUI slider notches/fills using the current saved
-- GameSettings values. We run this on scene load (before the UI is visible)
-- so when the user first clicks Settings the notches render at the correct
-- position immediately. Without this, MainMenuSettingsSlider's Start runs a
-- frame AFTER the entity becomes active (scripts on inactive entities don't
-- tick) and the slider visibly flashes from its prefab default to the real
-- value on first open.
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
    fields = {
        fadeDuration      = 1.0,
        bgmFadeInDuration = 2.0,
        fadeScreenName    = "MenuFadeScreen",
        targetScene       = "Resources/Scenes/02_IntroCutscene.scene",
        androidTargetScene = "Resources/Scenes/04_Level.scene",
        bgmEntityName     = "BGM"
    },
    _pendingScene    = nil,
    _isFading        = false,
    _fadeAlpha       = 0,
    _fadeTimer       = 0,
    _bgmAudio        = nil,
    _bgmInitialVolume = 1.0,
    _isFadingIn      = false,
    _fadeInTimer     = 0,

    Awake = function(self)
        -- Guard against double-Awake (hot-reload / stop-play cycle)
        local stale = {"_playGameSub", "_quitSub", "_settingsSub", "_creditsSub"}
        if _G.event_bus and _G.event_bus.unsubscribe then
            for _, key in ipairs(stale) do
                if self[key] then _G.event_bus.unsubscribe(self[key]); self[key] = nil end
            end
        end

        if not (_G.event_bus and _G.event_bus.subscribe) then return end
        local eb = _G.event_bus

        self._playGameSub = eb.subscribe("play_game_start", function()
            if self._isFading then return end
            self._isFadingIn = false
            self._isFading   = true
            self._fadeTimer  = 0
            self._fadeAlpha  = 0

            local fe = Engine.GetEntityByName(self.fadeScreenName)
            if fe then
                self._fadeActive = GetComponent(fe, "ActiveComponent")
                self._fadeSprite = GetComponent(fe, "SpriteRenderComponent")
                if self._fadeActive then self._fadeActive.isActive = true end
                if self._fadeSprite then
                    self._fadeSprite.color.x = 0
                    self._fadeSprite.color.y = 0
                    self._fadeSprite.color.z = 0
                    self._fadeSprite.alpha   = 0
                end
            end
            self._pendingScene = self.targetScene
        end)

        self._quitSub = eb.subscribe("quit_clicked", function()
            local e = Engine.GetEntityByName("QuitPromptUI")
            if e then
                local active = GetComponent(e, "ActiveComponent")
                if active then active.isActive = true end
            end
            setButtonsInteractable(false)
        end)

        self._settingsSub = eb.subscribe("settings_clicked", function()
            local e = Engine.GetEntityByName("SettingsUI")
            if e then
                local active = GetComponent(e, "ActiveComponent")
                if active then active.isActive = true end
            end
            setButtonsInteractable(false)
            setTextsActive(false)
        end)

        self._creditsSub = eb.subscribe("credits_clicked", function()
            local e = Engine.GetEntityByName("CreditsUI")
            if e then
                local active = GetComponent(e, "ActiveComponent")
                if active then active.isActive = true end
            end
            setButtonsInteractable(false)
        end)
    end,

    Start = function(self)
        -- Ensure game is not paused when entering main menu
        Time.SetPaused(false)
        Time.SetTimeScale(1.0)

        -- Pre-position settings sliders while SettingsUI is still hidden so
        -- the user's first open doesn't flash the prefab default position.
        prePositionSettingsSliders()

        self._pendingScene = nil
        self._isFading     = false
        self._fadeAlpha    = 0
        self._fadeTimer    = 0

        local fadeEntity = Engine.GetEntityByName(self.fadeScreenName)
        if fadeEntity then
            local fadeActive = GetComponent(fadeEntity, "ActiveComponent")
            local fadeSprite = GetComponent(fadeEntity, "SpriteRenderComponent")
            if fadeActive then fadeActive.isActive = false end
            if fadeSprite then
                fadeSprite.color.x = 0
                fadeSprite.color.y = 0
                fadeSprite.color.z = 0
                fadeSprite.alpha   = 0
            end
        end

        local bgmEntity = Engine.GetEntityByName(self.bgmEntityName)
        if bgmEntity then
            self._bgmAudio = GetComponent(bgmEntity, "AudioComponent")
            if self._bgmAudio then
                self._bgmInitialVolume = self._bgmAudio.Volume
                self._bgmAudio:SetVolume(0)
                self._isFadingIn  = true
                self._fadeInTimer = 0
            end
        end
    end,

    OnDisable = function(self)
        local subs = {"_playGameSub", "_quitSub", "_settingsSub", "_creditsSub"}
        if _G.event_bus and _G.event_bus.unsubscribe then
            for _, key in ipairs(subs) do
                if self[key] then _G.event_bus.unsubscribe(self[key]); self[key] = nil end
            end
        end
    end,

    -- Button callbacks: pure event publishers (logic lives in Awake subscribers above)
    OnClickPlayButton = function(self)
        if _G.event_bus then
            _G.event_bus.publish("main_menu.clickplaygame", {})
            _G.event_bus.publish("play_game_start", {})
        end
    end,

    OnClickQuitButton = function(self)
        if _G.event_bus then
            _G.event_bus.publish("main_menu.click", {})
            _G.event_bus.publish("quit_clicked", {})
        end
    end,

    OnClickSettingButton = function(self)
        if _G.event_bus then
            _G.event_bus.publish("main_menu.click", {})
            _G.event_bus.publish("settings_clicked", {})
        end
    end,

    OnClickCreditButton = function(self)
        if _G.event_bus then
            _G.event_bus.publish("main_menu.click", {})
            _G.event_bus.publish("credits_clicked", {})
        end
    end,

    Update = function(self, dt)
        -- BGM fade in on scene start
        if self._isFadingIn and self._bgmAudio then
            self._fadeInTimer = self._fadeInTimer + dt
            local progress = math.min(self._fadeInTimer / (self.bgmFadeInDuration or 2.0), 1.0)
            self._bgmAudio:SetVolume(self._bgmInitialVolume * progress)
            if progress >= 1.0 then self._isFadingIn = false end
        end

        -- Fade to black + scene load
        if self._isFading and self._fadeSprite then
            local highlightEntity = Engine.GetEntityByName("HighlightButtons")
            if highlightEntity then
                local highlight = GetComponent(highlightEntity, "ActiveComponent")
                if highlight then highlight.isActive = false end
            end

            for _, name in ipairs(MAIN_MENU_BUTTONS) do
                local e = Engine.GetEntityByName(name)
                if e then
                    local btn = GetComponent(e, "ButtonComponent")
                    if btn then btn.interactable = false end
                end
            end

            self._fadeTimer = self._fadeTimer + dt
            self._fadeAlpha = math.min(self._fadeTimer / (self.fadeDuration or 1.0), 1.0)
            self._fadeSprite.alpha = self._fadeAlpha

            if self._bgmAudio then
                self._bgmAudio:SetVolume(self._bgmInitialVolume * (1.0 - self._fadeAlpha))
            end

            if self._fadeAlpha >= 1.0 then
                self._isFading = false
                if self._pendingScene and Scene and Scene.Load then
                    Scene.Load(self._pendingScene)
                    self._pendingScene = nil
                end
            end
        end
    end
}
