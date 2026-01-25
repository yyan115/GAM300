require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

-- Attach this script to the SettingsUI entity
-- Sliders now self-initialize in their Start() functions
-- This script just ensures GameSettings is initialized
return Component {
    Start = function(self)
        -- Initialize GameSettings (safe to call multiple times)
        GameSettings.Init()
        print("[SettingsMenuInit] GameSettings initialized")
    end,
}
