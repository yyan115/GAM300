-- BreakableDoorInteractable.lua
-- =============================================================================
-- BREAKABLE DOOR — Chain mash interactable
-- =============================================================================
-- Attach to the STATIC door entity (active at start).
-- The DYNAMIC rigidbody door is a SEPARATE entity, inactive at start.
--
-- Each mash tap rotates and translates the static door incrementally,
-- relative to its starting transform (no hardcoded positions).
--
-- Final mash:
--   1. Deactivates this (static) door.
--   2. Activates the dynamic door.
--   3. Applies AddImpulse + AddTorque to the dynamic door's rigidbody.
--
-- LinkedDoorName (optional): name of another door entity with this same script.
-- Both doors animate together on every mash and break together on final.
-- Works standalone (leave LinkedDoorName blank) or as a dual-door pair.
-- =============================================================================

local Component = require("extension.mono_helper")

-- =============================================================================
-- HELPERS
-- =============================================================================

-- Quaternion multiply: (a * b), returns w, x, y, z
local function quatMul(aw, ax, ay, az, bw, bx, by, bz)
    return
        aw*bw - ax*bx - ay*by - az*bz,
        aw*bx + ax*bw + ay*bz - az*by,
        aw*by - ax*bz + ay*bw + az*bx,
        aw*bz + ax*by - ay*bx + az*bw
end

-- Y-axis rotation quaternion from degrees: returns w, x, y, z
local function quatRotY(degrees)
    local half = math.rad(degrees) * 0.5
    return math.cos(half), 0, math.sin(half), 0
end

-- =============================================================================
-- COMPONENT
-- =============================================================================

return Component {

    -- =========================================================================
    -- INSPECTOR FIELDS
    -- All rotation is Euler Y degrees (no quaternions).
    -- All translation is world-space units per mash step.
    -- =========================================================================
    fields = {
        -- ── Identity ──────────────────────────────────────────────────────
        -- REQUIRED: set to the entity name of THIS static door (e.g. BreakableDoor1).
        -- Used as the registry key so LinkedDoorName lookups work correctly.
        DoorName            = "",

        -- ── Chain interaction ─────────────────────────────────────────────
        HitRadius           = 1.0,
        MashCount           = 3,        -- total mash taps required to break

        -- ── Per-mash-step transform (cumulative, relative to starting transform)
        -- Rotation: Y degrees per step. Positive = rotate right, negative = left.
        MashRotateYDeg      = 6.0,
        -- Translation: world units per step along each axis.
        MashTranslateX      = 0.0,
        MashTranslateY      = 0.0,
        MashTranslateZ      = -0.02,    -- nudge forward each mash

        -- ── Dynamic door ──────────────────────────────────────────────────
        -- Entity name of the INACTIVE rigidbody door that gets activated on
        -- final mash and receives the impulse / torque.
        DynamicDoorName     = "",

        -- Impulse applied to the dynamic door's rigidbody (world space).
        ImpulseX            = 0.0,
        ImpulseY            = 0.0,
        ImpulseZ            = -500.0,

        -- Torque applied to the dynamic door's rigidbody.
        TorqueX             = 0.0,
        TorqueY             = 200.0,
        TorqueZ             = 0.0,

        -- ── Linked door (optional) ────────────────────────────────────────
        -- Entity name of another door that uses this same script.
        -- It will animate in sync and break at the same time.
        -- Leave blank for a standalone door.
        LinkedDoorName      = "",

        -- ── Camera effects ────────────────────────────────────────────────
        MashShakeIntensity      = 0.25,
        MashShakeDuration       = 0.20,
        MashChromaticIntensity  = 0.30,
        MashChromaticDuration   = 0.25,
        FinalShakeIntensity     = 0.70,
        FinalShakeDuration      = 0.45,
        FinalChromaticIntensity = 0.75,
        FinalChromaticDuration  = 0.50,
    },

    -- =========================================================================
    -- LIFECYCLE
    -- =========================================================================

    Start = function(self)
        -- ── Resolve entity ID ─────────────────────────────────────────────
        self._entityId = nil
        pcall(function()
            if      self.GetEntityId then self._entityId = self:GetEntityId()
            elseif  self.GetEntity   then self._entityId = self:GetEntity()
            elseif  self.entityId    then self._entityId = self.entityId
            elseif  type(self.entity) == "number" then self._entityId = self.entity
            end
        end)

        -- ── Resolve entity name ───────────────────────────────────────────
        -- Priority 1: explicit DoorName field (strips whitespace and stray quotes)
        local function cleanName(v)
            return tostring(v or ""):gsub('["\']', ''):match("^%s*(.-)%s*$")
        end
        local configuredName = cleanName(self.DoorName)
        if configuredName ~= "" then
            self._doorName = configuredName
        else
            pcall(function()
                if      self.GetName    then self._doorName = self:GetName()
                elseif  self.name       then self._doorName = self.name
                elseif  self.gameObject and self.gameObject.name then
                    self._doorName = self.gameObject.name
                end
            end)
        end
        if not self._doorName or self._doorName == "" then
            self._doorName = tostring(self._entityId or "unknown_door")
            print("[BreakableDoor] WARNING: DoorName not set — set it in the inspector or registry lookups will fail!")
        end
        -- Expose cleanName for use below
        self._cleanName = cleanName

        -- ── Resolve transform ─────────────────────────────────────────────
        self._transform = nil
        if self._entityId and Engine then
            pcall(function()
                if      Engine.FindTransformByID     then self._transform = Engine.FindTransformByID(self._entityId)
                elseif  Engine.FindTransformByEntity then self._transform = Engine.FindTransformByEntity(self._entityId)
                elseif  Engine.GetTransformForEntity then self._transform = Engine.GetTransformForEntity(self._entityId)
                end
            end)
        end

        -- ── Snapshot base position and rotation ──────────────────────────
        -- All mash-step transforms are computed as offsets from these values,
        -- so placement in the scene just works without any field setup.
        self._basePos  = { x = 0, y = 0, z = 0 }
        self._baseQuat = { w = 1, x = 0, y = 0, z = 0 }
        if self._transform then
            pcall(function()
                -- Position
                if Engine and Engine.GetTransformWorldPosition then
                    local p = Engine.GetTransformWorldPosition(self._transform)
                    if p then
                        self._basePos = {
                            x = p[1] or p.x or 0,
                            y = p[2] or p.y or 0,
                            z = p[3] or p.z or 0,
                        }
                    end
                elseif self._transform.localPosition then
                    local p = self._transform.localPosition
                    self._basePos = { x = p.x or 0, y = p.y or 0, z = p.z or 0 }
                end
                -- Rotation
                local rot = self._transform.localRotation
                if type(rot) == "table" or type(rot) == "userdata" then
                    self._baseQuat = {
                        w = rot.w or 1,
                        x = rot.x or 0,
                        y = rot.y or 0,
                        z = rot.z or 0,
                    }
                end
            end)
        end
        print(string.format(
            "[BreakableDoor] '%s' base: pos=(%.3f,%.3f,%.3f) quat=(w=%.4f x=%.4f y=%.4f z=%.4f)",
            self._doorName,
            self._basePos.x,  self._basePos.y,  self._basePos.z,
            self._baseQuat.w, self._baseQuat.x, self._baseQuat.y, self._baseQuat.z))

        -- ── Global registry + per-group broken flag ───────────────────────
        -- Each door (or linked pair) gets its own broken state so that unrelated
        -- standalone doors are not blocked when another group finishes.
        -- Group key: sorted "DoorA|DoorB" for a pair, or just "DoorName" standalone.
        if not _G.BreakableDoorRegistry     then _G.BreakableDoorRegistry     = {} end
        if not _G.BreakableDoorGroupBroken  then _G.BreakableDoorGroupBroken  = {} end
        _G.BreakableDoorRegistry[self._doorName] = self

        local linkedRaw  = self._cleanName(self.LinkedDoorName)
        if linkedRaw ~= "" then
            local a, b = self._doorName, linkedRaw
            if b < a then a, b = b, a end          -- sort so both sides get the same key
            self._groupKey = a .. "|" .. b
        else
            self._groupKey = self._doorName
        end
        -- Initialise this group's broken state only if not already set.
        if _G.BreakableDoorGroupBroken[self._groupKey] == nil then
            _G.BreakableDoorGroupBroken[self._groupKey] = false
        end

        -- ── Deactivate dynamic door at start ──────────────────────────────
        local dynName = self._cleanName(self.DynamicDoorName)
        if dynName ~= "" then
            pcall(function()
                local dynEnt = Engine.GetEntityByName(dynName)
                if dynEnt then
                    local ac = GetComponent(dynEnt, "ActiveComponent")
                    if ac then
                        ac.isActive = false
                        print(string.format("[BreakableDoor] Dynamic door '%s' → set inactive at start", dynName))
                    end
                else
                    print(string.format("[BreakableDoor] WARNING: dynamic door '%s' not found at start", dynName))
                end
            end)
        end

        -- ── Per-throw state ───────────────────────────────────────────────
        self._isHooked       = false
        self._hitFired       = false
        self._mashProgress   = 0
        self._mashDone       = false
        self._detachDisarmed = false
        self._endpointPos    = nil
        self._endpointPrev   = nil
        self._currentStep    = 0   -- 0 = closed; counts up with each mash tap

        -- ── Subscribe to chain events ─────────────────────────────────────
        if not (_G.event_bus and _G.event_bus.subscribe) then
            print("[BreakableDoor] WARNING: event_bus not available — interactions will not fire")
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

        print(string.format(
            "[BreakableDoor] Ready — '%s' | mashes=%d | rotY=%.1f°/step | tz=%.3f/step | dynamic='%s' | linked='%s'",
            self._doorName,
            tonumber(self.MashCount)       or 1,
            tonumber(self.MashRotateYDeg)  or 0,
            tonumber(self.MashTranslateZ)  or 0,
            tostring(self.DynamicDoorName or ""),
            tostring(self.LinkedDoorName  or "")))
    end,

    -- =========================================================================
    -- UPDATE — re-pin static door to its current mash-step transform every frame.
    -- Prevents physics from drifting it between mash events.
    -- Stops running once the door is broken (static is inactive by then anyway).
    -- =========================================================================

    Update = function(self, dt)
        -- Deferred impulse: dynamic door was activated last frame, apply forces now.
        -- Impulse fired same frame as activation won't move the body (physics not init yet).
        if self._pendingImpulse then
            local p = self._pendingImpulse
            self._pendingImpulse = nil
            print(string.format("[BreakableDoor] '%s': _pendingImpulse firing for dynName='%s'", self._doorName, tostring(p.dynName)))
            pcall(function()
                local dynEnt = Engine.GetEntityByName(p.dynName)
                print(string.format("[BreakableDoor] '%s': pendingImpulse GetEntityByName('%s') = %s", self._doorName, p.dynName, tostring(dynEnt)))
                if not dynEnt then return end

                local rb = GetComponent(dynEnt, "RigidBodyComponent")
                print(string.format("[BreakableDoor] '%s': RigidBodyComponent = %s", self._doorName, tostring(rb)))
                if not rb then return end

                print(string.format("[BreakableDoor] '%s': calling AddImpulse(%.1f, %.1f, %.1f)", self._doorName, p.ix, p.iy, p.iz))
                rb:AddImpulse(p.ix, p.iy, p.iz)
                print(string.format("[BreakableDoor] '%s': AddImpulse done", self._doorName))

                if p.torx ~= 0 or p.tory ~= 0 or p.torz ~= 0 then
                    print(string.format("[BreakableDoor] '%s': calling AddTorque(%.1f, %.1f, %.1f)", self._doorName, p.torx, p.tory, p.torz))
                    rb:AddTorque(p.torx, p.tory, p.torz)
                    print(string.format("[BreakableDoor] '%s': AddTorque done", self._doorName))
                end
            end)
            pcall(function()
                if self._entityId then
                    local ac = GetComponent(self._entityId, "ActiveComponent")
                    print(string.format("[BreakableDoor] '%s': deactivating static, ActiveComponent = %s", self._doorName, tostring(ac)))
                    if ac then
                        ac.isActive = false
                        print(string.format("[BreakableDoor] '%s': static door deactivated", self._doorName))
                    end
                end
            end)
            return
        end

        if self._mashDone        then return end
        if self._currentStep == 0 then return end   -- at rest, no need to force
        self:_applyStepTransform(self._currentStep)
    end,

    -- =========================================================================
    -- CLEANUP
    -- =========================================================================

    OnDisable = function(self)
        self._isHooked     = false
        self._hitFired     = false
        self._mashProgress = 0
        self._mashDone     = false
        self._currentStep  = 0
        self._endpointPos  = nil
        self._endpointPrev = nil
        if _G.chain_retract_veto ~= nil then _G.chain_retract_veto = nil end
        if _G.BreakableDoorRegistry then
            _G.BreakableDoorRegistry[self._doorName] = nil
        end
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._subMoved       then pcall(function() _G.event_bus.unsubscribe(self._subMoved)       end) end
            if self._subRetracted   then pcall(function() _G.event_bus.unsubscribe(self._subRetracted)   end) end
            if self._subPullAttempt then pcall(function() _G.event_bus.unsubscribe(self._subPullAttempt) end) end
            if self._subDetached    then pcall(function() _G.event_bus.unsubscribe(self._subDetached)    end) end
        end
    end,

    -- =========================================================================
    -- INTERNAL — compute and write the transform for a given mash step index.
    -- step 0 = base (closed), step N = N increments applied.
    -- Only writes to THIS door's transform; does NOT notify linked door.
    -- =========================================================================

    _applyStepTransform = function(self, step)
        local tr = self._transform
        if not tr then return end

        local maxStep = math.max(1, tonumber(self.MashCount) or 1)
        step = math.max(0, math.min(step, maxStep))

        -- ── Translation: base + (step * per-step delta) ───────────────────
        local tx = tonumber(self.MashTranslateX) or 0
        local ty = tonumber(self.MashTranslateY) or 0
        local tz = tonumber(self.MashTranslateZ) or 0
        local px = self._basePos.x + tx * step
        local py = self._basePos.y + ty * step
        local pz = self._basePos.z + tz * step
        self:_writePos(tr, px, py, pz)

        -- ── Rotation: base * rotY(step * MashRotateYDeg) ─────────────────
        local totalDeg = (tonumber(self.MashRotateYDeg) or 0) * step
        local dw, dx, dy, dz = quatRotY(totalDeg)
        local bq = self._baseQuat
        local rw, rx, ry, rz = quatMul(bq.w, bq.x, bq.y, bq.z, dw, dx, dy, dz)
        self:_writeRot(tr, rw, rx, ry, rz)
    end,

    -- =========================================================================
    -- INTERNAL — apply a mash step to BOTH this door and the linked door.
    -- Linked door gets its transform updated directly (no re-propagation, no
    -- recursion risk).
    -- =========================================================================

    _applyMashStep = function(self, step)
        self._currentStep = step
        self:_applyStepTransform(step)

        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("door_mash_sound", { doorName = self._doorName })
            _G.event_bus.publish("camera_shake", {
                intensity = tonumber(self.MashShakeIntensity)     or 0.25,
                duration  = tonumber(self.MashShakeDuration)      or 0.20,
                frequency = 25.0,
            })
            _G.event_bus.publish("fx_chromatic", {
                intensity = tonumber(self.MashChromaticIntensity) or 0.30,
                duration  = tonumber(self.MashChromaticDuration)  or 0.25,
            })
        end

        local linkedName = self._cleanName(self.LinkedDoorName)
        if linkedName ~= "" then
            local linked = _G.BreakableDoorRegistry and _G.BreakableDoorRegistry[linkedName]
            if linked then
                pcall(function()
                    linked._currentStep = step
                    linked:_applyStepTransform(step)
                end)
            else
                print(string.format("[BreakableDoor] WARNING: linked door '%s' not in registry yet", linkedName))
            end
        end
    end,

    -- =========================================================================
    -- INTERNAL — final mash: deactivate static, activate dynamic + impulse/torque.
    -- Only executes for THIS door. Called individually on self and linked.
    -- =========================================================================

    _breakDoor = function(self)
        self._mashDone    = true
        self._currentStep = 0

        local dynName = self._cleanName(self.DynamicDoorName)
        print(string.format("[BreakableDoor] _breakDoor called on '%s', dynName='%s'", self._doorName, dynName))

        if dynName == "" then
            print(string.format("[BreakableDoor] '%s': DynamicDoorName not set -- skipping", self._doorName))
            pcall(function()
                if self._entityId then
                    local ac = GetComponent(self._entityId, "ActiveComponent")
                    if ac then ac.isActive = false end
                end
            end)
            return
        end

        pcall(function()
            local dynEnt = Engine.GetEntityByName(dynName)
            print(string.format("[BreakableDoor] '%s': GetEntityByName('%s') = %s", self._doorName, dynName, tostring(dynEnt)))
            if not dynEnt then return end

            local dynTr = nil
            if Engine then
                if      Engine.FindTransformByID     then dynTr = Engine.FindTransformByID(dynEnt)
                elseif  Engine.FindTransformByEntity then dynTr = Engine.FindTransformByEntity(dynEnt)
                elseif  Engine.GetTransformForEntity then dynTr = Engine.GetTransformForEntity(dynEnt)
                end
            end
            print(string.format("[BreakableDoor] '%s': dynTr = %s", self._doorName, tostring(dynTr)))

            if dynTr and self._transform then
                local px, py, pz = self:_getWorldPos()
                print(string.format("[BreakableDoor] '%s': static world pos = (%s, %s, %s)", self._doorName, tostring(px), tostring(py), tostring(pz)))
                if px then self:_writePos(dynTr, px, py, pz) end
                local rot = self._transform.localRotation
                print(string.format("[BreakableDoor] '%s': static localRotation = %s", self._doorName, tostring(rot)))
                if rot and (type(rot) == "table" or type(rot) == "userdata") then
                    self:_writeRot(dynTr, rot.w or 1, rot.x or 0, rot.y or 0, rot.z or 0)
                end
            end

            local ac = GetComponent(dynEnt, "ActiveComponent")
            print(string.format("[BreakableDoor] '%s': ActiveComponent = %s", self._doorName, tostring(ac)))
            if ac then
                ac.isActive = true
                print(string.format("[BreakableDoor] '%s': dynamic door activated", self._doorName))
            end
        end)

        self._pendingImpulse = {
            dynName = dynName,
            ix   = tonumber(self.ImpulseX) or 0,
            iy   = tonumber(self.ImpulseY) or 0,
            iz   = tonumber(self.ImpulseZ) or 0,
            torx = tonumber(self.TorqueX)  or 0,
            tory = tonumber(self.TorqueY)  or 0,
            torz = tonumber(self.TorqueZ)  or 0,
        }
        print(string.format("[BreakableDoor] '%s': _pendingImpulse queued: ix=%.1f iy=%.1f iz=%.1f torx=%.1f tory=%.1f torz=%.1f",
            self._doorName, self._pendingImpulse.ix, self._pendingImpulse.iy, self._pendingImpulse.iz,
            self._pendingImpulse.torx, self._pendingImpulse.tory, self._pendingImpulse.torz))
    end,

    -- =========================================================================
    -- INTERNAL — drive the full final-mash sequence for self + linked door.
    -- Guarded by _G.BreakableDoorGroupBroken so only one instance runs it once per group.
    -- =========================================================================

    _doFinalMash = function(self)
        if _G.BreakableDoorGroupBroken[self._groupKey] then
            print("[BreakableDoor] Already broken — skipping duplicate final mash")
            return
        end
        _G.BreakableDoorGroupBroken[self._groupKey] = true
        _G.chain_retract_veto   = nil
        print("[BreakableDoor] FINAL MASH — breaking doors!")

        if _G.event_bus and _G.event_bus.publish then
            _G.event_bus.publish("door_final_mash_sound", { doorName = self._doorName })
            _G.event_bus.publish("camera_shake", {
                intensity = tonumber(self.FinalShakeIntensity)     or 0.70,
                duration  = tonumber(self.FinalShakeDuration)      or 0.45,
                frequency = 20.0,
            })
            _G.event_bus.publish("fx_chromatic", {
                intensity = tonumber(self.FinalChromaticIntensity) or 0.75,
                duration  = tonumber(self.FinalChromaticDuration)  or 0.50,
            })
        end

        -- Trigger chain retraction now that the veto is cleared
        if _G.event_bus and _G.event_bus.publish then
            pcall(function() _G.event_bus.publish("chain.retract", {}) end)
        end

        -- Break linked door first (so both go inactive in the same frame)
        local linkedName = self._cleanName(self.LinkedDoorName)
        if linkedName ~= "" then
            local linked = _G.BreakableDoorRegistry and _G.BreakableDoorRegistry[linkedName]
            if linked then
                pcall(function() linked:_breakDoor() end)
            else
                print(string.format("[BreakableDoor] WARNING: linked door '%s' not in registry", linkedName))
            end
        end

        -- Break self
        self:_breakDoor()
    end,

    -- =========================================================================
    -- INTERNAL — chain endpoint moved (segment-sweep hit detection)
    -- =========================================================================

    _onEndpointMoved = function(self, payload)
        if _G.BreakableDoorGroupBroken[self._groupKey] then return end
        if self._mashDone          then return end

        -- Track previous endpoint position for segment-sweep
        if self._endpointPos then
            self._endpointPrev = { self._endpointPos.x, self._endpointPos.y, self._endpointPos.z }
        end
        if payload.position then self._endpointPos = payload.position end

        if self._hitFired       then return end
        if self._detachDisarmed then return end
        if payload.isFlopping   then return end
        if not self._endpointPos then return end

        local ox, oy, oz = self:_getWorldPos()
        if not ox then return end

        local ex = self._endpointPos.x
        local ey = self._endpointPos.y
        local ez = self._endpointPos.z
        local px = self._endpointPrev and self._endpointPrev[1] or ex
        local py = self._endpointPrev and self._endpointPrev[2] or ey
        local pz = self._endpointPrev and self._endpointPrev[3] or ez

        local radius   = math.max(0.01, tonumber(self.HitRadius) or 1.0)
        local radiusSq = radius * radius

        -- Closest point on the swept segment to this door's origin
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
        if ddx*ddx + ddy*ddy + ddz*ddz > radiusSq then return end

        -- HIT confirmed
        self._hitFired     = true
        self._isHooked     = true
        self._mashProgress = 0
        self._mashDone     = false
        _G.chain_retract_veto = function()
            return self._isHooked and not self._mashDone
        end
        print(string.format("[BreakableDoor] HIT on '%s' — waiting for mash", self._doorName))
    end,

    -- =========================================================================
    -- INTERNAL — pull attempt (one mash tap)
    -- =========================================================================

    _onPullAttempt = function(self, payload)
        if not self._isHooked then return end
        if self._mashDone     then return end

        self._mashProgress = self._mashProgress + 1
        local max = math.max(1, tonumber(self.MashCount) or 1)
        print(string.format("[BreakableDoor] Mash %d/%d on '%s'", self._mashProgress, max, self._doorName))

        if self._mashProgress >= max then
            self:_doFinalMash()
        else
            self:_applyMashStep(self._mashProgress)
        end
    end,

    -- =========================================================================
    -- INTERNAL — chain fully retracted
    -- =========================================================================

    _onChainRetracted = function(self, payload)
        if self._isHooked and not self._mashDone then
            -- Re-arm: player retracted before finishing the mash
            print(string.format("[BreakableDoor] '%s' retracted at %d/%d — re-arming",
                self._doorName, self._mashProgress, math.max(1, tonumber(self.MashCount) or 1)))
            self._hitFired       = false
            self._detachDisarmed = false
            -- NOTE: _currentStep is intentionally NOT reset here.
            -- The door stays at whatever rotation it reached.
            return
        end
        -- Full clear (not engaged, or already done)
        self._isHooked       = false
        self._hitFired       = false
        self._mashProgress   = 0
        self._detachDisarmed = false
        self._endpointPos    = nil
        self._endpointPrev   = nil
        if _G.chain_retract_veto ~= nil then _G.chain_retract_veto = nil end
    end,

    -- =========================================================================
    -- INTERNAL — chain detached mid-throw
    -- =========================================================================

    _onDetach = function(self)
        print(string.format("[BreakableDoor] '%s' detached — disarming until retract", self._doorName))
        self._isHooked       = false
        self._detachDisarmed = true
        self._endpointPos    = nil
        self._endpointPrev   = nil
        if _G.chain_retract_veto ~= nil then _G.chain_retract_veto = nil end
    end,

    -- =========================================================================
    -- INTERNAL — world position read
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
            local p = tr.localPosition
            return p.x or p[1] or 0, p.y or p[2] or 0, p.z or p[3] or 0
        end
        return nil
    end,

    -- =========================================================================
    -- INTERNAL — write position
    -- =========================================================================

    _writePos = function(self, tr, x, y, z)
        if not tr then return end
        if type(tr.SetPosition) == "function" then
            pcall(function() tr:SetPosition(x, y, z) end)
            return
        end
        pcall(function()
            local pos = tr.localPosition
            if type(pos) == "table" or type(pos) == "userdata" then
                pos.x, pos.y, pos.z = x, y, z
                tr.isDirty = true
            end
        end)
    end,

    -- =========================================================================
    -- INTERNAL — write quaternion rotation
    -- =========================================================================

    _writeRot = function(self, tr, w, x, y, z)
        if not tr then return end
        pcall(function()
            local rot = tr.localRotation
            if type(rot) == "table" or type(rot) == "userdata" then
                rot.w, rot.x, rot.y, rot.z = w, x, y, z
                tr.isDirty = true
            end
        end)
    end,
}