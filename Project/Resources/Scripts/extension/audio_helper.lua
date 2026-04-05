-- audio_helper.lua
-- Shared audio utility helpers for random SFX playback with pitch/volume variation.
-- Consolidates common patterns from CombatAudio.lua, ChainAudio.lua, and similar.

local M = {}

local function _clamp(v, lo, hi)
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end

local function _isDebugEnabled(clips)
    if clips and clips._debug ~= nil then
        return clips._debug == true
    end
    local g = _G
    if not g then return false end
    return rawget(g, "AUDIO_HELPER_DEBUG") == true
end

local function _pickClipNonRepeating(clips)
    local valid = {}
    for i = 1, #clips do
        local c = clips[i]
        if c and c ~= "" then valid[#valid + 1] = i end
    end
    if #valid == 0 then return nil end

    local candidates = valid
    if #valid > 1 then
        local last = clips._lastIdx
        candidates = {}
        for _, i in ipairs(valid) do
            if i ~= last then candidates[#candidates + 1] = i end
        end
    end

    local idx = candidates[math.random(1, #candidates)]
    clips._lastIdx = idx
    return clips[idx], idx
end

-- ---------------------------------------------------------------------------
-- M.PlayRandomSFX(audio, clips, volume)
-- ---------------------------------------------------------------------------
-- Play a random clip, avoiding the immediately preceding clip index.
-- Optional volume defaults to 1.0 if not specified.
-- Returns nil if audio is nil or clips is empty/nil.
--
-- Usage:
--   AudioHelper.PlayRandomSFX(self._audio, self.attackSFXClips)
--   AudioHelper.PlayRandomSFX(self._audio, self.attackSFXClips, 0.8)
--
function M.PlayRandomSFX(audio, clips, volume)
    if not audio or not clips or #clips == 0 then return end

    local prevIdx = clips._lastIdx
    local volumeClamped = _clamp(tonumber(volume) or 1.0, 0.0, 1.0)
    local clip, idx = _pickClipNonRepeating(clips)
    if not clip or clip == "" then return end

    if _isDebugEnabled(clips) then
        --print(string.format(
        --    "[AudioHelper] idx=%s prevIdx=%s changed=%s clip=%s volume=%.2f",
        --    tostring(idx),
        --    tostring(prevIdx),
        --    tostring(prevIdx ~= idx),
        --    tostring(clip),
        --    volumeClamped
        --))
    end

    pcall(function()
        audio:SetVolume(volumeClamped)
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

    local prevIdx = clips._lastIdx
    pitchVar = math.abs(tonumber(pitchVar) or 0.1)
    local volumeClamped = _clamp(tonumber(volume) or 1.0, 0.0, 1.0)
    local clip, idx = _pickClipNonRepeating(clips)
    if not clip or clip == "" then return end

    local pitch = 1.0 + (math.random() * 2 - 1) * pitchVar
    pitch = _clamp(pitch, 0.5, 2.0)

    if _isDebugEnabled(clips) then
        --print(string.format(
        --    "[AudioHelper] idx=%s prevIdx=%s changed=%s clip=%s volume=%.2f pitch=%.2f",
        --    tostring(idx),
        --    tostring(prevIdx),
        --    tostring(prevIdx ~= idx),
        --    tostring(clip),
        --    volumeClamped,
        --    pitch
        --))
    end

    pcall(function()
        audio:SetPitch(pitch)
        audio:SetVolume(volumeClamped)
        audio:PlayOneShot(clip)
    end)
end

return M
