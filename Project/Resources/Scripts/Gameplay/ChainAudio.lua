-- ChainAudio.lua
-- Pure Lua module owned and driven by ChainBootstrap.
-- Bootstrap calls New(), Start(), Update(dt, pub, positions, activeN), Cleanup().
--
-- =============================================================================
-- SOUND TRIGGERS SUMMARY
--
--   Throw       one-shot   Link1 (hand)        chain starts extending
--   Retract     one-shot   Link1 (hand)        chain starts retracting
--   HitFlesh    one-shot   Link[tip]            endpoint locks onto entity
--   HitWall     one-shot   Link[tip]            endpoint snaps to geometry
--   Flop        loop       Link[1..maxLinks]    up to maxActiveLinks links play
--                                               simultaneously, fastest links
--                                               chosen each frame
--   WallRub     loop       Link[mid]            LOS anchors present
--   Aim         loop       Link1, 2D            aim camera held
--   Taut        one-shot   Link[mid]            chain transitions lax -> taut
--   LaxSlow     one-shot   Link[mid]            taut -> lax, chain moving little
--   LaxFast     one-shot   Link[mid]            taut -> lax, chain moving a lot
--
-- MULTI-LINK FLOP
--   Each frame all active link speeds are computed from position deltas.
--   The top maxActiveLinks (default 3) fastest links are selected to play.
--   Each selected link independently picks lax slow or fast set based on its
--   own speed vs flopSpeedThreshold. Links that fall out of the top N are
--   stopped. This lets the chain layer up to maxActiveLinks simultaneous loops.
--
-- LAX VOLUME
--   laxVolMult (default 2.0) doubles lax volume on top of base AudioVolume.
-- =============================================================================

local M = {}

-- ---------------------------------------------------------------------------
-- Helpers declared FIRST — Lua lexical scope requires this.
-- ---------------------------------------------------------------------------

local function _pickRandom(arr)
    if type(arr) ~= "table" or #arr == 0 then return nil end
    return arr[math.random(1, #arr)]
end

local function _randVariation(base, range)
    return base + (math.random() * 2 - 1) * range
end

local function _posX(p) return p and (p.x or p[1] or 0) or 0 end
local function _posY(p) return p and (p.y or p[2] or 0) or 0 end
local function _posZ(p) return p and (p.z or p[3] or 0) or 0 end

-- ---------------------------------------------------------------------------

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
    self.volume             = tonumber(settings and settings.volume)             or 1.0
    self.minDistance        = tonumber(settings and settings.minDistance)        or 1.0
    self.maxDistance        = tonumber(settings and settings.maxDistance)        or 15.0
    self.dopplerLevel       = tonumber(settings and settings.dopplerLevel)       or 0.5
    self.pitchVariation     = tonumber(settings and settings.pitchVariation)     or 0.1
    self.volVariation       = tonumber(settings and settings.volVariation)       or 0.08
    self.hitFleshVolMult    = tonumber(settings and settings.hitFleshVolMult)    or 2.5
    self.laxSpeedThreshold  = tonumber(settings and settings.laxSpeedThreshold)  or 3.0
    self.flopSpeedThreshold = tonumber(settings and settings.flopSpeedThreshold) or 3.0
    self.maxActiveLinks     = tonumber(settings and settings.maxActiveLinks)     or 3
    self.laxVolMult         = tonumber(settings and settings.laxVolMult)         or 2.0
    self._state             = STATE.IDLE
    self._aiming            = false
    self._rubbing           = false
    self._rubLinkIndex      = 1
    -- Multi-link flop tracking: keyed by link index
    -- _flopState[i] = "slow" | "fast" | nil (nil = not playing)
    self._flopState         = {}
    self._prevPositions     = {}   -- positions[i] from last frame for speed calc
    self._prevIsTaut        = false
    self._prevTipPos        = nil  -- tip position for lax speed calc
    self._subAim            = nil
    return self
end

-- ---------------------------------------------------------------------------
-- Start
-- ---------------------------------------------------------------------------
function M:Start()
    if not (_G.event_bus and _G.event_bus.subscribe) then return end
    -- Guard against double-Start (hot-reload / stop-play cycle)
    if self._subAim and _G.event_bus.unsubscribe then
        pcall(function() _G.event_bus.unsubscribe(self._subAim) end)
        self._subAim = nil
    end
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

    -- Chain state machine
    local newState = self:_resolveState(pub)
    if newState ~= self._state then
        self:_onStateChange(self._state, newState, pub, activeN, positions, dt)
        self._state = newState
    end

    -- Taut / Lax edge detection
    local isTaut = pub.IsTaut or false
    if isTaut ~= self._prevIsTaut then
        -- Try mid link first, then tip, then link 1.
        -- Not every link entity has an AudioComponent — walk candidates until one resolves.
        local midIdx = math.max(1, math.floor(activeN / 2))
        local ac = self:_getLink(midIdx)
                 or self:_getLink(activeN)
                 or self:_getLink(1)
        if isTaut then
            self:_playVaried(ac, _pickRandom(self.clips.taut), 1.0, false)
        else
            -- Pick lax set by tip speed. Falls back to clips.lax if split sets are empty.
            local speed = self:_linkSpeed(activeN, positions, dt)
            local clipSet
            if speed >= self.laxSpeedThreshold and self.clips.laxFast and #self.clips.laxFast > 0 then
                clipSet = self.clips.laxFast
            elseif self.clips.laxSlow and #self.clips.laxSlow > 0 then
                clipSet = self.clips.laxSlow
            else
                clipSet = self.clips.lax
            end
            self:_playVaried(ac, _pickRandom(clipSet), 1.0, false, self.laxVolMult)
        end
        self._prevIsTaut = isTaut
    end

    -- Multi-link flop update
    if self._state == STATE.FLOPPING then
        self:_updateMultiFlop(dt, positions, activeN)
    end

    -- Wall rub loop
    local anchorCount = pub.LOSAnchorCount or 0
    local chainActive = (pub.ChainLength or 0) > 1e-4
    if anchorCount > 0 and chainActive then
        self:_startWallRub(activeN)
    else
        self:_stopWallRub()
    end

    -- Store positions for next frame
    if positions then
        for i = 1, activeN do
            if positions[i] then
                self._prevPositions[i] = positions[i]
            end
        end
    end
    if positions and positions[activeN] then
        self._prevTipPos = positions[activeN]
    end
end

-- ---------------------------------------------------------------------------
-- Cleanup
-- ---------------------------------------------------------------------------
function M:Cleanup()
    self:_stopWallRub()
    self:_stopAllFlop()
    pcall(function()
        local ac = self:_getLink(1)
        if ac and ac:GetIsPlaying() then ac:Stop() end
    end)
    self._prevIsTaut    = false
    self._prevPositions = {}
    self._prevTipPos    = nil
    self._flopState     = {}
    if _G.event_bus and _G.event_bus.unsubscribe then
        if self._subAim then pcall(function() _G.event_bus.unsubscribe(self._subAim) end) end
    end
end

-- ---------------------------------------------------------------------------
-- Internal
-- ---------------------------------------------------------------------------

function M:_linkSpeed(index, positions, dt)
    if not positions or not positions[index] or not self._prevPositions[index] then
        return 0.0
    end
    if not dt or dt <= 0 then return 0.0 end
    local cur  = positions[index]
    local prev = self._prevPositions[index]
    local dx = _posX(cur) - _posX(prev)
    local dy = _posY(cur) - _posY(prev)
    local dz = _posZ(cur) - _posZ(prev)
    return math.sqrt(dx*dx + dy*dy + dz*dz) / dt
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

function M:_startFlopOnLink(index, mode)
    local clipSet = (mode == "fast") and self.clips.flopFast or self.clips.flopSlow
    -- Fall back to the other set if the intended one is empty
    if (not clipSet or #clipSet == 0) and self.clips.flop then
        clipSet = self.clips.flop
    end
    local clip = _pickRandom(clipSet)
    local ac   = self:_getLink(index)
    if not ac or not clip then return end
    self._flopState[index] = mode
    pcall(function()
        self:_configureSpatial(ac, 1.0, true)
        ac:SetPitch(_randVariation(1.0, self.pitchVariation))
        ac:SetVolume(math.min(1.0, _randVariation(self.volume,
                                                   self.volume * self.volVariation)))
        ac:SetClip(clip)
        ac:SetLoop(true)
        ac:Play()
    end)
end

function M:_stopFlopOnLink(index)
    if not self._flopState[index] then return end
    self._flopState[index] = nil
    pcall(function()
        local ac = self:_getLink(index)
        if ac and ac:GetIsPlaying() then ac:Stop() end
    end)
end

function M:_stopAllFlop()
    for i in pairs(self._flopState) do
        self:_stopFlopOnLink(i)
    end
    self._flopState = {}
end

-- Each frame: compute speed per link, pick top maxActiveLinks, play correct set.
function M:_updateMultiFlop(dt, positions, activeN)
    if not positions then return end

    -- Compute speed for every active link
    local speeds = {}
    for i = 1, activeN do
        speeds[i] = self:_linkSpeed(i, positions, dt)
    end

    -- Sort indices by speed descending, cap at maxActiveLinks
    local indices = {}
    for i = 1, activeN do indices[#indices+1] = i end
    table.sort(indices, function(a, b) return speeds[a] > speeds[b] end)

    local limit    = math.min(self.maxActiveLinks, activeN)
    local selected = {}
    for i = 1, limit do
        selected[indices[i]] = true
    end

    -- Stop links no longer selected
    for idx in pairs(self._flopState) do
        if not selected[idx] then
            self:_stopFlopOnLink(idx)
        end
    end

    -- Start or switch mode for selected links
    for idx in pairs(selected) do
        local mode = (speeds[idx] >= self.flopSpeedThreshold) and "fast" or "slow"
        if self._flopState[idx] ~= mode then
            -- Mode changed (or not yet playing) — restart with correct clip set
            self:_stopFlopOnLink(idx)
            self:_startFlopOnLink(idx, mode)
        end
    end
end

function M:_resolveState(pub)
    if pub.IsRetracting       then return STATE.RETRACTING
    elseif pub.EndPointLocked then return STATE.LOCKED
    elseif pub.RaycastSnapped then return STATE.LOCKED
    elseif pub.IsExtending    then return STATE.EXTENDING
    elseif pub.Flopping       then return STATE.FLOPPING
    else                           return STATE.IDLE
    end
end

function M:_onStateChange(from, to, pub, activeN, positions, dt)
    if from == STATE.FLOPPING then
        self:_stopAllFlop()
    end

    if to == STATE.EXTENDING then
        local ac = self:_getLink(1)
        self:_playVaried(ac, _pickRandom(self.clips.throw), 1.0, false)

    elseif to == STATE.RETRACTING then
        local ac = self:_getLink(1)
        self:_playVaried(ac, _pickRandom(self.clips.retract), 1.0, false)

    elseif to == STATE.FLOPPING then
        -- _updateMultiFlop will handle initial link selection this frame
        self._flopState     = {}
        self._prevPositions = {}

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

function M:_startWallRub(activeN)
    local clip = _pickRandom(self.clips.wallRub)
    if not clip then return end
    local rubIdx = math.max(1, math.floor(activeN / 2))
    -- Fall back to tip or link 1 if mid has no AudioComponent
    local ac = self:_getLink(rubIdx)
    if not ac then rubIdx = activeN;      ac = self:_getLink(rubIdx) end
    if not ac then rubIdx = 1;            ac = self:_getLink(rubIdx) end
    if not ac then return end
    if self._rubbing and self._rubLinkIndex == rubIdx then return end
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