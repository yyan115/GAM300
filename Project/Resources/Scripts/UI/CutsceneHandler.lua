require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local QuitPrompt = require("UI.QuitPromptOverlay")

return Component {
    fields = {
        targetScene = "Resources/Scenes/03_Loading.scene"
    },
    Start = function(self)
        self._videoComp = self:GetComponent("VideoComponent")
        self._pendingScene = self.targetScene
        self._videoComp.cutsceneEnded = false
        QuitPrompt.Forget()
        self._skipWasInteractable = nil
    end,

    -- The Skip button would otherwise take the same click that answers the
    -- quit prompt, and skipping the cutscene under the prompt is not what the
    -- player asked for.
    _setSkipInteractable = function(self, interactable)
        local skip = Engine.GetEntityByName("SkipUI")
        if not skip or skip == -1 then return end
        local button = GetComponent(skip, "ButtonComponent")
        if button then button.interactable = interactable end
    end,

    Update = function(self, dt)
        -- Alt+F4 here used to close the game outright, because a cutscene has
        -- no UI of its own to ask with.
        QuitPrompt.Update(dt)
        if QuitPrompt.IsShown() then
            if self._skipWasInteractable == nil then
                self._skipWasInteractable = true
                self:_setSkipInteractable(false)
            end
            return
        elseif self._skipWasInteractable then
            self._skipWasInteractable = nil
            self:_setSkipInteractable(true)
        end

        if not self._videoComp then 
            return
        end

        if self._videoComp.cutsceneEnded == false then
            return
        end
        Scene.Load(self._pendingScene)
    end
}
