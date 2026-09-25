require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local BASE = "Resources/Sprites/PlayerHUD/Android/"

local HOOK_FRAMES = (function()
    local t = {}
    for i = 1, 9 do t[i] = BASE .. "ChainHookSprite/ChainHookSprite" .. i .. ".png" end
    return t
end)()

local PULL_FRAMES = (function()
    local t = {}
    for i = 1, 9 do t[i] = BASE .. "ChainPull/ChainPull" .. i .. ".png" end
    return t
end)()

local SLAM_FRAMES = (function()
    local t = {}
    for i = 1, 9 do t[i] = BASE .. "ChainSlam1/ChainSlamSpritev1_" .. i .. ".png" end
    return t
end)()

return Component {
    fields = {
        fps = 24,
    },

    Start = function(self)
        self._sprite   = self:GetComponent("SpriteRenderComponent")
        self._current  = HOOK_FRAMES
        self._playing  = false
        self._frame    = 1
        self._timer    = 0
        self._hookedTarget = nil

        --print(string.format("[AndroidChainButtonAnim] Start | sprite=%s hookFrames=%d pullFrames=%d slamFrames=%d",
        --    tostring(self._sprite ~= nil), #HOOK_FRAMES, #PULL_FRAMES, #SLAM_FRAMES))

        if self._sprite then
            self._sprite:SetTextureFromPath(HOOK_FRAMES[1])
        end

        if not _G.event_bus or not _G.event_bus.subscribe then
            --print("[AndroidChainButtonAnim] WARNING: event_bus not available")
            return
        end

        -- Attachment selects the next available action. Pull/slam notifications
        -- mean that action has been consumed, so both must return to throw.
        local function reset()
            self._hookedTarget = nil
            self:_setState(HOOK_FRAMES)
        end
        self._subPull = _G.event_bus.subscribe("chain.pull_chain", function(p)
            if p then reset() end
        end)
        self._subSlam = _G.event_bus.subscribe("chain.slam_chain", function(p)
            if p then reset() end
        end)
        -- Boss hooks need not publish the normal enemy's pull/slam animation
        -- event. Reset on the shared execution event as well; listener order
        -- must not leave the consumed action displayed.
        self._subConsumed = _G.event_bus.subscribe("chain.enemy_hooked", reset)
        self._subRetract = _G.event_bus.subscribe("chain.retract_chain", function(p)
            if p then reset() end
        end)
        self._subRetracted = _G.event_bus.subscribe("chain.endpoint_retracted", reset)
        self._subDetach = _G.event_bus.subscribe("chain.detached", reset)
        self._subHookedType = _G.event_bus.subscribe("chain.hooked_target_type", function(p)
            if not p then return end
            self._hookedTarget = p.entityId
            self:_setState(p.isFlying and SLAM_FRAMES or PULL_FRAMES)
        end)
        self._subTargetDied = _G.event_bus.subscribe("enemy_died", function(p)
            if p and self._hookedTarget ~= nil and p.entityId == self._hookedTarget then
                reset()
            end
        end)
        self._subPlayerDead = _G.event_bus.subscribe("playerDead", function(dead)
            if dead then reset() end
        end)
        self._subRespawn = _G.event_bus.subscribe("respawnPlayer", function(respawn)
            if respawn then reset() end
        end)
    end,

    _setState = function(self, frames)
        self._current = frames
        self._playing = false
        self._frame   = 1
        self._timer   = 0
        self._sprite = self:GetComponent("SpriteRenderComponent")
        if self._sprite then
            self._sprite:SetTextureFromPath(frames[1])
        end
    end,

    Update = function(self, dt)
        if not self._sprite then return end

        if Input.IsActionPressed("ChainAttack") then
            --print(string.format("[AndroidChainButtonAnim] Press! starting %d-frame anim", #self._current))
            self._playing = true
            self._frame   = 1
            self._timer   = 0
            self._sprite:SetTextureFromPath(self._current[1])
        end

        if not self._playing then return end

        self._timer = self._timer + dt
        local frameDuration = 1.0 / self.fps

        while self._timer >= frameDuration do
            self._timer = self._timer - frameDuration
            self._frame = self._frame + 1

            if self._frame > #self._current then
                --print("[AndroidChainButtonAnim] Animation done, back to frame 1")
                self._playing = false
                self._frame   = 1
                self._sprite:SetTextureFromPath(self._current[1])
                return
            end

            self._sprite:SetTextureFromPath(self._current[self._frame])
        end
    end,

    OnDisable = function(self)
        if not _G.event_bus or not _G.event_bus.unsubscribe then return end
        local subs = {
            "_subPull", "_subSlam", "_subConsumed", "_subRetract", "_subRetracted",
            "_subDetach", "_subHookedType", "_subTargetDied", "_subPlayerDead", "_subRespawn",
        }
        for _, k in ipairs(subs) do
            if self[k] then
                pcall(function() _G.event_bus.unsubscribe(self[k]) end)
                self[k] = nil
            end
        end
    end,
}
