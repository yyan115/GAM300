-- rigidbody_test.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local debug = require("extension.debug_helpers")

return Component {
    fields = {},  -- optional if your engine requires it

    Start = function(self)
        -- create an instance field _transform
        self._rb = nil

        -- try to fetch RigidBodyComponent component
        -- if self.getcomponent then
        --     self._rb = self:getcomponent("RigidBodyComponent")
        if self.GetComponent then
            self._rb = self:GetComponent("RigidBodyComponent")
        end

        if self._rb then
            print("[rigidbody_test] RigidBodyComponent component obtained successfully")
        else
            print("[rigidbody_test] RigidBodyComponent component not found!")
        end
        self._rb.isTrigger = true
    end,

    Update = function(self, dt)
        if not self._rb then
            print("[rigidbody_test] cannot get component")
        end
        print("rb.istrigger is " .. tostring(self._rb.isTrigger))
    end,
}
