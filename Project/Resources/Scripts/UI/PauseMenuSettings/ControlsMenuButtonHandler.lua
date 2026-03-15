require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

return Component {
    fields = {
        -- Audio: [1] = hover SFX, [2] = click SFX
        buttonSFX = {},
        -- Sprite GUIDs: [1] = normal, [2] = hover
        BackSpriteGUIDs = {},
    },

    Start = function(self)
        -- Initialize GameSettings (safe to call multiple times)
        if GameSettings then
            GameSettings.Init()
        end

        -- Cache audio component from Buttons entity
        self._audio = self:GetComponent("AudioComponent")

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
                print("[ControlssMenuButtonHandler] Warning: Missing entity " .. config.base)
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
                -- Play hover sound
                if self._audio and self.buttonSFX and self.buttonSFX[1] then
                    self._audio:PlayOneShot(self.buttonSFX[1])
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

    OnClickBackButton = function(self)
        local audiocomp = GetComponent(Engine.GetEntityByName("ControlsBackButton"), "AudioComponent")
        if audiocomp then
            audiocomp:Play()
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
