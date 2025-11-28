require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },
    
    fields = {
        health = 5,
        damage = 1,
        atk_CD = 4, --attack cooldown
    },
    
    Start = function(self)
        self.animation = self:GetComponent("AnimationComponent")
        self.collider  = self:GetComponent("ColliderComponent")
    end,
    
    Update = function(self, dt)

        --REPLACE GETKEY FUNCTIONS BY TAKE DAMAGE FUNCTIONS E.T.C, PLACEHOLDER
    end
}


        -- local clipIndex = 1
        -- local toLoop = true
        -- if Input.GetKeyDown(Input.Key.V) then
        --     Animation.PlayClip(self.animation, clipIndex, toLoop)
        -- end
        -- if Input.GetKeyDown(Input.Key.U) then
        --     Animation.PlayOnce(self.animation, clipIndex)
        -- end
        -- if Input.GetKeyDown(Input.Key.I) then
        --     Animation.Pause(self.animation)
        -- end
        -- if Input.GetKeyDown(Input.Key.O) then
        --     Animation.Stop(self.animation)
        -- end
        -- if Input.GetKeyDown(Input.Key.P) then
        --     Animation.SetSpeed(self.animation, speed)
        -- end
        -- if Input.GetKeyDown(Input.Key.K) then
        --     Animation.SetLooping(self.animation, toLoop)
        -- end

        -- bool isPlaying =  Animation.IsPlaying(self.animation)




            -- Animation.PlayOnce(self.animation, clipIndex)
            -- Animation.Pause(self.animation)
            -- Animation.Stop(self.animation)
            -- float speed = 2.0f
            -- Animation.SetSpeed(self.animation, speed)
            -- Animation.SetLooping(self.animation, toLoop)
            -- Animation.IsPlaying(self.animation)
