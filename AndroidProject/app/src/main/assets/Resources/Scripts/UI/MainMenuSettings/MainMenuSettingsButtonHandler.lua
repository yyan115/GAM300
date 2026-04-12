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

-- Combined handler for CloseButton and ResetButton inside the SettingsUI panel.
-- Attach this script to the SettingsUI entity.
-- OnClickCloseButton / OnClickResetButton are ButtonComponent callbacks that may be
-- invoked on standalone instances (no Start/Update). They publish events so the
-- attached instance (which has initialized state and runs Update) handles the logic.
return Component {
    fields = {
        CloseSpriteGUIDs = {},  -- [1] normal, [2] hover
        ResetSpriteGUIDs = {},  -- [1] normal, [2] hover
    },

    Awake = function(self)
        -- Guard against double-Awake (hot-reload / stop-play cycle)
        local stale = {"_closeSub", "_resetSub"}
        if _G.event_bus and _G.event_bus.unsubscribe then
            for _, key in ipairs(stale) do
                if self[key] then _G.event_bus.unsubscribe(self[key]); self[key] = nil end
            end
        end

        if not (_G.event_bus and _G.event_bus.subscribe) then return end
        local eb = _G.event_bus

        self._closeSub = eb.subscribe("settings_close_clicked", function()
            local isActive = self._settingsUIActive and self._settingsUIActive.isActive
            if not isActive then return end

            if self._closeButton then self._closeButton.interactable = false end
            if self._resetButton then self._resetButton.interactable = false end
            self:_resetHoverVisuals()

            GameSettings.SaveIfDirty()
            setButtonsInteractable(true)
            setTextsActive(true)

            if self._settingsUIActive then self._settingsUIActive.isActive = false end
            -- Force rising-edge detection on next open: Update() stops while entity is
            -- disabled, so _wasSettingsActive stays true and rising edge is never seen.
            self._wasSettingsActive = false
        end)

        self._resetSub = eb.subscribe("settings_reset_clicked", function()
            local isActive = self._settingsUIActive and self._settingsUIActive.isActive
            if not isActive then return end

            GameSettings.Init()
            GameSettings.ResetToDefaults()
            eb.publish("settings_reset", {})
        end)
    end,

    Start = function(self)
        self._isCloseHovered     = false
        self._isResetHovered     = false
        self._wasSettingsActive  = false

        local settingsUIEntity = Engine.GetEntityByName("SettingsUI")
        if settingsUIEntity then
            self._settingsUIActive = GetComponent(settingsUIEntity, "ActiveComponent")
        end

        local closeEntity = Engine.GetEntityByName("CloseButton")
        if closeEntity then
            self._closeTransform = GetComponent(closeEntity, "Transform")
            self._closeSprite    = GetComponent(closeEntity, "SpriteRenderComponent")
            self._closeButton    = GetComponent(closeEntity, "ButtonComponent")
        end

        local resetEntity = Engine.GetEntityByName("ResetButton")
        if resetEntity then
            self._resetTransform = GetComponent(resetEntity, "Transform")
            self._resetSprite    = GetComponent(resetEntity, "SpriteRenderComponent")
            self._resetButton    = GetComponent(resetEntity, "ButtonComponent")
        end

        -- Start non-interactable; enabled when SettingsUI opens
        if self._closeButton then self._closeButton.interactable = false end
        if self._resetButton then self._resetButton.interactable = false end
    end,

    OnDisable = function(self)
        local subs = {"_closeSub", "_resetSub"}
        if _G.event_bus and _G.event_bus.unsubscribe then
            for _, key in ipairs(subs) do
                if self[key] then _G.event_bus.unsubscribe(self[key]); self[key] = nil end
            end
        end
    end,

    Update = function(self, dt)
        local isActive = self._settingsUIActive and self._settingsUIActive.isActive

        -- Rising edge: SettingsUI just became visible. Also check C++ justActivated
        -- flag since _wasSettingsActive can't be cleared during dormancy.
        local justActivated = self._settingsUIActive and self._settingsUIActive.justActivated
        if isActive and (not self._wasSettingsActive or justActivated) then
            self._isCloseHovered = false
            self._isResetHovered = false
            self:_resetHoverVisuals()
            if self._closeButton then self._closeButton.interactable = true end
            if self._resetButton then self._resetButton.interactable = true end
        end

        self._wasSettingsActive = isActive
        if not isActive then return end

        self:_updateHover()
    end,

    _resetHoverVisuals = function(self)
        if self._closeSprite and self.CloseSpriteGUIDs and self.CloseSpriteGUIDs[1] then
            self._closeSprite:SetTextureFromGUID(self.CloseSpriteGUIDs[1])
        end
        if self._resetSprite and self.ResetSpriteGUIDs and self.ResetSpriteGUIDs[1] then
            self._resetSprite:SetTextureFromGUID(self.ResetSpriteGUIDs[1])
        end
    end,

    _checkHover = function(self, transform)
        if not transform then return false end
        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return false end
        local mc    = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        local x, y  = mc[1], mc[2]
        local pos   = transform.localPosition
        local scale = transform.localScale
        return x >= pos.x - scale.x * 0.5 and x <= pos.x + scale.x * 0.5
           and y >= pos.y - scale.y * 0.5 and y <= pos.y + scale.y * 0.5
    end,

    _updateHover = function(self)
        local function updateButton(isHoveredKey, transform, sprite, guids)
            local hovering = self:_checkHover(transform)
            if hovering and not self[isHoveredKey] then
                self[isHoveredKey] = true
                if _G.event_bus then _G.event_bus.publish("main_menu.hover", {}) end
                if sprite and guids and guids[2] then sprite:SetTextureFromGUID(guids[2]) end
            elseif not hovering and self[isHoveredKey] then
                self[isHoveredKey] = false
                if sprite and guids and guids[1] then sprite:SetTextureFromGUID(guids[1]) end
            end
        end

        updateButton("_isCloseHovered", self._closeTransform, self._closeSprite, self.CloseSpriteGUIDs)
        updateButton("_isResetHovered", self._resetTransform, self._resetSprite, self.ResetSpriteGUIDs)
    end,

    -- Button callbacks: pure event publishers (logic lives in Awake subscribers above)
    OnClickCloseButton = function(self)
        if _G.event_bus then
            _G.event_bus.publish("main_menu.click", {})
            _G.event_bus.publish("settings_close_clicked", {})
        end
    end,

    OnClickResetButton = function(self)
        if _G.event_bus then
            _G.event_bus.publish("main_menu.click", {})
            _G.event_bus.publish("settings_reset_clicked", {})
        end
    end,
}
