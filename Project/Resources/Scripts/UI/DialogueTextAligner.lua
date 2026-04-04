require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

-- DialogueTextAligner.lua
-- Attach to the DialogueText entity in the cutscene.
-- When board 7 (the long 3-line dialogue) becomes active, shifts the
-- text up immediately so it centres correctly inside the dialogue box.

return Component {
    fields = {
        -- World-unit amount to shift the text upward for the long dialogue.
        -- Tune this until the 3-line block looks centred.
        yShift = 0.08,
    },

    Start = function(self)
        self._tr = self:GetComponent("Transform")

        self._defaultY = 0.03
        if self._tr then
            local pos = self._tr.localPosition
            if pos then self._defaultY = pos.y end
        end

        local cutsceneEnt = Engine.GetEntityByName("Cutscene")
        if cutsceneEnt then
            self._videoComp = GetComponent(cutsceneEnt, "VideoComponent")
        end

        self._shifted        = false
        self._lastBoardIndex = -1
    end,

    Update = function(self, dt)
        if not self._tr or not self._videoComp then return end

        local boardIndex = self._videoComp.currentBoardIndex
        if boardIndex == self._lastBoardIndex then return end
        self._lastBoardIndex = boardIndex

        -- Board 7 (0-indexed) is the long 3-line dialogue in 02_IntroCutscene.
        local isLongBoard = (boardIndex == 7)

        if isLongBoard and not self._shifted then
            self._shifted = true
            local pos = self._tr.localPosition
            if pos then
                pos.y = self._defaultY + self.yShift
                self._tr.isDirty = true
            end
        elseif not isLongBoard and self._shifted then
            self._shifted = false
            local pos = self._tr.localPosition
            if pos then
                pos.y = self._defaultY
                self._tr.isDirty = true
            end
        end
    end,
}
