require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    Update = function(self, dt)
        local dtSec = dt or 0
        if dtSec > 1.0 then dtSec = dtSec * 0.001 end
        if dtSec <= 0 then return end
        if dtSec > 0.05 then dtSec = 0.05 end

        -- Update all character controllers ONCE per frame
        if CharacterController and CharacterController.UpdateAll then
            CharacterController.UpdateAll(dtSec)
        end
    end
}
