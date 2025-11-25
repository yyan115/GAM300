-- rigidbody_test.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local debug = require("extension.debug_helpers")

return Component {
    fields = {},  -- optional if your engine requires it

    Start = function(self)
        -- create an instance field _transform
        self._cc = self:GetComponent("CharacterControllerComponent")
        self._cc.speed = 10
    end,

    Update = function(self, dt)

        if self._cc then
            print("CharacterComponent is obtained")
        else
            print("shit, this aint working...")
        end

    end,
}