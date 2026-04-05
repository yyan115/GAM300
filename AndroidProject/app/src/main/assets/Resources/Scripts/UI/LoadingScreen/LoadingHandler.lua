require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        barFillName     = "LoadingBarFill",
        loadingTextName = "PercentText",
        targetScene     = "Resources/Scenes/04_Level.scene",
        fullScaleX      = 1280.0   -- absolute target X scale at 100%
    },

    _progress = 0,
    _started = false,
    _barEntity = nil,
    _barTransform = nil,
    _textEntity = nil,
    _text = nil,
    _barOriginX = nil,

    Start = function(self)
        self._progress = 0
        self._started  = false

        -- Cache bar
        self._barEntity    = Engine.GetEntityByName(self.barFillName)
        self._barContainer = Engine.GetEntityByName("LoadingBarContainer")
        self._barContainerTransform = GetComponent(self._barContainer, "Transform")

        if self._barEntity then
            self._barTransform = GetComponent(self._barEntity, "Transform")

            if self._barTransform then
                -- Save the left-edge origin, ignore whatever the authored scale was
                local containerWidth = self.fullScaleX or 1280.0
                self._barOriginX = self._barTransform.localPosition.x - (containerWidth / 2)

                -- Initialize to 0%
                self._barTransform.localScale.x    = 0.0
                self._barTransform.localPosition.x = self._barOriginX
                self._barTransform.isDirty         = true
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

        -- fullScaleX is the absolute X scale at 100% — no multiplier on top
        local newScaleX = (self.fullScaleX or 1280.0) * p
        local newPosX   = self._barOriginX + (newScaleX / 2)

        self._barTransform.localScale.x    = newScaleX
        self._barTransform.localPosition.x = newPosX
        self._barTransform.isDirty         = true
    end,

    Update = function(self, dt)
        -- Kick off async load on the first Update frame
        if not self._started then
            --print("[Loading] Starting LoadAsync")
            Scene.LoadAsync(self.targetScene)
            self._started = true
            return
        end

        -- Read real progress from the engine
        local progress = Scene.GetLoadProgress()

        self._progress = progress

        -- Update bar + text
        self:_applyBarProgress(self._progress)

        local percentInt = math.floor(self._progress * 100 + 0.5)
        self:_setPercentText(percentInt)
    end
}