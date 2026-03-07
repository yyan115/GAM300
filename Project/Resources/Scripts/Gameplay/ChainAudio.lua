-- ChainAudio.lua
-- Pure Lua module owned and driven by ChainBootstrap.
-- Bootstrap calls New(), Start(), Update(dt, pub, positions, activeN), Cleanup().
--
-- =============================================================================
-- FIXES IN THIS VERSION
--   1. HitFlesh moved from event subscription to state transition so it fires
--      reliably at the correct activeN without timing/ordering issues.
--   2. Flop clip now uses _pickRandom(array) — fixes silent flop caused by
--      engine storing the field as a table instead of a plain string.
--   3. Wall rub loop added: plays while LOSAnchorCount > 0 (chain wrapped
--      around geometry). Requires new asset — see MISSING ASSETS below.
--
-- MISSING ASSETS (none of the current 12 sounds suit these slots)
--   AudioClips_WallRub
--     Looping metallic chain scraping/dragging against stone.
--     Duration  : seamlessly loopable, ~2 sec loop point
--     Character : dry metallic drag, light resonance, not percussive
--     Trigger   : starts when first LOS anchor appears, stops when all clear
--
--   AudioClips_Aim
--     Looping ambient while aim camera is held.
--     Currently no asset — leave array empty until sourced.
--
-- LINK SPATIAL STRATEGY
--   Throw / Retract : Link1 (hand) — sound originates at player
--   HitFlesh        : Link[activeN] (tip) — impact at endpoint
--   HitWall         : Link[activeN] (tip) — impact at geometry hit point
--   Flop loop       : Link[activeN] (tip) — moves with physics tip
--   Wall rub loop   : Link[activeN/2] (midchain) — approximates wrap point
--   Aim loop        : Link1, SpatialBlend=0 (2D)
-- =============================================================================

local M = {}

local STATE = {
    IDLE       = "idle",
    EXTENDING  = "extending",
    RETRACTING = "retracting",
    FLOPPING   = "flopping",
    LOCKED     = "locked",
}

function M.New(linkName, clips, settings)
    local self           = setmetatable({}, {__index = M})
    self.linkName        = linkName or "Link"
    self.clips           = clips    or {}
    self.volume          = tonumber(settings and settings.volume)         or 1.0
    self.minDistance     = tonumber(settings and settings.minDistance)    or 1.0
    self.maxDistance     = tonumber(settings and settings.maxDistance)    or 15.0
    self.dopplerLevel    = tonumber(settings and settings.dopplerLevel)   or 0.5
    self.pitchVariation  = tonumber(settings and settings.pitchVariation) or 0.1
    self.volVariation    = tonumber(settings and settings.volVariation)   or 0.08
    self._state          = STATE.IDLE
    self._aiming         = false
    self._tipIndex       = 1
    self._rubbing        = false   -- true while wall-rub loop is active
    self._rubLinkIndex   = 1       -- which link the rub loop is playing on
    self._flopLinkIndex  = 1       -- which link the flop loop is playing on
    self._configured     = {}
    self._subAim         = nil
    return self
end

-- ---------------------------------------------------------------------------
-- Start
-- ---------------------------------------------------------------------------
function M:Start()
    if not (_G.event_bus and _G.event_bus.subscribe) then return end

    self._subAim = _G.event_bus.subscribe("chain.aim_camera", function(payload)
        if not payload then return end
        pcall(function() self:_onAim(payload.active == true) end)
    end)
    -- NOTE: HitFlesh is now handled in _onStateChange (LOCKED + EndPointLocked)
    -- rather than via event subscription, so timing is deterministic.
end

-- ---------------------------------------------------------------------------
-- Update
-- ---------------------------------------------------------------------------
function M:Update(dt, pub, positions, activeN)
    if not pub then return end
    activeN = activeN or 1
    self._tipIndex = activeN

    -- State machine — drives throw/retract/flop/hit sounds
    local newState = self:_resolveState(pub)
    if newState ~= self._state then
        self:_onStateChange(self._state, newState, pub, activeN)
        self._state = newState
    end

    -- Wall rub loop — independent of state machine, driven by LOSAnchorCount
    local anchorCount = pub.LOSAnchorCount or 0
    local chainActive = pub.ChainLength and pub.ChainLength > 1e-4
    if anchorCount > 0 and chainActive then
        self:_startWallRub(activeN)
    else
        self:_stopWallRub()
    end
end

-- ---------------------------------------------------------------------------
-- Cleanup
-- ---------------------------------------------------------------------------
function M:Cleanup()
    self:_stopWallRub()
    pcall(function()
        local ac = self:_getLink(1)
        if ac and ac:GetIsPlaying() then ac:Stop() end
    end)
    pcall(function()
        local ac = self:_getLink(self._flopLinkIndex)
        if ac and ac:GetIsPlaying() then ac:Stop() end
    end)
    self._configured = {}
    if _G.event_bus and _G.event_bus.unsubscribe then
        if self._subAim then pcall(function() _G.event_bus.unsubscribe(self._subAim) end) end
    end
end

-- ---------------------------------------------------------------------------
-- Internal
-- ---------------------------------------------------------------------------

local function _pickRandom(arr)
    if type(arr) ~= "table" or #arr == 0 then return nil end
    return arr[math.random(1, #arr)]
end

local function _randVariation(base, range)
    return base + (math.random() * 2 - 1) * range
end

function M:_getLink(index)
    local ok, ac = pcall(function()
        return Engine.FindAudioCompByName(self.linkName .. tostring(index))
    end)
    return (ok and ac ~= nil) and ac or nil
end

function M:_ensureConfigured(ac, index, blend, doppler)
    local key = tostring(index) .. "_" .. tostring(blend)
    if self._configured[key] then return end
    pcall(function()
        ac.SpatialBlend = blend
        ac.MinDistance  = self.minDistance
        ac.MaxDistance  = self.maxDistance
        ac.DopplerLevel = doppler and self.dopplerLevel or 0
    end)
    self._configured[key] = true
end

function M:_playVaried(ac, index, clipGuid, blend, doppler)
    if not ac or not clipGuid or clipGuid == "" then return end
    pcall(function()
        self:_ensureConfigured(ac, index, blend or 1.0, doppler or false)
        ac:SetPitch(_randVariation(1.0, self.pitchVariation))
        ac:SetVolume(_randVariation(self.volume, self.volume * self.volVariation))
        ac:PlayOneShot(clipGuid)
    end)
end

function M:_resolveState(pub)
    if pub.IsRetracting                             then return STATE.RETRACTING
    elseif pub.IsExtending                          then return STATE.EXTENDING
    elseif pub.Flopping                             then return STATE.FLOPPING
    elseif pub.EndPointLocked or pub.RaycastSnapped then return STATE.LOCKED
    else                                                 return STATE.IDLE
    end
end

function M:_onStateChange(from, to, pub, activeN)
    -- Stop flop loop when leaving flop state
    if from == STATE.FLOPPING then
        pcall(function()
            local ac = self:_getLink(self._flopLinkIndex)
            if ac and ac:GetIsPlaying() then ac:Stop() end
        end)
    end

    if to == STATE.EXTENDING then
        local ac = self:_getLink(1)
        self:_playVaried(ac, 1, _pickRandom(self.clips.throw), 1.0, false)

    elseif to == STATE.RETRACTING then
        local ac = self:_getLink(1)
        self:_playVaried(ac, 1, _pickRandom(self.clips.retract), 1.0, false)

    elseif to == STATE.FLOPPING then
        -- Flop: looping, plays from tip link, doppler on
        local ac = self:_getLink(activeN)
        local clip = _pickRandom(self.clips.flop)
        if ac and clip then
            self._flopLinkIndex = activeN
            self:_ensureConfigured(ac, activeN, 1.0, true)
            pcall(function()
                ac:SetVolume(self.volume)
                ac:SetPitch(1.0)
                ac:SetClip(clip)
                ac:SetLoop(true)
                ac:Play()
            end)
        end

    elseif to == STATE.LOCKED then
        -- HitFlesh: endpoint locked onto an entity — play from tip link
        if pub.EndPointLocked then
            local ac = self:_getLink(activeN)
            self:_playVaried(ac, activeN, _pickRandom(self.clips.hitFlesh), 1.0, false)
        -- HitWall: geometry raycast snap — play from tip link
        elseif pub.RaycastSnapped then
            local ac = self:_getLink(activeN)
            self:_playVaried(ac, activeN, _pickRandom(self.clips.hitWall), 1.0, false)
        end

    elseif to == STATE.IDLE then
        -- no sound needed
    end
end

-- Wall rub: start loop if not already playing on the correct link
function M:_startWallRub(activeN)
    local clip = _pickRandom(self.clips.wallRub)
    if not clip then return end  -- no asset assigned yet

    -- Use midchain link as approximate anchor contact point
    local rubIdx = math.max(1, math.floor(activeN / 2))

    -- Already rubbing on the same link — do nothing
    if self._rubbing and self._rubLinkIndex == rubIdx then return end

    -- Stop old rub if it was on a different link
    self:_stopWallRub()

    local ac = self:_getLink(rubIdx)
    if not ac then return end

    self._rubbing      = true
    self._rubLinkIndex = rubIdx
    pcall(function()
        self:_ensureConfigured(ac, rubIdx, 1.0, false)
        ac:SetVolume(self.volume * 0.6)  -- slightly quieter than hit sounds
        ac:SetPitch(1.0)
        ac:SetClip(clip)
        ac:SetLoop(true)
        ac:Play()
    end)
end

function M:_stopWallRub()
    if not self._rubbing then return end
    self._rubbing = false
    pcall(function()
        local ac = self:_getLink(self._rubLinkIndex)
        if ac and ac:GetIsPlaying() then ac:Stop() end
    end)
end

-- Aim: 2D loop from Link1
function M:_onAim(active)
    local ac = self:_getLink(1)
    if not ac then return end
    if active and not self._aiming then
        self._aiming = true
        local clip = _pickRandom(self.clips.aim)
        if clip then
            pcall(function()
                self:_ensureConfigured(ac, 1, 0.0, false)
                ac:SetVolume(self.volume)
                ac:SetPitch(1.0)
                ac:SetClip(clip)
                ac:SetLoop(true)
                ac:Play()
            end)
        end
    elseif not active and self._aiming then
        self._aiming = false
        pcall(function() ac:Stop() end)
    end
end

return M