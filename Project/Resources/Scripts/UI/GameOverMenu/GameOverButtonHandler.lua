require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        fadeDuration = 3.0,
        fadeScreenName = "GameOverFade",
    },

    OnClickRespawnButton = function(self)
        if Screen and Screen.IsCursorLocked() then
            Screen.SetCursorLocked(true)
        end
        self._startRespawning = true
        Scene.Load(Scene.GetCurrentPath())
    end,

    Start = function(self)
        self._buttonData = {} 
        local buttonMapping = {
            { base = "RespawnButton", hover = "RespawnButtonHovered" }
        }

        for index, names in ipairs(buttonMapping) do
            local baseEnt = Engine.GetEntityByName(names.base)
            local hoverEnt = Engine.GetEntityByName(names.hover)

            if baseEnt and hoverEnt then
                local transform = GetComponent(baseEnt, "Transform")
                local pos = transform.localPosition
                local scale = transform.localScale
                local hoverSprite = GetComponent(hoverEnt, "SpriteRenderComponent")

                self._buttonData[index] = {
                    hoverSprite = hoverSprite,
                    minX = pos.x - (scale.x / 2),
                    maxX = pos.x + (scale.x / 2),
                    minY = pos.y - (scale.y / 2),
                    maxY = pos.y + (scale.y / 2)
                }

                if hoverSprite then
                    hoverSprite.isVisible = false
                end
            else
                print("Warning: Missing entities for " .. names.base)
            end
        end

        self._fadeDuration = 0
        local fadeEntity = Engine.GetEntityByName(self.fadeScreenName)
        if fadeEntity then
            self._fadeActive = GetComponent(fadeEntity, "ActiveComponent")
            self._fadeSprite = GetComponent(fadeEntity, "SpriteRenderComponent")
            if self._fadeActive then
                self._fadeActive.isActive = false
            end
            if self._fadeSprite then
                -- Initialize alpha to 0
                self._fadeSprite.alpha = 0
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
            if data.hoverSprite then
                local isHovering = (inputX >= data.minX and inputX <= data.maxX and
                                   inputY >= data.minY and inputY <= data.maxY)
                
                data.hoverSprite.isVisible = isHovering
            end
        end

        if self._startRespawning then
            self._fadeActive.isActive = true
            self._fadeDuration = self._fadeDuration + dt
            local duration = self.fadeDuration or 1.0
            self._fadeAlpha = math.min(self._fadeDuration / duration, 1.0)
            self._fadeSprite.alpha = self._fadeAlpha

            -- Stop once fade is complete
            if self._fadeAlpha >= 1.0 then
                self._startRespawning = false
                Scene.Load(Scene.GetCurrentScenePath())
            end
        end
    end,
}