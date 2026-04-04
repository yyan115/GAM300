--[[
================================================================================
SETTINGS MENU BUTTON HANDLER
================================================================================
PURPOSE:
    Handles hover effects and click actions for settings menu buttons.
    Audio is delegated to PauseMenuAudio via event_bus.

SINGLE RESPONSIBILITY: Handle button interactions. Audio via event_bus.
================================================================================
--]]

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

return Component {
    fields = {
        -- Sprite GUIDs: [1] = normal, [2] = hover
        ResetSpriteGUIDs = {},
        BackSpriteGUIDs = {},
    },

    Start = function(self)
        -- Initialize GameSettings (safe to call multiple times)
        if GameSettings then
            GameSettings.Init()
        end

        self._pageWasActive = false

        -- Setup button data with sprite swapping support
        self._buttonData = {}
        local buttonMapping = {
            { base = "ResetButton", spriteGUIDs = self.ResetSpriteGUIDs },
            { base = "BackButton", spriteGUIDs = self.BackSpriteGUIDs },
        }

        for index, config in ipairs(buttonMapping) do
            local baseEnt = Engine.GetEntityByName(config.base)

            if baseEnt then
                local transform = GetComponent(baseEnt, "Transform")
                local sprite = GetComponent(baseEnt, "SpriteRenderComponent")
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
                --print("[SettingsMenuButtonHandler] Warning: Missing entity " .. config.base)
            end
        end
    end,

    Update = function(self, dt)
        if not self._buttonData then return end

        local pageEntity = Engine.GetEntityByName("SettingsUI")
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
                    -- Publish hover event for PauseMenuAudio
                    if event_bus and event_bus.publish then
                        event_bus.publish("pause_menu.hover", {})
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

    -- Reset all settings to defaults
    OnClickResetButton = function(self)
        -- Publish click event for PauseMenuAudio
        if event_bus and event_bus.publish then
            event_bus.publish("pause_menu.click", {})
        end

        -- Initialize GameSettings (safe to call multiple times)
        GameSettings.Init()

        -- Reset to defaults (this also applies and saves via C++)
        GameSettings.ResetToDefaults()

        -- Notify all sliders to update their positions
        if event_bus then
            event_bus.publish("settings_reset", {})
        end

        --print("[SettingsMenuButtonHandler] All settings reset to defaults")
    end,

    OnClickBackButton = function(self)
        -- Publish click event for PauseMenuAudio
        if event_bus and event_bus.publish then
            event_bus.publish("pause_menu.click", {})
        end

        -- Disable Settings UI, enable Pause UI
        local SettingsUIEntity = Engine.GetEntityByName("SettingsUI")
        local SettingsComp = GetComponent(SettingsUIEntity, "ActiveComponent")
        if SettingsComp then SettingsComp.isActive = false end

        local PauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        local PauseComp = GetComponent(PauseUIEntity, "ActiveComponent")
        if PauseComp then PauseComp.isActive = true end
    end,

}
