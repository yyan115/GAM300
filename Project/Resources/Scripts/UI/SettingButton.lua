require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    _pendingScene = nil,

    OnClickSettingButton = function(self)
        print("CruelWorld")
    end,

    Update = function(self, dt)
    end
}
