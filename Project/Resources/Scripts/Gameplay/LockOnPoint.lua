-- LockOnPoint.lua
-- Attach to the LockOn child entity on an enemy (one tier below root).
-- PartEntityIds: list of entity IDs for the body parts to consider for attachment.
-- IDs are assigned in the editor — no name lookups, no duplicates.
--
-- Secondary sweep check: each Update while the endpoint is extending, performs a
-- point-to-segment radius check + raycast against each part so a fast-moving
-- endpoint that tunnels through the trigger volume is still detected.
-- Publishes "chain.lockon_sweep_hit" with { entityId, partId, partX, partY, partZ }
-- so ChainEndpointController treats it identically to a real OnTriggerEnter.

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        -- Body part entity names, populated in the editor.
        PartEntityNames = {},

        -- Radius within which the endpoint is considered to have hit a part.
        -- Should match or slightly exceed the endpoint trigger collider radius.
        SweepRadius = 0.3,
    },

    Start = function(self)
        -- Initialize the empty table to hold the IDs
        self.PartEntityIds = {}
        local parentId = Engine.GetParentEntity(self.entityId)

        -- Loop through all the names provided by the editor
        for _, partName in pairs(self.PartEntityNames) do
            
            -- Skip any empty strings just in case the editor left a blank entry
            if partName and partName ~= "" then
                
                -- Search the hierarchy starting from parentId
                print("Calling Engine.FindChildByName")
                local childId = Engine.FindChildByName(parentId, partName)
                
                if childId ~= -1 then
                    -- Store it in the dictionary using the name as the key
                    self.PartEntityIds[partName] = childId
                    print("[LockOnPoint] Found body part: " .. partName .. " (ID: " .. childId .. ")")
                else
                    print("[LockOnPoint] WARNING: Could not find child named: " .. partName)
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

        self._endpointPos  = nil   -- {x,y,z} last known endpoint world position
        self._endpointPrev = nil   -- position one frame ago for segment sweep
        self._sweepActive  = false -- only sweep while endpoint is extending
        self._sweepFired   = false -- prevent double-firing until chain resets

        if _G.event_bus and _G.event_bus.subscribe then
            self._subMoved = _G.event_bus.subscribe("chain.endpoint_moved", function(payload)
                if not payload then return end
                if self._endpointPos then
                    self._endpointPrev = {
                        self._endpointPos.x,
                        self._endpointPos.y,
                        self._endpointPos.z,
                    }
                end
                if payload.position then
                    self._endpointPos = payload.position
                end
                self._sweepActive = payload.isExtending or false
            end)

            -- Reset sweep state when chain fully retracts
            self._subRetracted = _G.event_bus.subscribe("chain.endpoint_retracted", function(_)
                self._sweepActive  = false
                self._sweepFired   = false
                self._endpointPos  = nil
                self._endpointPrev = nil
            end)

            -- Suppress sweep if trigger collision already handled it
            self._subHit = _G.event_bus.subscribe("chain.endpoint_hit_entity", function(_)
                self._sweepFired = true
            end)
        end
    end,

    -- Returns the world position {x,y,z}, entity ID, and distance of the closest
    -- body part to the given world position (px, py, pz).
    -- Returns nil if no parts are registered or Engine API unavailable.
    GetClosestPart = function(self, px, py, pz)
        local ids = self.PartEntityIds
        if not ids or #ids == 0 then return nil end
        if not (Engine and Engine.FindTransformByID and Engine.GetTransformWorldPosition) then return nil end

        local bestDistSq = math.huge
        local bestPos    = nil
        local bestId     = nil

        for i = 1, #ids do
            local id = tonumber(ids[i])
            if id and id ~= 0 then
            local transform = Engine.FindTransformByID(id)
            if transform then
                local p = Engine.GetTransformWorldPosition(transform)
                local ex, ey, ez
                if type(p) == "table" then
                    ex = p[1] or p.x or 0.0
                    ey = p[2] or p.y or 0.0
                    ez = p[3] or p.z or 0.0
                else
                    ex, ey, ez = p or 0.0, 0.0, 0.0
                end

                local dx = px - ex
                local dy = py - ey
                local dz = pz - ez
                local distSq = dx*dx + dy*dy + dz*dz
                if distSq < bestDistSq then
                    bestDistSq = distSq
                    bestPos    = { x = ex, y = ey, z = ez }
                    bestId     = id
                end
            end
            end -- if id valid
        end

        return bestPos, bestId, math.sqrt(bestDistSq)
    end,

    Update = function(self, dt)
        if not self._sweepActive or self._sweepFired then return end
        if not self._endpointPos then return end

        local ids = self.PartEntityIds
        if not ids or #ids == 0 then return end
        if not (Engine and Engine.FindTransformByID and Engine.GetTransformWorldPosition) then return end

        local ex = self._endpointPos.x
        local ey = self._endpointPos.y
        local ez = self._endpointPos.z

        local px = self._endpointPrev and self._endpointPrev[1] or ex
        local py = self._endpointPrev and self._endpointPrev[2] or ey
        local pz = self._endpointPrev and self._endpointPrev[3] or ez

        local sweepRad   = tonumber(self.SweepRadius) or 0.3
        local sweepRadSq = sweepRad * sweepRad

        -- Endpoint movement segment prev→current for tunnelling check
        local segDX, segDY, segDZ = ex - px, ey - py, ez - pz
        local segLenSq = segDX*segDX + segDY*segDY + segDZ*segDZ

        for i = 1, #ids do
            local id = tonumber(ids[i])
            if not id or id == 0 then
                -- skip unset/empty slots in the editor-populated list
            else
            repeat
                local transform = Engine.FindTransformByID(id)
                if not transform then break end

                local p = Engine.GetTransformWorldPosition(transform)
                local partX, partY, partZ
                if type(p) == "table" then
                    partX = p[1] or p.x or 0.0
                    partY = p[2] or p.y or 0.0
                    partZ = p[3] or p.z or 0.0
                else
                    partX, partY, partZ = p or 0.0, 0.0, 0.0
                end

                -- Find closest point on the endpoint movement segment to this part
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

                -- Radius check: closest point on segment must be within SweepRadius of the part
                if ddx*ddx + ddy*ddy + ddz*ddz > sweepRadSq then break end

                -- LOS check: raycast from current endpoint position toward part
                local toPX = partX - ex
                local toPY = partY - ey
                local toPZ = partZ - ez
                local toDist = math.sqrt(toPX*toPX + toPY*toPY + toPZ*toPZ)
                local hasLOS = true

                if Physics and toDist > 1e-4 then
                    local ndx = toPX / toDist
                    local ndy = toPY / toDist
                    local ndz = toPZ / toDist

                    if Physics.RaycastFull then
                        local ok, hit, hitDist, _, _, _, _, _, _, hitBodyId = pcall(function()
                            return Physics.RaycastFull(ex, ey, ez, ndx, ndy, ndz, toDist)
                        end)
                        if ok and hit and hitDist and hitDist < toDist - 0.05 then
                            hasLOS = (hitBodyId == id)
                        end
                    elseif Physics.Raycast then
                        local ok, hitDist = pcall(function()
                            return Physics.Raycast(ex, ey, ez, ndx, ndy, ndz, toDist)
                        end)
                        if ok and hitDist and hitDist > 0 and hitDist < toDist - 0.05 then
                            hasLOS = false
                        end
                    end
                end

                if not hasLOS then break end

                -- Hit confirmed — publish sweep hit and stop checking
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
            end -- if id valid
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