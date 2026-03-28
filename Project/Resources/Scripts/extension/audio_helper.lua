-- audio_helper.lua
-- Shared audio utility helpers for random SFX playback with pitch/volume variation.
-- Consolidates common patterns from CombatAudio.lua, ChainAudio.lua, and similar.

local M = {}

-- ---------------------------------------------------------------------------
-- M.PlayRandomSFX(audio, clips, volume)
-- ---------------------------------------------------------------------------
-- Play a random clip from clips array, avoiding repeating the last index.
-- Stores _lastIdx on the clips array itself as a lightweight state slot.
-- Optional volume defaults to 1.0 if not specified.
-- Returns nil if audio is nil or clips is empty/nil.
--
-- Usage:
--   AudioHelper.PlayRandomSFX(self._audio, self.attackSFXClips)
--   AudioHelper.PlayRandomSFX(self._audio, self.attackSFXClips, 0.8)
--
function M.PlayRandomSFX(audio, clips, volume)
    if not audio or not clips or #clips == 0 then return end

    volume = tonumber(volume) or 1.0
    local n    = #clips
    local last = clips._lastIdx
    local idx
    if n == 1 then
        idx = 1
    elseif not last then
        idx = math.random(1, n)
    else
        idx = math.random(1, n - 1)
        if idx >= last then idx = idx + 1 end
    end
    clips._lastIdx = idx

    local clip = clips[idx]
    if not clip or clip == "" then return end

    pcall(function()
        audio:SetVolume(math.max(0.0, math.min(1.0, volume)))
        audio:PlayOneShot(clip)
    end)
end

-- ---------------------------------------------------------------------------
-- M.Play   RandomSFXPitched(audio, clips, pitchVar, volume)
-- ---------------------------------------------------------------------------
-- Play a random clip with pitch variation applied.
-- pitchVar defaults to 0.1 if not provided.
-- Pitch is calculated as: 1.0 + (rand*2 - 1) * pitchVar, clamped to [0.5, 2.0]
-- Optional volume defaults to 1.0 if not specified.
--
-- Usage:
--   AudioHelper.PlayRandomSFXPitched(self._audio, self.attackSFXClips, 0.15)
--   AudioHelper.PlayRandomSFXPitched(self._audio, self.attackSFXClips, 0.15, 0.8)
--
function M.PlayRandomSFXPitched(audio, clips, pitchVar, volume)
    if not audio or not clips or #clips == 0 then return end

    pitchVar = tonumber(pitchVar) or 0.1
    volume = tonumber(volume) or 1.0
    local n    = #clips
    local last = clips._lastIdx
    local idx
    if n == 1 then
        idx = 1
    elseif not last then
        idx = math.random(1, n)
    else
        idx = math.random(1, n - 1)
        if idx >= last then idx = idx + 1 end
    end
    clips._lastIdx = idx

    local clip = clips[idx]
    if not clip or clip == "" then return end

    pcall(function()
        local pitch = 1.0 + (math.random() * 2 - 1) * pitchVar
        pitch = math.max(0.5, math.min(2.0, pitch))
        audio:SetPitch(pitch)
        audio:SetVolume(math.max(0.0, math.min(1.0, volume)))
        audio:PlayOneShot(clip)
    end)
end

return M
