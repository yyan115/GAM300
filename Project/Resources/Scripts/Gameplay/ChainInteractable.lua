-- ChainInteractable.lua
-- =============================================================================
-- CHAIN INTERACTABLE — Self-contained proximity + isRetracting detection
-- =============================================================================

local Component = require("extension.mono_helper")

return Component {

    -- =========================================================================
    -- INSPECTOR FIELDS
    -- =========================================================================
    fields = {
        BehaviorMode = "Hit",   -- "Hit" | "Pull" | "Mash"
        HitRadius    = 0.35,    -- world-unit proximity radius for endpoint detection
        MashCount    = 3,       -- (Mash) required pull/retract count before MashFinal
    },

    -- =========================================================================
    -- LIFECYCLE
    -- =========================================================================

    Start = function(self)
        -- Resolve own entity id
        self._entityId = nil
        do
            local ok, eid = pcall(function()
                if self.GetEntityId then return self:GetEntityId() end
            end)
            if ok and eid then self._entityId = eid end
        end
        if not self._entityId then
            local ok, eid = pcall(function()
                if self.GetEntity then return self:GetEntity() end
            end)
            if ok and eid then self._entityId = eid end
        end
        if not self._entityId and self.entityId then self._entityId = self.entityId end
        if not self._entityId and self.entity and type(self.entity) == "number" then self._entityId = self.entity end
        if not self._entityId and self.gameObject and self.gameObject.EntityId then self._entityId = self.gameObject.EntityId end

        -- Resolve own transform for world-position reads
        self._transform = nil
        if self._entityId and Engine then
            pcall(function()
                if      Engine.FindTransformByID    then self._transform = Engine.FindTransformByID(self._entityId)
                elseif  Engine.FindTransformByEntity then self._transform = Engine.FindTransformByEntity(self._entityId)
                elseif  Engine.GetTransformForEntity then self._transform = Engine.GetTransformForEntity(self._entityId)
                end
            end)
        end

        -- Per-throw state
        self._isHooked      = false   -- endpoint entered HitRadius this throw
        self._hitFired      = false   -- prevents double-trigger within same throw
        self._actionFired   = false   -- Pull: prevents double-trigger per retraction
        self._mashProgress  = 0       -- Mash: completed pull/retract count
        self._mashDone      = false   -- Mash: final behaviour already fired
        self._mashCounted   = false   -- Mash fallback: prevent counting multiple frames of isRetracting
        self._detachDisarmed = false  -- set when chain detaches mid-throw; blocks hit re-detection until retraction completes

        -- Rolling endpoint positions (for segment sweep)
        self._endpointPos  = nil
        self._endpointPrev = nil

        if not (_G.event_bus and _G.event_bus.subscribe) then
            print("[ChainInteractable] WARNING: event_bus not available — no interactions will fire")
            return
        end

        -- Every frame the chain is active
        self._subMoved = _G.event_bus.subscribe("chain.endpoint_moved", function(payload)
            if payload then pcall(function() self:_onEndpointMoved(payload) end) end
        end)

        -- Chain finished retracting all the way back
        self._subRetracted = _G.event_bus.subscribe("chain.endpoint_retracted", function(payload)
            if payload then pcall(function() self:_onChainRetracted(payload) end) end
        end)

        -- Mash primary: Bootstrap patch vetoed a retraction tap
        self._subPullAttempt = _G.event_bus.subscribe("chain.pull_attempt", function(payload)
            if payload then pcall(function() self:_onPullAttempt(payload) end) end
        end)

        self._subDetached = _G.event_bus.subscribe("chain.detached", function()
            print("[ChainInteractable] Chain detached/flopped — clearing veto (hit detection disarmed until retract)")
            pcall(function() self:_onDetach() end)
        end)

        local rawMode = self.BehaviorMode or "Hit"
        local mode = string.gsub(rawMode, '["\']', '')
        print(string.format("[ChainInteractable] Ready — mode=%s radius=%.2f entityId=%s",
            tostring(mode), tonumber(self.HitRadius) or 0.35, tostring(self._entityId)))
    end,

    -- =========================================================================
    -- INTERNAL
    -- =========================================================================

    _getWorldPos = function(self)
        local tr = self._transform
        if not tr then return nil end

        if Engine and Engine.GetTransformWorldPosition then
            local ok, p = pcall(function() return Engine.GetTransformWorldPosition(tr) end)
            if ok and p then
                if type(p) == "table" then
                    return p[1] or p.x or 0, p[2] or p.y or 0, p[3] or p.z or 0
                end
                if type(p) == "number" then return p, 0, 0 end
            end
        end
        if type(tr.GetPosition) == "function" then
            local ok, a, b, c = pcall(function() return tr:GetPosition() end)
            if ok then
                if type(a) == "table" then return a[1] or a.x or 0, a[2] or a.y or 0, a[3] or a.z or 0 end
                if type(a) == "number" then return a, b, c end
            end
        end
        if tr.localPosition then
            local pos = tr.localPosition
            return pos.x or pos[1] or 0, pos.y or pos[2] or 0, pos.z or pos[3] or 0
        end
        return nil
    end,

    -- -------------------------------------------------------------------------
    -- Called every frame the chain is active.
    -- Handles: proximity hit detection, Pull trigger, Mash fallback trigger.
    -- -------------------------------------------------------------------------
    _onEndpointMoved = function(self, payload)
        -- Clean the string to prevent Inspector quote bugs
        local rawMode = self.BehaviorMode or "Hit"
        local mode = string.gsub(rawMode, '["\']', '')

        -- Roll positions along
        if self._endpointPos then
            self._endpointPrev = { self._endpointPos.x, self._endpointPos.y, self._endpointPos.z }
        end
        if payload.position then self._endpointPos = payload.position end

        -- ── Pull (Fallback): fire the instant the chain starts coming back ────────────
        -- This covers cases where the veto somehow dropped but we still want the behaviour to trigger
        if mode == "Pull" and self._isHooked and not self._actionFired then
            if payload.isRetracting then
                self._actionFired = true
                print("[ChainInteractable] Pull — OnBehaviourPull firing (via fallback)")
                pcall(function() self:OnBehaviourPull() end)
            end
        end


        -- ── Hit detection: segment sweep against own world position ────────
        -- Skip once a hit is already registered this throw, or if the chain
        -- has detached mid-throw (_detachDisarmed), or if the endpoint is
        -- currently locked onto a static wall / in free-fall physics —
        -- in those states the chain is no longer travelling toward us.
        if self._hitFired then return end
        if self._detachDisarmed then return end
        if payload.isFlopping    then return end
        if not self._endpointPos then return end

        local ox, oy, oz = self:_getWorldPos()
        if not ox then
            print("[ChainInteractable] WARNING: could not read own world position — check transform")
            return
        end

        local ex = self._endpointPos.x
        local ey = self._endpointPos.y
        local ez = self._endpointPos.z
        local px = self._endpointPrev and self._endpointPrev[1] or ex
        local py = self._endpointPrev and self._endpointPrev[2] or ey
        local pz = self._endpointPrev and self._endpointPrev[3] or ez

        local radius   = math.max(0.01, tonumber(self.HitRadius) or 0.35)
        local radiusSq = radius * radius

        -- Closest point on segment [prev→current] to our centre
        local sdx, sdy, sdz = ex - px, ey - py, ez - pz
        local segLenSq = sdx*sdx + sdy*sdy + sdz*sdz
        local cx, cy, cz
        if segLenSq < 1e-8 then
            cx, cy, cz = ex, ey, ez
        else
            local t = ((ox-px)*sdx + (oy-py)*sdy + (oz-pz)*sdz) / segLenSq
            t = math.max(0, math.min(1, t))
            cx = px + sdx * t
            cy = py + sdy * t
            cz = pz + sdz * t
        end

        local ddx, ddy, ddz = ox - cx, oy - cy, oz - cz
        local distSq = ddx*ddx + ddy*ddy + ddz*ddz

        if distSq > radiusSq then return end

        -- ── HIT confirmed ────────────────────────────────────────────────
        self._hitFired = true
        self._isHooked = true
        print(string.format("[ChainInteractable] HIT detected — mode=%s dist=%.3f radius=%.3f",
            mode, math.sqrt(distSq), radius))

        -- Force ChainBootstrap to lock the chain onto this interactable
        --if _G.event_bus and _G.event_bus.publish then
        --    _G.event_bus.publish("chain.endpoint_hit_entity", {
        --        entityId = self._entityId,
        --        entityName = "Interactable",
        --        rootTag = "Interactable", 
        --        rootEntityId = self._entityId,
         --       isThrowable = false
        --    })
        --end

        if mode == "Hit" then
            pcall(function() self:OnBehaviourHit() end)

        elseif mode == "Pull" then
            -- Install veto to authorize retraction manually
            _G.chain_retract_veto = function()
                -- 1. If we aren't even hooked anymore, stop vetoing immediately
                if not self._isHooked then 
                    _G.chain_retract_veto = nil
                    return false 
                end

                -- 2. If the logic is "Hit" or "Pull" (no mash), we only veto once
                if self.BehaviorMode ~= "Mash" then
                    return true -- Bootstrap will fire pull_attempt, then we authorize
                end

                -- 3. If it's "Mash", veto until count is reached
                return (self._mashCurrent < self.MashCount)
            end
            print("[ChainInteractable] Pull — Veto installed. Waiting for player to pull.")

        elseif mode == "Mash" then
            self._mashProgress = 0
            self._mashDone     = false
            self._mashCounted  = false
            -- Install veto (PRIMARY path, requires ChainBootstrap_PATCH)
            _G.chain_retract_veto = function()
                return self._isHooked and not self._mashDone
            end
            print("[ChainInteractable] Mash — veto installed")
        end
    end,

    -- -------------------------------------------------------------------------
    -- Chain fully retracted back to player.
    -- -------------------------------------------------------------------------
    _onChainRetracted = function(self, payload)
        local rawMode = self.BehaviorMode or "Hit"
        local mode = string.gsub(rawMode, '["\']', '')

        if mode == "Mash" and self._isHooked and not self._mashDone then
            -- Fallback path: retraction completed mid-mash.
            -- Re-arm so the next throw can land and count as the next mash.
            print(string.format("[ChainInteractable] Mash — retracted at progress %d/%d, re-arming",
                self._mashProgress, math.max(1, tonumber(self.MashCount) or 3)))
            self._hitFired        = false
            self._mashCounted     = false
            self._detachDisarmed  = false   -- re-arm hit detection for the next throw
            -- Keep _isHooked true and _mashProgress as-is (session continues)
            return
        end

        -- After MashFinal or any non-Mash mode: full reset
        self:_clearState()
    end,

    -- -------------------------------------------------------------------------
    -- chain.detached: chain flopped or snapped to a wall mid-throw.
    -- Clears the veto and hooked state but KEEPS _hitFired = true so the
    -- proximity check cannot re-register a hit on the same throw.
    -- _detachDisarmed is also set to block hit detection until the chain
    -- fully retracts and _clearState() resets it.
    -- -------------------------------------------------------------------------
    _onDetach = function(self)
        self._isHooked        = false
        self._actionFired     = false
        self._mashProgress    = 0
        self._mashDone        = false
        self._mashCounted     = false
        self._detachDisarmed  = true   -- prevents hit re-detection for this throw
        self._endpointPos     = nil
        self._endpointPrev    = nil
        -- NOTE: _hitFired intentionally NOT reset — keeps proximity check locked
        -- out for the remainder of this throw.
        if _G.chain_retract_veto ~= nil then _G.chain_retract_veto = nil end
    end,

    -- -------------------------------------------------------------------------
    -- Bootstrap patch: retraction was vetoed, this tap is a mash pull.
    -- -------------------------------------------------------------------------
    _onPullAttempt = function(self, payload)
        if not self._isHooked then return end
        
        -- Clean quotes
        local rawMode = self.BehaviorMode or "Hit"
        local mode = string.gsub(rawMode, '["\']', '')

        if mode == "Pull" and not self._actionFired then
            self._actionFired = true
            _G.chain_retract_veto = nil -- Drop the veto!
            print("[ChainInteractable] Pull — Authorized! Firing OnBehaviourPull")
            pcall(function() self:OnBehaviourPull() end)
            return
        end

        if self._mashDone     then return end
        if mode ~= "Mash" then return end

        self._mashProgress = self._mashProgress + 1
        local max = math.max(1, tonumber(self.MashCount) or 3)
        print(string.format("[ChainInteractable] Mash primary %d/%d", self._mashProgress, max))
        pcall(function() self:OnBehaviourMash(self._mashProgress, max) end)

        if self._mashProgress >= max then
            self._mashDone = true
            _G.chain_retract_veto = nil -- Drop the veto!
            print("[ChainInteractable] Mash FINAL — Authorized! OnBehaviourMashFinal firing")
            pcall(function() self:OnBehaviourMashFinal() end)
        end
    end,

    _clearState = function(self)
        self._isHooked        = false
        self._hitFired        = false
        self._actionFired     = false
        self._mashProgress    = 0
        self._mashDone        = false
        self._mashCounted     = false
        self._detachDisarmed  = false   -- re-arm for the next throw
        self._endpointPos     = nil
        self._endpointPrev    = nil
        if _G.chain_retract_veto ~= nil then _G.chain_retract_veto = nil end
    end,

    -- =========================================================================
    -- CLEANUP
    -- =========================================================================

    OnDisable = function(self)
        self:_clearState()
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._subMoved       then pcall(function() _G.event_bus.unsubscribe(self._subMoved)       end) end
            if self._subRetracted   then pcall(function() _G.event_bus.unsubscribe(self._subRetracted)   end) end
            if self._subPullAttempt then pcall(function() _G.event_bus.unsubscribe(self._subPullAttempt) end) end
            if self._subDetached then pcall(function() _G.event_bus.unsubscribe(self._subDetached) end) end
        end
    end,

    -- =========================================================================
    -- ✏️  BEHAVIOUR FUNCTIONS — FILL THESE IN WHEN COPYING
    -- =========================================================================

    -- [Hit] Fires once when the chain endpoint enters HitRadius.
    OnBehaviourHit = function(self)
        print("[ChainInteractable] OnBehaviourHit — TODO: replace this stub")
    end,

    -- [Pull] Fires the moment the chain starts retracting after a hit.
    OnBehaviourPull = function(self)
        print("[ChainInteractable] OnBehaviourPull — TODO: replace this stub")
    end,

    -- [Mash] Fires once per tap/retract (progress = 1…MashCount).
    OnBehaviourMash = function(self, progress, maxCount)
        print(string.format("[ChainInteractable] OnBehaviourMash %d/%d — TODO: replace this stub", progress, maxCount))
    end,

    -- [Mash] Fires after the final mash. Chain can now retract.
    OnBehaviourMashFinal = function(self)
        print("[ChainInteractable] OnBehaviourMashFinal — TODO: replace this stub")
    end,
}