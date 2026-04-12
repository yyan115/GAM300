--[[
================================================================================
CONTROLS MENU BUTTON HANDLER
================================================================================
PURPOSE:
    Handles hover effects and click actions for controls menu buttons.
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
            { base = "ControlsBackButton", spriteGUIDs = self.BackSpriteGUIDs },
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
                --print("[ControlssMenuButtonHandler] Warning: Missing entity " .. config.base)
            end
        end
    end,

    Update = function(self, dt)
        if not self._buttonData then return end

        local pageEntity = Engine.GetEntityByName("ControlsUI")
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

        local ac = Engine.GetEntityByName("ControlsUI")
        local acComp = ac and GetComponent(ac, "ActiveComponent")
        local justBecameActive = not self._pageWasActive or (acComp and acComp.justActivated)
        self._pageWasActive = true

        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return end

        local mouseCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        local inputX, inputY = mouseCoordinate[1], mouseCoordinate[2]

        for _, data in pairs(self._buttonData) do
            local isHovering = inputX >= data.minX and inputX <= data.maxX and
                               inputY >= data.minY and inputY <= data.maxY

            if justBecameActive then
                -- First frame page is visible: force-reset sprite to match the
                -- current hover state. Update() doesn't run while the entity is
                -- disabled, so any stale hover sprite from the previous session
                -- would otherwise persist until the user hovers off the button.
                if data.sprite and data.spriteGUIDs then
                    if isHovering and data.spriteGUIDs[2] then
                        data.sprite:SetTextureFromGUID(data.spriteGUIDs[2])
                    elseif data.spriteGUIDs[1] then
                        data.sprite:SetTextureFromGUID(data.spriteGUIDs[1])
                    end
                end
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

    OnClickBackButton = function(self)
        -- Publish click event for PauseMenuAudio
        if event_bus and event_bus.publish then
            event_bus.publish("pause_menu.click", {})
        end

        -- Disable Controls UI, enable Pause UI
        local ControlsUIEntity = Engine.GetEntityByName("ControlsUI")
        local ControlsComp = GetComponent(ControlsUIEntity, "ActiveComponent")
        if ControlsComp then ControlsComp.isActive = false end

        local PauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        local PauseComp = GetComponent(PauseUIEntity, "ActiveComponent")
        if PauseComp then PauseComp.isActive = true end
    end,
}
