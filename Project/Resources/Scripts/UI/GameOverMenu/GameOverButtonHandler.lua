require("extension.engine_bootstrap")

local event_bus = _G.event_bus
local Component = require("extension.mono_helper")

return Component {
    fields = {
        fadeDuration = 3.0,
        fadeScreenName = "GameOverFade",
        playerName = "Player"
    },

    OnClickRespawnButton = function(self)
         Time.SetPaused(false)
         Time.SetTimeScale(1.0)

        if Screen and Screen.IsCursorLocked() then
            Screen.SetCursorLocked(true)
        end
        
        if event_bus and event_bus.publish then
            event_bus.publish("respawnPlayer", true)
        end

        -- Restart main BGM and ambience when the player continues
        local bgm1 = GetComponent(Engine.GetEntityByName("BGM1"), "AudioComponent")
        if bgm1 then
            bgm1:UnPause()
        end
        local ambience = GetComponent(Engine.GetEntityByName("Ambience"), "AudioComponent")
        if ambience then
            ambience:UnPause()
        end
        local DeathScreenBGM = GetComponent(Engine.GetEntityByName("DeathScreenBGM"), "AudioComponent")
        if DeathScreenBGM then
            DeathScreenBGM:Stop()
        end
    end,

    Awake = function(self)
        
    end,

    Start = function(self)
        -- Cache audio component for hover SFX
        self._audio = self:GetComponent("AudioComponent")

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
                    maxY = pos.y + (scale.y / 2),
                    wasHovering = false
                }

                if hoverSprite then
                    hoverSprite.isVisible = false
                end
            else
                print("Warning: Missing entities for " .. names.base)
            end
        end
        self._DeathBGActive = GetComponent(Engine.GetEntityByName("DeathScreenBG"), "ActiveComponent")
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
            if data.hoverSprite and self._DeathBGActive and self._DeathBGActive.isActive then
                local isHovering = (inputX >= data.minX and inputX <= data.maxX and
                                   inputY >= data.minY and inputY <= data.maxY)

                -- Play hover SFX only when entering hover state
                if isHovering and not data.wasHovering then
                    if self._audio then
                        self._audio:Play()
                    end
                end
                data.wasHovering = isHovering
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