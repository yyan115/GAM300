local Component = require("mono_helper")
local UI = require("ui_helpers")
return Component {
    fields = { name = "HealthBar", maxHealth = 100, current = 100 },
    Update = function(self, dt)
        local f = (self.current or 0) / (self.maxHealth or 1)
        UI.show_progress("player_health", f)
    end
}
