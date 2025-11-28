require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
    },

    OnClick = function(self)
        print("Button is now at least forced to call")
    end,
}
