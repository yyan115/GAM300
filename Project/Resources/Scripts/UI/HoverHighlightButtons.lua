require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local targetButtons = 
{   "PlayGame", 
    "Credits", 
    "ExitGame",
    "Settings" 
}

local UIMenus = 
{
    "SettingsUI",
    "CreditsUI",
    "QuitPromptUI"
}

return Component {
    Start = function(self)
        self.lastState = 1
        self.buttonBounds = {}
        self.UIState = {}
        self._lockedState = false

        self._audio = self:GetComponent("AudioComponent")

        for index, value in ipairs(targetButtons) do
            local targetEntity = Engine.GetEntityByName(value)
            if targetEntity then
                local transform = GetComponent(targetEntity, "Transform")
                local pos   = transform.localPosition
                local scale = transform.localScale

                local component = GetComponent(targetEntity, "SpriteRenderComponent")

                local buttonChild = Engine.GetChildAtIndex(targetEntity, 0)
                local textComponent = nil
                if buttonChild then
                    textComponent = GetComponent(buttonChild, "TextRenderComponent")
                end

                self.buttonBounds[index] = {
                    spriteComponent = component,
                    textComponent = textComponent,
                    minX = pos.x - (scale.x / 2),
                    maxX = pos.x + (scale.x / 2),
                    minY = pos.y - (scale.y / 2),
                    maxY = pos.y + (scale.y / 2)
                }

                if textComponent then
                    textComponent.color.x = 0.8
                    textComponent.color.y = 0.8
                    textComponent.color.z = 0.8
                end

                if component then
                    component.isVisible = (index == self.lastState)
                end
            end
        end

        for index, value in ipairs(UIMenus) do
            local UIEntity = Engine.GetEntityByName(value)
            if UIEntity then
                local activeComponent = GetComponent(UIEntity, "ActiveComponent")
                self.UIState[index] = {
                    component = activeComponent
                }
            end
        end
    end,

    Update = function(self, dt)

        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return end

        local mouseCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        if not mouseCoordinate then return end

        local inputX = mouseCoordinate[1]
        local inputY = mouseCoordinate[2]
        if not inputX or not inputY then return end

        local anyUIOpen = false
        for _, states in ipairs(self.UIState) do
            if states.component and states.component.isActive then
                anyUIOpen = true
                break
            end
        end

        if anyUIOpen then
            return
        end

        self._lockedState = false

        local hoveredIndex = nil

        for index, bounds in ipairs(self.buttonBounds) do
            if bounds.minX and bounds.maxX and bounds.minY and bounds.maxY and
               inputX >= bounds.minX and inputX <= bounds.maxX and
               inputY >= bounds.minY and inputY <= bounds.maxY then

                hoveredIndex = index

                if Input.IsPointerPressed() then
                    self._lockedState = true
                end

                break
            end
        end

        if not hoveredIndex then
            for _, data in ipairs(self.buttonBounds) do
                if data.spriteComponent then
                    data.spriteComponent.isVisible = false
                end
                if data.textComponent then
                    data.textComponent.color.x = 0.8
                    data.textComponent.color.y = 0.8
                    data.textComponent.color.z = 0.8
                end
            end
            return
        end

        if hoveredIndex == self.lastState then return end

        local oldData = self.buttonBounds[self.lastState]
        local newData = self.buttonBounds[hoveredIndex]

        if oldData then
            if oldData.spriteComponent then
                oldData.spriteComponent.isVisible = false
            end
            if oldData.textComponent then
                oldData.textComponent.color.x = 0.8
                oldData.textComponent.color.y = 0.8
                oldData.textComponent.color.z = 0.8
            end
        end

        if newData then
            if newData.spriteComponent then
                newData.spriteComponent.isVisible = true
            end
            if newData.textComponent then
                newData.textComponent.color.x = 0.0
                newData.textComponent.color.y = 0.0
                newData.textComponent.color.z = 0.0
            end
        end

        self.lastState = hoveredIndex

        if self._audio then
            self._audio:Play()
        end
    end,
}