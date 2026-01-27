require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local Input = _G.Input

-- Animation States
local HurtTrigger = "Hurt"

local function PlayerTakeDmg(self, dmg)
    -- TEST PLAYER TAKE DAMAGE FUNCTION CALL (Press K to take damage)
    if Input and Input.IsActionJustPressed and Input.IsActionJustPressed("Interact") then
        print("[PlayerTakeDmg] Animator set trigger Hurt")
        self._animator:SetTrigger(HurtTrigger)
        --self._health = self._health - dmg
        --print(string.format("[PlayerTakeDmg] Player took %d damage. Remaining health: %d", dmg, self._health))
    end
end

return Component {
    mixins = { TransformMixin },

    fields = {
        Health = 10
    },

    Awake = function(self)
        print("[PlayerHealth] Health initialized to ", self.Health)
    end,

    Start = function(self)
        self._animator  = self:GetComponent("AnimationComponent")
        self._health = self.Health
    end,

    Update = function(self, dt)
        if not self._animator then 
            return
        end

        -- TEST PLAYER TAKE DAMAGE FUNCTION CALL (Press K to take damage)
        PlayerTakeDmg(self, 1)
    end,

    OnDisable = function(self)

    end,
}