-- LoadingHandler.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        barFillName = "LoadingBarFill",
        loadingTextName = "PercentText",
        targetScene = "Resources/Scenes/04_Level.scene",
        anchorLeft = true,
        maxWidth = 1580
    },

    _progress = 0,
    _started = false,
    _barEntity = nil,
    _barTransform = nil,
    _textEntity = nil,
    _text = nil,
    _barBasePosX = nil,

    Start = function(self)
        self._progress = 0
        self._started = false

        -- Cache bar
        self._barEntity = Engine.GetEntityByName(self.barFillName)

        if self._barEntity then
            self._barTransform = GetComponent(self._barEntity, "Transform")

            if self._barTransform then
                self._barBasePosX = self._barTransform.localPosition.x

                -- Start empty
                self._barTransform.localScale.x = 0

                if self.anchorLeft then
                    self._barTransform.localPosition.x =
                        self._barBasePosX - (self.maxWidth * 0.5)
                end

                self._barTransform.isDirty = true
            end
        end

        -- Cache text
        self._textEntity = Engine.GetEntityByName(self.loadingTextName)
        if self._textEntity then
            self._text = GetComponent(self._textEntity, "TextRenderComponent")
        end

        self:_setPercentText(0)
    end,

    _setPercentText = function(self, percentInt)
        if self._text then
            self._text.text = tostring(percentInt) .. "%"
        end
    end,

    _applyBarProgress = function(self, p)
        if not self._barTransform then return end

        if p < 0 then p = 0 elseif p > 1 then p = 1 end

        local newWidth = self.maxWidth * p

        -- apply scale
        self._barTransform.localScale.x = newWidth

        -- offset so it fills left -> right
        if self.anchorLeft then
            local offset = (self.maxWidth - newWidth) * 0.5
            self._barTransform.localPosition.x = self._barBasePosX - offset
        end

        self._barTransform.isDirty = true
    end,

    Update = function(self, dt)
        if not self._started then
            print("[Loading] Starting LoadAsync")
            Scene.LoadAsync(self.targetScene)
            self._started = true
            return
        end

        self._progress = Scene.GetLoadProgress()

        self:_applyBarProgress(self._progress)

        local percentInt = math.floor(self._progress * 100 + 0.5)
        self:_setPercentText(percentInt)
    end
}