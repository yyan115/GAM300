-- LockOnPoint.lua
-- Attach to the LockOn child entity on an enemy.
-- PartEntityIds: one extendable list of entity IDs for the body parts
-- you want to check (head, chest, hand, etc.).
-- IDs are assigned in the editor — no name lookups, no duplicates.

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        -- Populate this list in the editor with the entity IDs of each
        -- body part you want to consider for attachment distance checks.
        PartEntityIds = {},
    },

    -- Returns the world position {x, y, z} and entity ID of the closest
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
            local id = ids[i]
            local transform = Engine.FindTransformByID(id)
            if transform then
                -- GetTransformWorldPosition returns a table {x,y,z} from the C++ tuple binding
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
        end

        return bestPos, bestId, math.sqrt(bestDistSq)
    end,
}