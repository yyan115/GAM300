-- ChainAudio.lua
-- Pure Lua module owned and driven by ChainBootstrap.
-- Bootstrap calls New(), Start(), Update(dt, pub, positions, activeN), Cleanup().
--
-- =============================================================================
-- SOUND TRIGGERS SUMMARY
--
--   Throw       one-shot   Link1 (hand)       chain starts extending
--   Retract     one-shot   Link1 (hand)       chain starts retracting
--   HitFlesh    one-shot   Link[tip]           endpoint locks onto entity
--   HitWall     one-shot   Link[tip]           endpoint snaps to geometry
--   Flop        loop       Link[tip]           tip in free physics mode
--   WallRub     loop       Link[mid]           LOS anchors present (chain wrapped)
--   Aim         loop       Link1, 2D           aim camera held
--   Taut        one-shot   Link[mid]           chain transitions lax -> taut
--   Lax         one-shot   Link[mid]           chain transitions taut -> lax
--
-- HitFlesh plays at volume * hitFleshVolMult (default 1.5) to cut through.
-- IsTaut edge is detected frame-to-frame — one-shot fires only on the transition.
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
    local self              = setmetatable({}, {__index = M})
    self.linkName           = linkName or "Link"
    self.clips              = clips    or {}
    self.volume             = tonumber(settings and settings.volume)          or 1.0
    self.minDistance        = tonumber(settings and settings.minDistance)     or 1.0
    self.maxDistance        = tonumber(settings and settings.maxDistance)     or 15.0
    self.dopplerLevel       = tonumber(settings and settings.dopplerLevel)    or 0.5
    self.pitchVariation     = tonumber(settings and settings.pitchVariation)  or 0.1
    self.volVariation       = tonumber(settings and settings.volVariation)    or 0.08
    self.hitFleshVolMult    = tonumber(settings and settings.hitFleshVolMult) or 1.5
    self._state             = STATE.IDLE
    self._aiming            = false
    self._tipIndex          = 1
    self._rubbing           = false
    self._rubLinkIndex      = 1
    self._flopLinkIndex     = 1
    self._prevIsTaut        = false   -- previous frame taut state for edge detection
    self._subAim            = nil
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
end

-- ---------------------------------------------------------------------------
-- Update
-- ---------------------------------------------------------------------------
function M:Update(dt, pub, positions, activeN)
    if not pub then return end
    activeN = activeN or 1
    self._tipIndex = activeN

    -- Chain state machine
    local newState = self:_resolveState(pub)
    if newState ~= self._state then
        self:_onStateChange(self._state, newState, pub, activeN)
        self._state = newState
    end

    -- Taut / Lax edge detection — independent of main state machine
    -- so it fires correctly whether chain is LOCKED, FLOPPING, or IDLE
    local isTaut = pub.IsTaut or false
    if isTaut ~= self._prevIsTaut then
        local midIdx = math.max(1, math.floor(activeN / 2))
        if isTaut then
            local ac = self:_getLink(midIdx)
            self:_playVaried(ac, _pickRandom(self.clips.taut), 1.0, false)
        else
            local ac = self:_getLink(midIdx)
            self:_playVaried(ac, _pickRandom(self.clips.lax), 1.0, false)
        end
        self._prevIsTaut = isTaut
    end

    -- Wall rub loop — driven by LOSAnchorCount, independent of state machine
    local anchorCount = pub.LOSAnchorCount or 0
    local chainActive = (pub.ChainLength or 0) > 1e-4
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
    self._prevIsTaut  = false
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

function M:_configureSpatial(ac, blend, doppler)
    pcall(function()
        ac.SpatialBlend = blend
        ac.MinDistance  = self.minDistance
        ac.MaxDistance  = self.maxDistance
        ac.DopplerLevel = doppler and self.dopplerLevel or 0
    end)
end

-- Play a one-shot with pitch+volume variation. volMult scales on top of base volume.
function M:_playVaried(ac, clipGuid, blend, doppler, volMult)
    if not ac or not clipGuid or clipGuid == "" then return end
    volMult = volMult or 1.0
    pcall(function()
        self:_configureSpatial(ac, blend or 1.0, doppler or false)
        ac:SetPitch(_randVariation(1.0, self.pitchVariation))
        ac:SetVolume(math.min(1.0, _randVariation(self.volume * volMult,
                                                   self.volume * volMult * self.volVariation)))
        ac:PlayOneShot(clipGuid)
    end)
end

function M:_resolveState(pub)
    -- IsRetracting must be checked BEFORE EndPointLocked.
    -- StartRetraction() sets isRetracting=true but does NOT clear endPointLocked,
    -- so both flags are true while pulling an enemy. Checking IsRetracting first
    -- ensures the RETRACTING transition fires and the retract sound plays.
    --
    -- EndPointLocked must still be checked BEFORE IsExtending.
    -- When an entity hit occurs, Bootstrap sets endPointLocked=true but does
    -- not clear isExtending in the same frame, so IsExtending and EndPointLocked
    -- are both true simultaneously. Checking EndPointLocked first ensures the
    -- LOCKED transition fires and HitFlesh plays correctly.
    if pub.IsRetracting                            then return STATE.RETRACTING
    elseif pub.EndPointLocked                      then return STATE.LOCKED
    elseif pub.RaycastSnapped                      then return STATE.LOCKED
    elseif pub.IsExtending                         then return STATE.EXTENDING
    elseif pub.Flopping                            then return STATE.FLOPPING
    else                                                return STATE.IDLE
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
        self:_playVaried(ac, _pickRandom(self.clips.throw), 1.0, false)

    elseif to == STATE.RETRACTING then
        local ac = self:_getLink(1)
        self:_playVaried(ac, _pickRandom(self.clips.retract), 1.0, false)

    elseif to == STATE.FLOPPING then
        local ac = self:_getLink(activeN)
        local clip = _pickRandom(self.clips.flop)
        if ac and clip then
            self._flopLinkIndex = activeN
            pcall(function()
                self:_configureSpatial(ac, 1.0, true)
                ac:SetVolume(self.volume)
                ac:SetPitch(1.0)
                ac:SetClip(clip)
                ac:SetLoop(true)
                ac:Play()
            end)
        end

    elseif to == STATE.LOCKED then
        if pub.EndPointLocked then
            local ac = self:_getLink(activeN)
            self:_playVaried(ac, _pickRandom(self.clips.hitFlesh), 1.0, false, self.hitFleshVolMult)
        elseif pub.RaycastSnapped then
            local ac = self:_getLink(activeN)
            self:_playVaried(ac, _pickRandom(self.clips.hitWall), 1.0, false)
        end

    elseif to == STATE.IDLE then
    end
end

-- Wall rub loop: midchain link, slightly quieter, starts when anchors appear
function M:_startWallRub(activeN)
    local clip = _pickRandom(self.clips.wallRub)
    if not clip then return end

    local rubIdx = math.max(1, math.floor(activeN / 2))
    if self._rubbing and self._rubLinkIndex == rubIdx then return end

    self:_stopWallRub()

    local ac = self:_getLink(rubIdx)
    if not ac then return end

    self._rubbing      = true
    self._rubLinkIndex = rubIdx
    pcall(function()
        self:_configureSpatial(ac, 1.0, false)
        ac:SetVolume(self.volume * 0.6)
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
                self:_configureSpatial(ac, 0.0, false)
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