-- LoadingHandler.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
return Component {
    fields = {
        barFillName = "LoadingBarFill",
        loadingTextName = "PercentText",
        targetScene = "Resources/Scenes/04_Level.scene",
        anchorLeft = true,
        fullScaleX = 2.0,
    },
    _progress = 0,
    _started = false,
    _barEntity = nil,
    _barTransform = nil,
    _textEntity = nil,
    _text = nil,
    _barBasePosX = nil,
    _barBaseScaleX = nil,
    Start = function(self)
        self._progress = 0
        self._started = false
        -- Cache bar
        self._barEntity = Engine.GetEntityByName(self.barFillName)
        self._barContainer = Engine.GetEntityByName("LoadingBarContainer")
        self._barContainerTransform = GetComponent(self._barContainer, "Transform")
        if self._barEntity then
            self._barTransform = GetComponent(self._barEntity, "Transform")
            if self._barTransform then
                self._barBasePosX = self._barTransform.localPosition.x
                self._barBaseScaleX = self._barTransform.localScale.x
                -- Initialize to 0%
                self._barTransform.localScale.x = 0.0
                if self.anchorLeft then
                    self._barTransform.localPosition.x = self._barBasePosX - (self._barBaseScaleX * 0.5)
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
        if not self._barTransform or not self._barContainerTransform then return end
        if p < 0 then p = 0 elseif p > 1 then p = 1 end
        local fullX = (self._barBaseScaleX or 1.0) * (self.fullScaleX or 1.0)
        local newScaleX = fullX * p
        self._barTransform.localScale.x =  self._barContainerTransform.localScale.x * p
        if self.anchorLeft and self._barBasePosX then
            local missing = fullX - newScaleX
            self._barTransform.localPosition.x = self._barBasePosX - (missing * 0.5)
        end
        self._barTransform.isDirty = true
    end,
    Update = function(self, dt)
        -- Kick off async load on the first Update frame
        if not self._started then
            print("[Loading] Starting LoadAsync")
            Scene.LoadAsync(self.targetScene)
            self._started = true
            return
        end
        -- Read real progress from the engine
        local progress = Scene.GetLoadProgress()
        local isLoading = Scene.IsLoading()
        self._progress = Scene.GetLoadProgress()
        -- Update bar + text
        self:_applyBarProgress(self._progress)
        local percentInt = math.floor(self._progress * 100 + 0.5)
        self:_setPercentText(percentInt)
    end
}