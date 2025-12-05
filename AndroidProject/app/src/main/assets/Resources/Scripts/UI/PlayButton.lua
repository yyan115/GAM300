require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    _pendingScene = nil,

    OnClickPlayButton = function(self)
        self._pendingScene = "Resources/Scenes/M3_Gameplay.scene"
    end,

    Update = function(self, dt)
        if self._pendingScene then
            Scene.Load(self._pendingScene)
            self._pendingScene = nil
        end
    end
}
