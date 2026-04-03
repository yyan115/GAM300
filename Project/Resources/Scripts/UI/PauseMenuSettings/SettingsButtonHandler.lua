--[[
================================================================================
SETTINGS BUTTON HANDLER
================================================================================
PURPOSE:
    Handles hover effects and sprite swapping for the settings menu buttons
    (CloseButton and ResetButton). Audio is delegated to PauseMenuAudio via
    event_bus.

SINGLE RESPONSIBILITY: Handle button sprite/hover interactions. Audio via event_bus.
================================================================================
--]]

require("extension.engine_bootstrap")
local event_bus = _G.event_bus
local Component = require("extension.mono_helper")

return Component {
    fields = {
        -- Sprite GUIDs: [1] = normal, [2] = hover
        CloseSpriteGUIDs = {},
        ResetSpriteGUIDs = {},
    },

    Start = function(self)
        self._pageWasActive = false

        self._buttonData = {}
        local buttonMapping = {
            { base = "CloseButton", spriteGUIDs = self.CloseSpriteGUIDs },
            { base = "ResetButton", spriteGUIDs = self.ResetSpriteGUIDs },
        }

        for index, config in ipairs(buttonMapping) do
            local baseEnt = Engine.GetEntityByName(config.base)

            if baseEnt then
                local transform = GetComponent(baseEnt, "Transform")
                local sprite = GetComponent(baseEnt, "SpriteRenderComponent")

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
                    print("[SettingsButtonHandler] Warning: Missing Transform or Sprite on " .. config.base)
                end
            else
                print("[SettingsButtonHandler] Warning: Missing entity " .. config.base)
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
                data.wasHovered = isHovering
            else
                if isHovering and not data.wasHovered then
                    if event_bus and event_bus.publish then
                        event_bus.publish("pause_menu.hover", {})
                    end
                    if data.sprite and data.spriteGUIDs and data.spriteGUIDs[2] then
                        data.sprite:SetTextureFromGUID(data.spriteGUIDs[2])
                    end
                elseif not isHovering and data.wasHovered then
                    if data.sprite and data.spriteGUIDs and data.spriteGUIDs[1] then
                        data.sprite:SetTextureFromGUID(data.spriteGUIDs[1])
                    end
                end

                data.wasHovered = isHovering
            end
        end
    end,
}
