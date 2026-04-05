--[[
================================================================================
CONFIRMATION MENU HANDLER
================================================================================
PURPOSE:
    Handles hover effects and click actions for confirmation prompt buttons.
    Audio is delegated to PauseMenuAudio via event_bus.

SINGLE RESPONSIBILITY: Handle button interactions. Audio via event_bus.
================================================================================
--]]

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

return Component {
    fields = {
        mainMenuScene = "Resources/Scenes/01_MainMenu.scene",
        -- Sprite GUIDs: [1] = normal, [2] = hover
        YesSpriteGUIDs = {},
        NoSpriteGUIDs = {},
    },

    Start = function(self)
        -- Setup button data with sprite swapping support
        self._buttonData = {}
        local buttonMapping = {
            { base = "YesButton", spriteGUIDs = self.YesSpriteGUIDs },
            { base = "NoButton", spriteGUIDs = self.NoSpriteGUIDs },
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
                --print("[ConfirmationMenuHandler] Warning: Missing entity " .. config.base)
            end
        end
    end,

    Update = function(self, dt)
        if not self._buttonData then return end

        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return end

        local mouseCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        local inputX, inputY = mouseCoordinate[1], mouseCoordinate[2]

        for _, data in pairs(self._buttonData) do
            local isHovering = inputX >= data.minX and inputX <= data.maxX and
                               inputY >= data.minY and inputY <= data.maxY

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
    end,

    OnClickYesButton = function(self)
        -- Publish click event for PauseMenuAudio
        if event_bus and event_bus.publish then
            event_bus.publish("pause_menu.click", {})
        end

        --print("[ConfirmationMenuHandler] Returning to main menu")
        Time.SetPaused(false)  -- Reset pause state before loading scene
        Time.SetTimeScale(1.0)  -- Reset time scale to normal
        Audio.SetBusPaused("BGM", false)  -- Unpause game buses before scene load
        Audio.SetBusPaused("SFX", false)
        Scene.Load(self.mainMenuScene)
    end,

    OnClickNoButton = function(self)
        -- Publish click event for PauseMenuAudio
        if event_bus and event_bus.publish then
            event_bus.publish("pause_menu.click", {})
        end

        -- Close Confirmation UI, open Pause UI
        local confirmUIEntity = Engine.GetEntityByName("ConfirmationPromptUI")
        local confirmComp = GetComponent(confirmUIEntity, "ActiveComponent")
        if confirmComp then confirmComp.isActive = false end

        local pauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        local pauseComp = GetComponent(pauseUIEntity, "ActiveComponent")
        if pauseComp then pauseComp.isActive = true end
    end,
}
