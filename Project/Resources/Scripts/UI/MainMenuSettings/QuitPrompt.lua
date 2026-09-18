require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

-- The quit prompt's root. The engine puts the prompt up when a quit is asked
-- for and runs nothing else in the scene until it is answered, so anything
-- that has to react to it, such as a hover highlight that would otherwise
-- stay lit underneath, hears about it here.
return Component {
    Start = function(self)
        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("quit_prompt.shown", {})
        end
    end,
}
