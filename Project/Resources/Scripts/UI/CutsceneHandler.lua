require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        targetScene = "Resources/Scenes/04_GameLevel.scene"
    },
    Start = function(self)
        self._videoComp = self:GetComponent("VideoComponent")
        self._pendingScene = self.targetScene
        self._videoComp.cutsceneEnded = false
    end,

    Update = function(self, dt)
        if not self._videoComp then 
            return
        end

        if self._videoComp.cutsceneEnded == false then
            return
        end
        Scene.Load(self._pendingScene)
    end
}
