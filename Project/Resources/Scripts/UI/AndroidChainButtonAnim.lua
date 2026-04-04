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

        print(string.format("[AndroidChainButtonAnim] Start | sprite=%s hookFrames=%d pullFrames=%d slamFrames=%d",
            tostring(self._sprite ~= nil), #HOOK_FRAMES, #PULL_FRAMES, #SLAM_FRAMES))

        if self._sprite then
            self._sprite:SetTextureFromPath(HOOK_FRAMES[1])
        end

        if not _G.event_bus or not _G.event_bus.subscribe then
            print("[AndroidChainButtonAnim] WARNING: event_bus not available")
            return
        end

        self._subPull = _G.event_bus.subscribe("chain.pull_chain", function(p)
            if p then
                print("[AndroidChainButtonAnim] State -> PULL")
                self:_setState(PULL_FRAMES)
            end
        end)
        self._subSlam = _G.event_bus.subscribe("chain.slam_chain", function(p)
            if p then
                print("[AndroidChainButtonAnim] State -> SLAM")
                self:_setState(SLAM_FRAMES)
            end
        end)
        self._subRetract = _G.event_bus.subscribe("chain.retract_chain", function(p)
            if p then
                print("[AndroidChainButtonAnim] State -> HOOK (retract)")
                self:_setState(HOOK_FRAMES)
            end
        end)
        self._subDetach = _G.event_bus.subscribe("chain.detached", function()
            print("[AndroidChainButtonAnim] State -> HOOK (detach)")
            self:_setState(HOOK_FRAMES)
        end)
    end,

    _setState = function(self, frames)
        self._current = frames
        self._playing = false
        self._frame   = 1
        if self._sprite then
            self._sprite:SetTextureFromPath(frames[1])
        end
    end,

    Update = function(self, dt)
        if not self._sprite then return end

        if Input.IsActionJustPressed("ChainAttack") then
            print(string.format("[AndroidChainButtonAnim] Press! starting %d-frame anim", #self._current))
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
                print("[AndroidChainButtonAnim] Animation done, back to frame 1")
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
        local subs = { "_subPull", "_subSlam", "_subRetract", "_subDetach" }
        for _, k in ipairs(subs) do
            if self[k] then
                pcall(function() _G.event_bus.unsubscribe(self[k]) end)
                self[k] = nil
            end
        end
    end,
}
