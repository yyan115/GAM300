require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        dialogueName = "Intro",
        NarrativeSFX = {},
        entryDisplayTime = 3.0,  -- seconds before visuals are disabled
    },

    Start = function(self)
        self._audio = self:GetComponent("AudioComponent")
        self._lastIndex = -1
        self._entryTimer = 0
        self._isHidden = false
        self._currentDisplayTime = self.entryDisplayTime

        local monologueEnt = Engine.GetEntityByName("Monologue")
        if monologueEnt then
            self._monologueActive = GetComponent(monologueEnt, "ActiveComponent")
        end

        local swEntity = Engine.GetEntityByName("MonologueSoundwave")
        if swEntity then
            self._soundwaveAnim = GetComponent(swEntity, "SpriteAnimationComponent")
        end
    end,

    _setVisualsActive = function(self, active)
        if self._monologueActive then
            self._monologueActive.isActive = active
        end
        if self._soundwaveAnim then
            self._soundwaveAnim.playing = active
            if active then
                self._soundwaveAnim.currentFrameIndex = 0
            end
        end
    end,

    Update = function(self, dt)
        local currentIndex = DialogueManager.GetCurrentIndex(self.dialogueName)
        -- Handle dialogue entry change
        if currentIndex ~= self._lastIndex then
            self._lastIndex = currentIndex
            self._entryTimer = 0
            self._isHidden = false

            if currentIndex >= 0 then
                -- Resolve display duration from dialogue entry (Time mode) or fallback
                local autoTime = DialogueManager.GetCurrentEntryAutoTime(self.dialogueName)  
                self._currentDisplayTime = (autoTime > 0) and autoTime or self.entryDisplayTime
                print("Current Dialogue Index:", currentIndex, "AutoTime:", autoTime, "Set display time to:", self._currentDisplayTime)

                -- Play VO for this entry
                local guidIndex = currentIndex + 1  -- Lua is 1-indexed
                local guid = self.NarrativeSFX[guidIndex]
                if self._audio and guid and guid ~= "" then
                    self._audio:Stop()
                    self._audio:PlayOneShot(guid)
                end

                -- Show all visuals and start soundwave
                self:_setVisualsActive(true)
            end
        end

        -- Disable visuals after display time
        if currentIndex >= 0 and not self._isHidden then
            self._entryTimer = self._entryTimer + dt
            if self._entryTimer >= self._currentDisplayTime then
                self._isHidden = true
                self:_setVisualsActive(false)
            end
        end

        -- Disable visuals when dialogue fully finishes
        if currentIndex < 0 then
            self:_setVisualsActive(false)
        end
    end,
}
