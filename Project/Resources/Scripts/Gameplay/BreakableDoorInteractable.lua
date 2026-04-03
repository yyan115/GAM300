-- BreakableDoorInteractable.lua
-- =============================================================================
-- BREAKABLE DOOR — Dual-door Mash interactable (attach to BOTH door entities)
-- =============================================================================
-- Attach this same script to BreakableDoor1 and BreakableDoor2.
-- Both instances share state via _G.BreakableDoorRegistry so that whichever
-- door the chain hits drives BOTH doors to their open state.
-- =============================================================================

local Component = require("extension.mono_helper")

-- =============================================================================
-- DOOR TRANSFORM STATES
-- =============================================================================
-- pos  = { x, y, z }
-- quat = { w, x, y, z }  ← quaternion, written via localRotation (same pattern
--                            as ChainLinkTransformHandler:ApplyRotations)
--
-- Quaternion derivations from inspector Euler angles (XYZ = Pitch, Yaw, Roll):
--   Euler (180, 0, 180) → quat (w= 0,       x=0, y=1,       z=0)  [confirmed via Info panel]
--   Euler (180, 3, 180) → quat (w=-0.02618, x=0, y=0.99966, z=0)
--   Euler (180,-3, 180) → quat (w= 0.02618, x=0, y=0.99966, z=0)
-- =============================================================================

-- Quaternion reference (Euler XYZ, base rotation 180,0,180 = w=0 y=1):
--   Euler (180,  6, 180) → w= 0.05234  y=0.99863   (Door1 opens RIGHT 6°, flipped & doubled)
--   Euler (180, -6, 180) → w=-0.05234  y=0.99863   (Door2 opens LEFT  6°, flipped & doubled)
--   sin(3°)=0.05234  cos(3°)=0.99863
local DOOR_STATES = {
    BreakableDoor1 = {
        [1] = {
            pos  = { 4.243,  3.250, 23.204 },
            quat = { w = 0.05234, x = 0, y = 0.99863, z = 0 },
        },
        [0] = {
            pos  = { 4.243,  3.250, 23.192 },
            quat = { w = 0, x = 0, y = 1, z = 0 },
        },
    },
    BreakableDoor2 = {
        [1] = {
            pos  = { 2.910,  3.250, 23.203 },
            quat = { w = -0.05234, x = 0, y = 0.99863, z = 0 },
        },
        [0] = {
            pos  = { 2.910,  3.250, 23.191 },
            quat = { w = 0, x = 0, y = 1, z = 0 },
        },
    },
}

-- =============================================================================
-- HELPERS
-- =============================================================================

-- Quaternion slerp — mirrors the slerp block in ChainLinkTransformHandler:ApplyRotations
local function quatSlerp(aw,ax,ay,az, bw,bx,by,bz, t)
    local dot = aw*bw + ax*bx + ay*by + az*bz
    -- shortest path
    if dot < 0 then bw,bx,by,bz = -bw,-bx,-by,-bz; dot = -dot end
    if dot > 0.9995 then
        -- linear fallback when nearly identical
        local rw = aw + t*(bw-aw)
        local rx = ax + t*(bx-ax)
        local ry = ay + t*(by-ay)
        local rz = az + t*(bz-az)
        local l = math.sqrt(rw*rw+rx*rx+ry*ry+rz*rz)
        if l < 1e-12 then return 1,0,0,0 end
        return rw/l, rx/l, ry/l, rz/l
    end
    dot = math.max(-1, math.min(1, dot))
    local theta = math.acos(dot)
    local sinTheta = math.sin(theta)
    local s0 = math.sin((1-t)*theta) / sinTheta
    local s1 = math.sin(   t *theta) / sinTheta
    local rw = aw*s0 + bw*s1
    local rx = ax*s0 + bx*s1
    local ry = ay*s0 + by*s1
    local rz = az*s0 + bz*s1
    local l = math.sqrt(rw*rw+rx*rx+ry*ry+rz*rz)
    if l < 1e-12 then return 1,0,0,0 end
    return rw/l, rx/l, ry/l, rz/l
end

-- =============================================================================
-- COMPONENT
-- =============================================================================

return Component {

    -- =========================================================================
    -- INSPECTOR FIELDS
    -- =========================================================================
    fields = {
        BehaviorMode    = "Mash",
        HitRadius       = 1.0,
        MashCount       = 2,        -- number of mash taps to fully open the doors
        -- Set this explicitly in the inspector to match the entity name,
        -- e.g. "BreakableDoor1" or "BreakableDoor2".  Fixes the nil-name
        -- issue that prevents transform lookup from succeeding.
        DoorName        = "",
        -- Additional Z-axis nudge applied while the doors are rotating (world units).
        -- Scales linearly with mash progress so the doors slide forward as they open.
        MashZShift      = 0.05,
        -- Name of the static virtual door entity that should be deactivated once
        -- the mash is complete (uses ActiveComponent.isActive = false).
        VirtualDoorName = "",
    },

    -- =========================================================================
    -- LIFECYCLE
    -- =========================================================================

    Start = function(self)
        -- ── Resolve own entity id ─────────────────────────────────────────
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

        -- ── Resolve own transform ─────────────────────────────────────────
        self._transform = nil
        if self._entityId and Engine then
            pcall(function()
                if      Engine.FindTransformByID     then self._transform = Engine.FindTransformByID(self._entityId)
                elseif  Engine.FindTransformByEntity then self._transform = Engine.FindTransformByEntity(self._entityId)
                elseif  Engine.GetTransformForEntity then self._transform = Engine.GetTransformForEntity(self._entityId)
                end
            end)
        end

        -- ── Resolve own rigidbody ─────────────────────────────────────────
        self._rigidbody = nil
        pcall(function()
            if      self.GetComponent         then self._rigidbody = self:GetComponent("RigidBodyComponent")
            elseif  self.gameObject and self.gameObject.GetComponent
                                              then self._rigidbody = self.gameObject:GetComponent("RigidBodyComponent")
            end
        end)
        if not self._rigidbody then
            print("[BreakableDoor] WARNING: no RigidBodyComponent found on this door")
        end

        -- ── Pin rigidbody to transform (kinematic) ────────────────────────
        -- The door must follow its transform at all times during the mash.
        -- We set motionID = 0 here and never change it; AddImpulse on the
        -- final mash will still fire the door outward.
        if self._rigidbody then
            pcall(function()
                self._rigidbody.motionID    = 0
                self._rigidbody.motion_dirty = true
            end)
        end

        -- ── Resolve own entity name ───────────────────────────────────────
        -- Priority 1: explicit inspector field (most reliable — fixes nil-name bug)
        self._doorName = nil
        local configuredName = tostring(self.DoorName or ""):match("^%s*(.-)%s*$")
        if configuredName and configuredName ~= "" then
            self._doorName = configuredName
        end
        -- Priority 2: runtime API fallbacks
        if not self._doorName then
            pcall(function()
                if      self.GetName    then self._doorName = self:GetName()
                elseif  self.name       then self._doorName = self.name
                elseif  self.gameObject and self.gameObject.name then self._doorName = self.gameObject.name
                end
            end)
        end
        if not self._doorName then
            print("[BreakableDoor] WARNING: DoorName not resolved — set the DoorName field in the inspector!")
        end

        -- ── Register self in the global door registry ─────────────────────
        if not _G.BreakableDoorRegistry then _G.BreakableDoorRegistry = {} end
        local key = self._doorName or tostring(self._entityId)
        _G.BreakableDoorRegistry[key] = self
        print(string.format("[BreakableDoor] '%s' registered (entityId=%s)", key, tostring(self._entityId)))

        -- ── Shared broken flag ────────────────────────────────────────────
        if _G.BreakableDoorsBroken == nil then _G.BreakableDoorsBroken = false end

        -- ── Per-throw state ───────────────────────────────────────────────
        self._isHooked       = false
        self._hitFired       = false
        self._actionFired    = false
        self._mashProgress   = 0
        self._mashDone       = false
        self._mashCounted    = false
        self._detachDisarmed = false
        self._endpointPos    = nil
        self._endpointPrev   = nil
        self._currentT       = 0   -- lerp fraction; driven by mash taps, consumed by Update

        if not (_G.event_bus and _G.event_bus.subscribe) then
            print("[BreakableDoor] WARNING: event_bus not available — no interactions will fire")
            return
        end

        self._subMoved = _G.event_bus.subscribe("chain.endpoint_moved", function(payload)
            if payload then pcall(function() self:_onEndpointMoved(payload) end) end
        end)
        self._subRetracted = _G.event_bus.subscribe("chain.endpoint_retracted", function(payload)
            if payload then pcall(function() self:_onChainRetracted(payload) end) end
        end)
        self._subPullAttempt = _G.event_bus.subscribe("chain.pull_attempt", function(payload)
            if payload then pcall(function() self:_onPullAttempt(payload) end) end
        end)
        self._subDetached = _G.event_bus.subscribe("chain.detached", function()
            pcall(function() self:_onDetach() end)
        end)

        print(string.format("[BreakableDoor] Ready — door='%s' mode=Mash radius=%.2f mashes=%d",
            tostring(self._doorName), tonumber(self.HitRadius) or 1.0, tonumber(self.MashCount) or 1))
    end,

    -- =========================================================================
    -- INTERNAL — world position read (unchanged from ChainInteractable template)
    -- =========================================================================

    _getWorldPos = function(self)
        local tr = self._transform
        if not tr then return nil end
        if Engine and Engine.GetTransformWorldPosition then
            local ok, p = pcall(function() return Engine.GetTransformWorldPosition(tr) end)
            if ok and p then
                if type(p) == "table" then return p[1] or p.x or 0, p[2] or p.y or 0, p[3] or p.z or 0 end
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

    -- =========================================================================
    -- INTERNAL — write position
    -- Mirrors write_pos_safe from ChainLinkTransformHandler:
    --   1. tr:SetPosition(x, y, z)
    --   2. tr.localPosition field write + isDirty = true
    -- =========================================================================

    _writePos = function(self, tr, x, y, z)
        if not tr then return false end
        if type(tr.SetPosition) == "function" then
            local ok = pcall(function() tr:SetPosition(x, y, z) end)
            if ok then return true end
        end
        pcall(function()
            local pos = tr.localPosition
            if type(pos) == "table" or type(pos) == "userdata" then
                pos.x, pos.y, pos.z = x, y, z
                tr.isDirty = true
            end
        end)
        return true
    end,

    -- =========================================================================
    -- INTERNAL — write quaternion rotation
    -- Mirrors ChainLinkTransformHandler:ApplyRotations write block:
    --   local rot = transform.localRotation
    --   rot.w, rot.x, rot.y, rot.z = w, x, y, z
    --   transform.isDirty = true
    -- =========================================================================

    _writeRot = function(self, tr, w, x, y, z)
        if not tr then return false end
        pcall(function()
            local rot = tr.localRotation
            if type(rot) == "table" or type(rot) == "userdata" then
                rot.w, rot.x, rot.y, rot.z = w, x, y, z
                tr.isDirty = true
            end
        end)
        return true
    end,

    -- =========================================================================
    -- INTERNAL — get the transform stored on any registered door instance
    -- =========================================================================

    _getTransformForDoor = function(self, doorName)
        local inst = _G.BreakableDoorRegistry and _G.BreakableDoorRegistry[doorName]
        if inst then return inst._transform end
        return nil
    end,

    -- =========================================================================
    -- INTERNAL — transpose ALL doors to the given state index (snap)
    -- =========================================================================

    _transposeDoors = function(self, stateIndex)
        print(string.format("[BreakableDoor] Transposing all doors to state %d", stateIndex))
        for doorName, states in pairs(DOOR_STATES) do
            local state = states[stateIndex]
            if not state then
                print(string.format("[BreakableDoor] WARNING: no state[%d] defined for '%s'", stateIndex, doorName))
            else
                local tr = self:_getTransformForDoor(doorName)
                if tr then
                    local p = state.pos
                    local q = state.quat
                    self:_writePos(tr, p[1], p[2], p[3])
                    self:_writeRot(tr, q.w, q.x, q.y, q.z)
                    print(string.format(
                        "[BreakableDoor] '%s' → state %d | pos(%.3f,%.3f,%.3f) | quat(w=%.5f x=%.5f y=%.5f z=%.5f)",
                        doorName, stateIndex, p[1], p[2], p[3], q.w, q.x, q.y, q.z))
                else
                    print(string.format("[BreakableDoor] WARNING: transform not found for '%s'", doorName))
                end
            end
        end
    end,

    -- =========================================================================
    -- INTERNAL — interpolate ALL doors between state[0] (closed) and state[1]
    -- (open) by fraction t in [0, 1].  Called on each mash tap so the doors
    -- visibly rotate a little further with every press.
    -- A MashZShift offset is also applied along Z so the doors slide forward
    -- proportionally as they open.
    -- =========================================================================

    _transposeDoorsFraction = function(self, t)
        t = math.max(0, math.min(1, t))
        local zShift = tonumber(self.MashZShift) or 0.05
        for doorName, states in pairs(DOOR_STATES) do
            local s0 = states[0]
            local s1 = states[1]
            if not s0 or not s1 then
                print(string.format("[BreakableDoor] WARNING: missing state for '%s', skipping lerp", doorName))
            else
                local tr = self:_getTransformForDoor(doorName)
                if tr then
                    -- lerp position + Z nudge scaled by mash progress
                    local px = s0.pos[1] + (s1.pos[1] - s0.pos[1]) * t
                    local py = s0.pos[2] + (s1.pos[2] - s0.pos[2]) * t
                    local pz = s0.pos[3] + (s1.pos[3] - s0.pos[3]) * t + zShift * t
                    self:_writePos(tr, px, py, pz)

                    -- slerp rotation (using module-level quatSlerp)
                    local qw, qx, qy, qz = quatSlerp(
                        s0.quat.w, s0.quat.x, s0.quat.y, s0.quat.z,
                        s1.quat.w, s1.quat.x, s1.quat.y, s1.quat.z,
                        t)
                    self:_writeRot(tr, qw, qx, qy, qz)

                    print(string.format(
                        "[BreakableDoor] '%s' lerp t=%.3f | pos(%.3f,%.3f,%.3f) | quat(w=%.5f x=%.5f y=%.5f z=%.5f)",
                        doorName, t, px, py, pz, qw, qx, qy, qz))
                else
                    print(string.format("[BreakableDoor] WARNING: transform not found for '%s'", doorName))
                end
            end
        end
    end,

    -- =========================================================================
    -- INTERNAL — endpoint moved (segment-sweep proximity, same as template)
    -- =========================================================================

    _onEndpointMoved = function(self, payload)
        if _G.BreakableDoorsBroken then return end

        if self._endpointPos then
            self._endpointPrev = { self._endpointPos.x, self._endpointPos.y, self._endpointPos.z }
        end
        if payload.position then self._endpointPos = payload.position end

        if self._hitFired       then return end
        if self._detachDisarmed then return end
        if payload.isFlopping   then return end
        if not self._endpointPos then return end

        local ox, oy, oz = self:_getWorldPos()
        if not ox then
            print("[BreakableDoor] WARNING: could not read own world position")
            return
        end

        local ex = self._endpointPos.x
        local ey = self._endpointPos.y
        local ez = self._endpointPos.z
        local px = self._endpointPrev and self._endpointPrev[1] or ex
        local py = self._endpointPrev and self._endpointPrev[2] or ey
        local pz = self._endpointPrev and self._endpointPrev[3] or ez

        local radius   = math.max(0.01, tonumber(self.HitRadius) or 1.0)
        local radiusSq = radius * radius

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

        -- ── HIT confirmed ──────────────────────────────────────────────────
        self._hitFired = true
        self._isHooked = true
        print(string.format("[BreakableDoor] HIT on '%s' dist=%.3f radius=%.3f",
            tostring(self._doorName), math.sqrt(distSq), radius))

        self._mashProgress = 0
        self._mashDone     = false
        self._mashCounted  = false
        _G.chain_retract_veto = function()
            return self._isHooked and not self._mashDone
        end
        print("[BreakableDoor] Mash veto installed — waiting for player to mash")
    end,

    -- =========================================================================
    -- INTERNAL — chain fully retracted
    -- =========================================================================

    _onChainRetracted = function(self, payload)
        if self._isHooked and not self._mashDone then
            print(string.format("[BreakableDoor] '%s' retracted at progress %d/%d — re-arming",
                tostring(self._doorName), self._mashProgress, math.max(1, tonumber(self.MashCount) or 1)))
            self._hitFired       = false
            self._mashCounted    = false
            self._detachDisarmed = false
            return
        end
        self:_clearState()
    end,

    -- =========================================================================
    -- INTERNAL — chain detached mid-throw
    -- =========================================================================

    _onDetach = function(self)
        print(string.format("[BreakableDoor] '%s' detached — disarming until retract", tostring(self._doorName)))
        self._isHooked       = false
        self._actionFired    = false
        self._mashProgress   = 0
        self._mashDone       = false
        self._mashCounted    = false
        self._detachDisarmed = true
        self._endpointPos    = nil
        self._endpointPrev   = nil
        if _G.chain_retract_veto ~= nil then _G.chain_retract_veto = nil end
    end,

    -- =========================================================================
    -- INTERNAL — pull attempt (each mash tap counted here)
    -- =========================================================================

    _onPullAttempt = function(self, payload)
        if not self._isHooked then return end
        if self._mashDone     then return end

        local rawMode = self.BehaviorMode or "Mash"
        local mode = string.gsub(rawMode, '["\']', '')
        if mode ~= "Mash" then return end

        self._mashProgress = self._mashProgress + 1
        local max = math.max(1, tonumber(self.MashCount) or 1)
        print(string.format("[BreakableDoor] Mash tap %d/%d on '%s'", self._mashProgress, max, tostring(self._doorName)))
        pcall(function() self:OnBehaviourMash(self._mashProgress, max) end)

        if self._mashProgress >= max then
            self._mashDone = true
            _G.chain_retract_veto = nil
            print("[BreakableDoor] Mash FINAL — firing OnBehaviourMashFinal")
            pcall(function() self:OnBehaviourMashFinal() end)
        end
    end,

    _clearState = function(self)
        self._isHooked       = false
        self._hitFired       = false
        self._actionFired    = false
        self._mashProgress   = 0
        self._mashDone       = false
        self._mashCounted    = false
        self._detachDisarmed = false
        self._endpointPos    = nil
        self._endpointPrev   = nil
        self._currentT       = 0
        if _G.chain_retract_veto ~= nil then _G.chain_retract_veto = nil end
    end,

    -- =========================================================================
    -- CLEANUP
    -- =========================================================================

    OnDisable = function(self)
        self:_clearState()
        local key = self._doorName or tostring(self._entityId)
        if _G.BreakableDoorRegistry then _G.BreakableDoorRegistry[key] = nil end
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._subMoved       then pcall(function() _G.event_bus.unsubscribe(self._subMoved)       end) end
            if self._subRetracted   then pcall(function() _G.event_bus.unsubscribe(self._subRetracted)   end) end
            if self._subPullAttempt then pcall(function() _G.event_bus.unsubscribe(self._subPullAttempt) end) end
            if self._subDetached    then pcall(function() _G.event_bus.unsubscribe(self._subDetached)    end) end
        end
    end,

    -- =========================================================================
    -- UPDATE — continuously pin this door to its current lerp state
    -- Runs every frame until the mash is complete so that physics cannot drift
    -- the door away from where the transform says it should be.
    -- =========================================================================

    Update = function(self, dt)
        --if _G.BreakableDoorsBroken then return end   -- physics owns it after break
        --if not self._doorName      then return end
        local states = DOOR_STATES[self._doorName]
        --if not states then return end
        local s0 = states[0]
        local s1 = states[1]
        --if not s0 or not s1 then return end
        local tr = self._transform
        --if not tr then return end

        local t       = self._currentT or 0
        local zShift  = tonumber(self.MashZShift) or 0.05

        local px = s0.pos[1] + (s1.pos[1] - s0.pos[1]) * t
        local py = s0.pos[2] + (s1.pos[2] - s0.pos[2]) * t
        local pz = s0.pos[3] + (s1.pos[3] - s0.pos[3]) * t + zShift * t
        self:_writePos(tr, px, py, pz)

        local qw, qx, qy, qz = quatSlerp(
            s0.quat.w, s0.quat.x, s0.quat.y, s0.quat.z,
            s1.quat.w, s1.quat.x, s1.quat.y, s1.quat.z,
            t)
        self:_writeRot(tr, qw, qx, qy, qz)
    end,

    -- =========================================================================
    -- BEHAVIOUR CALLBACKS
    -- =========================================================================

    -- [Mash] Fires once per tap before the final.
    -- Records the new lerp fraction on this instance; Update picks it up next frame
    -- and applies it to THIS door's transform continuously.
    OnBehaviourMash = function(self, progress, maxCount)
        print(string.format("[BreakableDoor] Mash progress %d/%d — rotating doors", progress, maxCount))
        self._currentT = progress / math.max(1, maxCount)
    end,

    -- [Mash] Fires on the final mash tap — both doors transpose to state 1,
    -- an impulse is applied to each rigidbody, and the virtual door is deactivated.
    OnBehaviourMashFinal = function(self)
        print("[BreakableDoor] MASH FINAL — breaking doors open!")

        -- Guard: only one instance should run the transpose
        if _G.BreakableDoorsBroken then
            print("[BreakableDoor] Already broken — skipping duplicate transpose")
            return
        end
        _G.BreakableDoorsBroken = true

        -- Snap both doors to their fully open positions + rotations
        self:_transposeDoors(1)

        -- Apply impulse to each registered door rigidbody (no motionID change)
        local registry = _G.BreakableDoorRegistry or {}
        for doorName, inst in pairs(registry) do
            local rb = inst._rigidbody
            if rb then
                pcall(function()
                    rb:AddImpulse(0, 0, -9990)
                    print(string.format("[BreakableDoor] '%s' → impulse applied", doorName))
                end)
            else
                print(string.format("[BreakableDoor] '%s' WARNING: no rigidbody for impulse", doorName))
            end
        end

        -- Deactivate the static virtual door via its ActiveComponent.
        -- The virtual door does NOT carry this script, so we look it up purely
        -- through the engine's entity/component APIs.
       -- Deactivate the static virtual door via its ActiveComponent.
        local virtualName = tostring(self.VirtualDoorName or ""):match("^%s*(.-)%s*$")
        if virtualName and virtualName ~= "" then
            pcall(function()
                -- ── Step 1: resolve the virtual door entity ───────────────────
                local virtualEnt = Engine.GetEntityByName(virtualName)
                
                if not virtualEnt then
                    print(string.format("[BreakableDoor] WARNING: virtual door entity '%s' not found", virtualName))
                    return
                end

                -- ── Step 2 & 3: get ActiveComponent and set inactive ──────────
                -- Using the global GetComponent just like DoorTrigger.lua does
                local ac = GetComponent(virtualEnt, "ActiveComponent")
                
                if ac then
                    ac.isActive = false
                    print(string.format("[BreakableDoor] Virtual door '%s' → ActiveComponent.isActive = false", virtualName))
                else
                    print(string.format("[BreakableDoor] WARNING: ActiveComponent not found on virtual door '%s'", virtualName))
                end
            end)
        else
            print("[BreakableDoor] VirtualDoorName not set — skipping virtual door deactivation")
        end

        -- TODO: play door-break sound / spawn debris particles here
        -- TODO: disable colliders post-break if needed:
        --   Engine.SetColliderEnabled(doorTransform, false)
    end,
}