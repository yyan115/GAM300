require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local TARGET_BUTTONS = {"PlayGame", "Credits", "ExitGame", "Settings"}
local UI_MENUS       = {"SettingsUI", "CreditsUI", "QuitPromptUI"}

local NORMAL_TEXT_COLOR  = {0.8, 0.8, 0.8}
local HOVERED_TEXT_COLOR = {0.0, 0.0, 0.0}

local function applyTextColor(textComponent, color)
    if not textComponent then return end
    textComponent.color.x = color[1]
    textComponent.color.y = color[2]
    textComponent.color.z = color[3]
end

return Component {
    Start = function(self)
        self.lastState    = nil
        self.buttonBounds = {}
        self.UIState      = {}
        self._lockedState = false

        for index, name in ipairs(TARGET_BUTTONS) do
            local e = Engine.GetEntityByName(name)
            if e then
                local transform = GetComponent(e, "Transform")
                local pos, scale = transform.localPosition, transform.localScale
                local sprite     = GetComponent(e, "SpriteRenderComponent")

                local child = Engine.GetChildAtIndex(e, 0)
                local text  = child and GetComponent(child, "TextRenderComponent")

                self.buttonBounds[index] = {
                    spriteComponent = sprite,
                    textComponent   = text,
                    minX = pos.x - scale.x * 0.5,
                    maxX = pos.x + scale.x * 0.5,
                    minY = pos.y - scale.y * 0.5,
                    maxY = pos.y + scale.y * 0.5,
                }

                applyTextColor(text, NORMAL_TEXT_COLOR)
                if sprite then sprite.isVisible = false end
            end
        end

        for index, name in ipairs(UI_MENUS) do
            local e = Engine.GetEntityByName(name)
            if e then
                self.UIState[index] = { component = GetComponent(e, "ActiveComponent") }
            end
        end
    end,

    Update = function(self, dt)
        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return end

        local mc = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        if not mc then return end
        local inputX, inputY = mc[1], mc[2]
        if not inputX or not inputY then return end

        -- Suppress hover when any overlay is open
        for _, state in ipairs(self.UIState) do
            if state.component and state.component.isActive then return end
        end

        self._lockedState = false

        -- Find hovered button
        local hoveredIndex = nil
        for index, bounds in ipairs(self.buttonBounds) do
            if inputX >= bounds.minX and inputX <= bounds.maxX and
               inputY >= bounds.minY and inputY <= bounds.maxY then
                hoveredIndex = index
                if Input.IsPointerPressed() then self._lockedState = true end
                break
            end
        end

        -- No hover: hide all highlights and clear tracked state
        if not hoveredIndex then
            for _, data in ipairs(self.buttonBounds) do
                if data.spriteComponent then data.spriteComponent.isVisible = false end
                applyTextColor(data.textComponent, NORMAL_TEXT_COLOR)
            end
            self.lastState = nil  -- reset so re-entering the same button triggers hover again
            return
        end

        -- Same button as before: nothing to do
        if hoveredIndex == self.lastState then return end

        -- Deactivate old, activate new
        local oldData = self.buttonBounds[self.lastState]
        if oldData then
            if oldData.spriteComponent then oldData.spriteComponent.isVisible = false end
            applyTextColor(oldData.textComponent, NORMAL_TEXT_COLOR)
        end

        local newData = self.buttonBounds[hoveredIndex]
        if newData then
            if newData.spriteComponent then newData.spriteComponent.isVisible = true end
            applyTextColor(newData.textComponent, HOVERED_TEXT_COLOR)
        end

        self.lastState = hoveredIndex

        -- Hover SFX via centralised audio handler
        if _G.event_bus then _G.event_bus.publish("main_menu.hover", {}) end
    end,
}
