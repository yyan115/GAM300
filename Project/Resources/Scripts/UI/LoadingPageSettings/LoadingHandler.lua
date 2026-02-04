require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        totalTime = 10.0,              -- seconds to reach 100%
        barFillName = "BarFill",
        loadingTextName = "ValueText",
        targetScene = "Resources/Scenes/04_GameLevel.scene",

        -- If your bar fill is centered and you want it to grow from the left,
        -- set this true so we also shift position while scaling.
        anchorLeft = true,

        -- Optional: if your bar is 1.0 scale at 100%, leave as 1.0.
        -- If your art expects something else, change it.
        fullScaleX = 2.0,
    },

    _t = 0,
    _progress = 0,

    _barEntity = nil,
    _barTransform = nil,
    _barSprite = nil,

    _textEntity = nil,
    _text = nil,

    -- For anchoring math (only used if anchorLeft == true and transform exists)
    _barBasePosX = nil,
    _barBaseScaleX = nil,

    Start = function(self)
        self._t = 0
        self._progress = 0

                -- Cache bar
        -- Cache bar
        self._barEntity = Engine.GetEntityByName(self.barFillName)
        if self._barEntity then
            self._barTransform = GetComponent(self._barEntity, "Transform")

            if self._barTransform then
                -- Store authored "full" width + position
                self._barBasePosX = self._barTransform.localPosition.x
                self._barBaseScaleX = self._barTransform.localScale.x

                -- Initialize to 0% (width 0)
                self._barTransform.localScale.x = 0.0

                -- If left anchored, shift so left edge stays put at 0%
                if self.anchorLeft then
                    self._barTransform.localPosition.x = self._barBasePosX - (self._barBaseScaleX * 0.5)
                end

                self._barTransform.isDirty = true
            end
        end


        -- Cache text
        self._textEntity = Engine.GetEntityByName(self.loadingTextName)
        print(Engine.GetSceneName())
        if self._textEntity then
            -- Change component name if your engine uses something else (e.g. "TextComponent")
            self._text = GetComponent(self._textEntity, "TextRenderComponent")
            if not self._text then
                print("[LoadingPage] Warning: TextRenderComponent missing on " .. tostring(self.loadingTextName))
            end
        else
            print("[LoadingPage] Warning: LoadingValueText entity not found: " .. tostring(self.loadingTextName))
        end

        -- Set initial text
        self:_setPercentText(0)


    end,

    _setPercentText = function(self, percentInt)
        if self._text then
            -- If your text field is named differently, adjust here.
            self._text.text = tostring(percentInt) .. "%"
        end
    end,

    _applyBarProgress = function(self, p)
    if not self._barTransform or not self._barTransform.localScale then return end

    -- clamp
    if p < 0 then p = 0 elseif p > 1 then p = 1 end

local fullX = (self._barBaseScaleX or 1.0) * (self.fullScaleX or 1.0)
    local newScaleX = fullX * p

    -- Scale width
    self._barTransform.localScale.x = newScaleX

    -- Keep left edge fixed while scaling (bar scales about center)
    if self.anchorLeft and self._barBasePosX then
        local missing = fullX - newScaleX
        self._barTransform.localPosition.x = self._barBasePosX - (missing * 0.5)
    end

    -- IMPORTANT in your engine (matches your slider code)
    self._barTransform.isDirty = true
    end,


    Update = function(self, dt)
        local total = self.totalTime or 10.0
        if total <= 0 then total = 10.0 end

        self._t = (self._t or 0) + dt
        self._progress = math.min(self._t / total, 1.0)

        -- Update bar + text
        self:_applyBarProgress(self._progress)
        local percentInt = math.floor(self._progress * 100 + 0.5)
        self:_setPercentText(percentInt)

        -- Optional: when done, you can auto-advance to next scene.
        -- Uncomment and set nextScene in fields if desired.
        -- if self._progress >= 1.0 and self.nextScene and Scene and Scene.Load then
        -- Scene.Load(self.nextScene)
        
        -- end
    end
}
