require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local BASE = "Resources/Sprites/PlayerHUD/Android/"

local FRAMES = {
    Dash = (function()
        local t = {}
        for i = 1, 7 do t[i] = BASE .. "DashSprite/DashSprite" .. i .. ".png" end
        return t
    end)(),
    Jump = (function()
        local t = {}
        for i = 1, 7 do t[i] = BASE .. "JumpSprite/JumpSprite" .. i .. ".png" end
        return t
    end)(),
    Attack = (function()
        local t = {}
        for i = 1, 9 do t[i] = BASE .. "SlashSprite/SlashSprite" .. i .. ".png" end
        return t
    end)(),
    FeatherSkill = (function()
        local t = {}
        for i = 1, 9 do t[i] = BASE .. "FeatherBurst/FeatherBurstSprite" .. i .. ".png" end
        return t
    end)(),
}

return Component {
    fields = {
        -- "Dash", "Jump", "Attack", or "FeatherSkill"
        actionName = "Dash",
        fps        = 24,
    },

    Start = function(self)
        self._sprite  = self:GetComponent("SpriteRenderComponent")
        -- Strip surrounding quotes the editor may have added e.g. "Attack" -> Attack
        local action  = tostring(self.actionName):gsub('^"(.*)"$', '%1')
        self.actionName = action
        self._frames  = FRAMES[action] or {}
        self._playing = false
        self._frame   = 1
        self._timer   = 0

        print(string.format("[AndroidButtonAnim] Start | actionName=%s frames=%d sprite=%s",
            action, #self._frames, tostring(self._sprite ~= nil)))

        if #self._frames == 0 then
            print("[AndroidButtonAnim] WARNING: no frames found - is actionName set correctly?")
        end

        if self._sprite and #self._frames > 0 then
            self._sprite:SetTextureFromPath(self._frames[1])
        end
    end,

    Update = function(self, dt)
        if not self._sprite or #self._frames == 0 then return end

        if Input.IsActionJustPressed(self.actionName) then
            print(string.format("[AndroidButtonAnim] Press! action=%s starting %d-frame anim",
                self.actionName, #self._frames))
            self._playing = true
            self._frame   = 1
            self._timer   = 0
            self._sprite:SetTextureFromPath(self._frames[1])
        end

        if not self._playing then return end

        self._timer = self._timer + dt
        local frameDuration = 1.0 / self.fps

        while self._timer >= frameDuration do
            self._timer = self._timer - frameDuration
            self._frame = self._frame + 1

            if self._frame > #self._frames then
                print("[AndroidButtonAnim] Animation done, back to frame 1")
                self._playing = false
                self._frame   = 1
                self._sprite:SetTextureFromPath(self._frames[1])
                return
            end

            self._sprite:SetTextureFromPath(self._frames[self._frame])
        end
    end,
}
