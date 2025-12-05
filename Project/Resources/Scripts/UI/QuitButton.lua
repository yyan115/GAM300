require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    _pendingScene = nil,

    OnClickQuitButton = function(self)
        Screen.RequestClose()
    end
}
