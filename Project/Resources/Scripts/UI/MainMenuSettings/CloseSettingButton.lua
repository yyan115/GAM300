require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    Start = function(self)
        local closeEntity = Engine.GetEntityByName("CloseButton")
        if closeEntity then
            self._audio = GetComponent(closeEntity, "AudioComponent")
            self._transform = GetComponent(closeEntity, "Transform")
        end

        self._isHovered = false
    end,

    Update = function(self, dt)
        if not self._transform then return end

        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return end

        local mouseCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        local inputX = mouseCoordinate[1]
        local inputY = mouseCoordinate[2]

        local pos = self._transform.localPosition
        local scale = self._transform.localScale
        local minX = pos.x - (scale.x / 2)
        local maxX = pos.x + (scale.x / 2)
        local minY = pos.y - (scale.y / 2)
        local maxY = pos.y + (scale.y / 2)

        local isHovering = inputX >= minX and inputX <= maxX and inputY >= minY and inputY <= maxY

        if isHovering and not self._isHovered then
            self._isHovered = true
            if self._audio then
                self._audio:Play()
            end
        elseif not isHovering then
            self._isHovered = false
        end
    end,

    OnClickCloseButton = function(self)
        if self._audio then
            self._audio:Play()
        end

        -- Save settings when closing menu (only writes if dirty)
        GameSettings.SaveIfDirty()

        -- BUTTONS TO ENABLE
        local targetButtons = {
            "PlayGame",
            "Credits",
            "ExitGame",
            "Settings"
        }

        for index, value in ipairs(targetButtons) do
            local targetEntity = Engine.GetEntityByName(value)
            if targetEntity and targetEntity ~= -1 then
                local btnComp = GetComponent(targetEntity, "ButtonComponent")
                if btnComp then
                    btnComp.interactable = true
                else
                end
            else
            end
        end

        -- CLOSE SETTINGS UI
        local settingUIEntity = Engine.GetEntityByName("SettingsUI")
        if settingUIEntity and settingUIEntity ~= -1 then
            local activeComp = GetComponent(settingUIEntity, "ActiveComponent")
            if activeComp then
                activeComp.isActive = false
            else
            end
        else
        end
    end,
}
