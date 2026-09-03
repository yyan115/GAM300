-- LockOnPoint.lua
-- Attach to the LockOn child entity on an enemy (one tier below root),
-- OR to the root of a throwable object (tag: "Throwable").
--
-- ENEMY/BOSS mode (tag == "Enemy" or "Boss" on root):
--   Existing behaviour preserved - sweeps against PartEntityIds body parts,
--   publishes "chain.lockon_sweep_hit" with closest part pos so
--   ChainEndpointController snaps to the nearest bone.
--
-- THROWABLE mode (tag == "Throwable" on root):
--   PartEntityIds is ignored. The sweep checks only the root entity's own
--   transform. This acts as a fast-endpoint backup for the OnTriggerEnter
--   that fires when the chain collider overlaps the throwable's static body.
--   Publishes the same "chain.lockon_sweep_hit" so ChainEndpointController
--   handles it identically — no snap to a part, just root position.

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        -- Body part entity names, populated in the editor.
        PartEntityNames = {},

        -- Radius within which the endpoint is considered to have hit a part/root.
        -- Should match or slightly exceed the endpoint trigger collider radius.
        SweepRadius = 0.3,
    },

    Start = function(self)
        -- Keep dense, parallel arrays: the sweep iterates these every active frame.
        self.PartEntityIds = {}
        self._partTransforms = {}
        local parentId = Engine.GetParentEntity(self.entityId)

        -- Loop through all the names provided by the editor
        for _, partName in pairs(self.PartEntityNames) do
            
            -- Skip any empty strings just in case the editor left a blank entry
            if partName and partName ~= "" then
                
                -- Search the hierarchy starting from parentId
                --print("Calling Engine.FindChildByName")
                local childId = Engine.FindChildByName(parentId, partName)
                
                if childId ~= -1 then
                    local index = #self.PartEntityIds + 1
                    self.PartEntityIds[index] = childId
                    self._partTransforms[index] = Engine.FindTransformByID(childId)
                    --print("[LockOnPoint] Found body part: " .. partName .. " (ID: " .. childId .. ")")
                else
                    --print("[LockOnPoint] WARNING: Could not find child named: " .. partName)
                end
            end
        end

        -- Resolve our own entity ID so we can pass it in the sweep hit event.
        self._entityId = nil
        pcall(function() if self.GetEntityId then self._entityId = self:GetEntityId() end end)
        if not self._entityId then
            pcall(function() if self.GetEntity then self._entityId = self:GetEntity() end end)
        end
        if not self._entityId and self.entityId then self._entityId = self.entityId end

        -- Walk up to root and detect mode via tag.
        self._rootEntityId   = self._entityId
        self._isThrowable    = false
        self._isEnemy        = false
        self._isInteractable = false

        if Engine and Engine.GetParentEntity and self._entityId then
            local current = self._entityId
            local depth   = 0
            while true do
                depth = depth + 1
                if depth > 32 then break end
                local parentId = Engine.GetParentEntity(current)
                if not parentId or parentId < 0 then break end
                current = parentId
            end
            self._rootEntityId = current
        end

        if Engine and Engine.GetEntityTag and self._rootEntityId then
            local ok, tag = pcall(function() return Engine.GetEntityTag(self._rootEntityId) end)
            if ok and tag then
                if tag == "Throwable" then
                    self._isThrowable = true
                elseif tag == "Enemy" or tag == "Boss" then
                    self._isEnemy = true
                elseif tag == "Interactable" then
                    self._isInteractable = true
                end
            end
        end

        self._rootTransform = self._rootEntityId and Engine.FindTransformByID(self._rootEntityId) or nil
        local sweepRadius = tonumber(self.SweepRadius) or 0.3
        self._sweepRadiusSq = sweepRadius * sweepRadius
        self._closestPartPos = {x=0, y=0, z=0}

        self._endpointPos  = nil
        self._endpointPrev = nil
        self._endpointPosScratch = {x=0, y=0, z=0}
        self._endpointPrevScratch = {0, 0, 0}
        self._sweepActive  = false
        self._sweepFired   = false

        if _G.event_bus and _G.event_bus.subscribe then
            self._subMoved = _G.event_bus.subscribe("chain.endpoint_moved", function(payload)
                if not payload then return end
                if self._endpointPos then
                    local previous = self._endpointPrevScratch
                    previous[1], previous[2], previous[3] =
                        self._endpointPos.x, self._endpointPos.y, self._endpointPos.z
                    self._endpointPrev = previous
                end
                if payload.position then
                    local position = self._endpointPosScratch
                    position.x, position.y, position.z =
                        payload.position.x, payload.position.y, payload.position.z
                    self._endpointPos = position
                end
                self._sweepActive = payload.isExtending or false
                if self._sweepActive and not self._sweepFired then
                    self:_Sweep()
                end
            end)

            self._subRetracted = _G.event_bus.subscribe("chain.endpoint_retracted", function(_)
                self._sweepActive  = false
                self._sweepFired   = false
                self._endpointPos  = nil
                self._endpointPrev = nil
            end)

            -- Suppress sweep once a collision is already confirmed.
            self._subHit = _G.event_bus.subscribe("chain.endpoint_hit_entity", function(_)
                self._sweepFired = true
            end)
        end
    end,

    -- Returns world position {x,y,z}, entity ID, and distance of the closest
    -- body part (Enemy mode) or own root (Throwable mode) to the given point.
    -- Returns nil if nothing is available.
    GetClosestPart = function(self, px, py, pz)
        -- ── Throwable mode: return root position directly ─────────────────
        if self._isThrowable then
            if not (Engine and Engine.FindTransformByID and Engine.GetTransformWorldPositionXYZ) then return nil end
            if not self._rootEntityId then return nil end
            local transform = self._rootTransform
            if not transform then return nil end
            local ex, ey, ez = Engine.GetTransformWorldPositionXYZ(transform)
            local dx = px - ex
            local dy = py - ey
            local dz = pz - ez
            local bestPos = self._closestPartPos
            bestPos.x, bestPos.y, bestPos.z = ex, ey, ez
            return bestPos, self._rootEntityId, math.sqrt(dx*dx + dy*dy + dz*dz)
        end

        -- ── Enemy mode: iterate PartEntityIds ─────────────────────────────
        local ids = self.PartEntityIds
        if not ids or #ids == 0 then return nil end
        if not (Engine and Engine.FindTransformByID and Engine.GetTransformWorldPositionXYZ) then return nil end

        local bestDistSq = math.huge
        local bestId     = nil
        local bestX, bestY, bestZ
        local transforms = self._partTransforms

        for i = 1, #ids do
            local id = ids[i]
            if id and id ~= 0 then
                local transform = transforms[i]
                if transform then
                    local ex, ey, ez = Engine.GetTransformWorldPositionXYZ(transform)
                    local dx = px - ex
                    local dy = py - ey
                    local dz = pz - ez
                    local distSq = dx*dx + dy*dy + dz*dz
                    if distSq < bestDistSq then
                        bestDistSq = distSq
                        bestX, bestY, bestZ = ex, ey, ez
                        bestId     = id
                    end
                end
            end
        end

        if not bestId then return nil end
        local bestPos = self._closestPartPos
        bestPos.x, bestPos.y, bestPos.z = bestX, bestY, bestZ
        return bestPos, bestId, math.sqrt(bestDistSq)
    end,

    -- Endpoint movement is already event-driven. Running the sweep from that
    -- event avoids invoking 18 idle Lua Update callbacks on every frame.
    _Sweep = function(self)
        if not self._sweepActive or self._sweepFired then return end
        if not self._endpointPos then return end
        if not (Engine and Engine.FindTransformByID and Engine.GetTransformWorldPositionXYZ) then return end

        local ex = self._endpointPos.x
        local ey = self._endpointPos.y
        local ez = self._endpointPos.z

        local px = self._endpointPrev and self._endpointPrev[1] or ex
        local py = self._endpointPrev and self._endpointPrev[2] or ey
        local pz = self._endpointPrev and self._endpointPrev[3] or ez

        local sweepRadSq = self._sweepRadiusSq

        local segDX, segDY, segDZ = ex - px, ey - py, ez - pz
        local segLenSq = segDX*segDX + segDY*segDY + segDZ*segDZ

        -- ── Throwable / Interactable mode: single root-entity check ───────────────────────
        if self._isThrowable or self._isInteractable then
            if not self._rootEntityId then return end
            repeat
                local transform = self._rootTransform
                if not transform then break end

                local partX, partY, partZ =
                    Engine.GetTransformWorldPositionXYZ(transform)

                local closestX, closestY, closestZ
                if segLenSq < 1e-8 then
                    closestX, closestY, closestZ = ex, ey, ez
                else
                    local t = ((partX-px)*segDX + (partY-py)*segDY + (partZ-pz)*segDZ) / segLenSq
                    t = math.max(0, math.min(1, t))
                    closestX = px + segDX * t
                    closestY = py + segDY * t
                    closestZ = pz + segDZ * t
                end

                local ddx = partX - closestX
                local ddy = partY - closestY
                local ddz = partZ - closestZ
                if ddx*ddx + ddy*ddy + ddz*ddz > sweepRadSq then break end

                -- LOS check toward root
                local toPX   = partX - ex
                local toPY   = partY - ey
                local toPZ   = partZ - ez
                local toDist = math.sqrt(toPX*toPX + toPY*toPY + toPZ*toPZ)
                local hasLOS = true
                if Physics and toDist > 1e-4 then
                    local ndx = toPX / toDist
                    local ndy = toPY / toDist
                    local ndz = toPZ / toDist
                    if Physics.RaycastFull then
                        local ok, hit, hitDist, _, _, _, _, _, _, hitBodyId =
                            pcall(Physics.RaycastFull, ex, ey, ez, ndx, ndy, ndz, toDist)
                        if ok and hit and hitDist and hitDist < toDist - 0.05 then
                            hasLOS = (hitBodyId == self._rootEntityId)
                        end
                    elseif Physics.Raycast then
                        local ok, hitDist = pcall(Physics.Raycast, ex, ey, ez, ndx, ndy, ndz, toDist)
                        if ok and hitDist and hitDist > 0 and hitDist < toDist - 0.05 then
                            hasLOS = false
                        end
                    end
                end
                if not hasLOS then break end

                self._sweepFired = true
                if _G.event_bus and _G.event_bus.publish and self._entityId then
                    _G.event_bus.publish("chain.lockon_sweep_hit", {
                        entityId = self._entityId,   -- the LockOnPoint entity (may be root for throwables)
                        partId   = self._rootEntityId,
                        partX    = partX,
                        partY    = partY,
                        partZ    = partZ,
                    })
                end
                return
            until true
            return
        end

        -- ── Enemy mode: iterate PartEntityIds (original logic) ────────────
        local ids = self.PartEntityIds
        if not ids or #ids == 0 then return end
        local transforms = self._partTransforms

        for i = 1, #ids do
            local id = ids[i]
            if not id or id == 0 then
                -- skip unset/empty slots
            else
            repeat
                local transform = transforms[i]
                if not transform then break end

                local partX, partY, partZ =
                    Engine.GetTransformWorldPositionXYZ(transform)

                local closestX, closestY, closestZ
                if segLenSq < 1e-8 then
                    closestX, closestY, closestZ = ex, ey, ez
                else
                    local t = ((partX-px)*segDX + (partY-py)*segDY + (partZ-pz)*segDZ) / segLenSq
                    t = math.max(0, math.min(1, t))
                    closestX = px + segDX * t
                    closestY = py + segDY * t
                    closestZ = pz + segDZ * t
                end

                local ddx = partX - closestX
                local ddy = partY - closestY
                local ddz = partZ - closestZ
                if ddx*ddx + ddy*ddy + ddz*ddz > sweepRadSq then break end

                local toPX   = partX - ex
                local toPY   = partY - ey
                local toPZ   = partZ - ez
                local toDist = math.sqrt(toPX*toPX + toPY*toPY + toPZ*toPZ)
                local hasLOS = true
                if Physics and toDist > 1e-4 then
                    local ndx = toPX / toDist
                    local ndy = toPY / toDist
                    local ndz = toPZ / toDist
                    if Physics.RaycastFull then
                        local ok, hit, hitDist, _, _, _, _, _, _, hitBodyId =
                            pcall(Physics.RaycastFull, ex, ey, ez, ndx, ndy, ndz, toDist)
                        if ok and hit and hitDist and hitDist < toDist - 0.05 then
                            hasLOS = (hitBodyId == id)
                        end
                    elseif Physics.Raycast then
                        local ok, hitDist = pcall(Physics.Raycast, ex, ey, ez, ndx, ndy, ndz, toDist)
                        if ok and hitDist and hitDist > 0 and hitDist < toDist - 0.05 then
                            hasLOS = false
                        end
                    end
                end
                if not hasLOS then break end

                self._sweepFired = true
                if _G.event_bus and _G.event_bus.publish and self._entityId then
                    _G.event_bus.publish("chain.lockon_sweep_hit", {
                        entityId = self._entityId,
                        partId   = id,
                        partX    = partX,
                        partY    = partY,
                        partZ    = partZ,
                    })
                end
                return
            until true
            end
        end
    end,

    OnDisable = function(self)
        self._sweepActive = false
        self._sweepFired  = false
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._subMoved     then pcall(function() _G.event_bus.unsubscribe(self._subMoved)     end) end
            if self._subRetracted then pcall(function() _G.event_bus.unsubscribe(self._subRetracted) end) end
            if self._subHit       then pcall(function() _G.event_bus.unsubscribe(self._subHit)       end) end
        end
    end,
}
