-- spherical_mover.lua
-- Moves an entity around the surface of a sphere using spherical coordinates.

require("engine_bootstrap")
local Component = require("mono_helper")
local TransformMixin = require("transform_mixin")

return Component {
    mixins = { TransformMixin },

    fields = {
        -- Sphere parameters
        radius = 2.0,          -- if 0, radius will be taken from current distance to center on Start
        centerX = 0.0,
        centerY = 0.0,
        centerZ = 0.0,
        centerOnStart = true,  -- if true, use the entity's starting position as the sphere center

        -- Angular speeds (radians per second)
        thetaSpeed = 1.0,      -- azimuthal speed (around Y axis)
        phiSpeed = 0.0,        -- polar speed (latitude change). keep small to avoid flipping

        -- initial angles (optional — Start will overwrite them from current pos if available)
        initialTheta = 0.0,
        initialPhi = 1.0,      -- polar angle measured from +Y (0..pi)

        -- small safety
        phiEpsilon = 0.01,
    },

    --- Helper: try to get a world position as a plain {x=,y=,z=}
    _safeGetPosition = function(self)
        -- try several common APIs, use pcall where appropriate
        local pos = nil

        -- Try accessor methods commonly used in engines
        local ok, res = pcall(function()
            if type(self.GetPosition) == "function" then return self:GetPosition() end
            if type(self.getPosition) == "function" then return self:getPosition() end
            if type(self.GetWorldPosition) == "function" then return self:GetWorldPosition() end
            if type(self.get_world_position) == "function" then return self:get_world_position() end
            return nil
        end)
        if ok and res then pos = res end

        -- try fields
        if not pos then
            if self.position then pos = self.position end
            if not pos and self.transform and self.transform.position then pos = self.transform.position end
        end

        -- normalize into {x=,y=,z=}
        if type(pos) == "table" then
            if pos.x and pos.y and pos.z then
                return { x = pos.x, y = pos.y, z = pos.z }
            elseif pos[1] and pos[2] and pos[3] then
                return { x = pos[1], y = pos[2], z = pos[3] }
            end
        end

        -- fallback
        return { x = 0.0, y = 0.0, z = 0.0 }
    end,

    Start = function(self)
        print("SphericalMover started (radius =", tostring(self.radius) .. ")")

        -- Determine center
        local worldPos = self:_safeGetPosition()
        if self.centerOnStart then
            -- if centerOnStart, set center to current world position (useful to orbit around spawn point)
            self.center = { x = worldPos.x, y = worldPos.y, z = worldPos.z }
        else
            self.center = { x = self.centerX, y = self.centerY, z = self.centerZ }
        end

        -- Compute initial spherical coordinates from current position relative to center
        local rel = { x = worldPos.x - self.center.x, y = worldPos.y - self.center.y, z = worldPos.z - self.center.z }
        local rlen = math.sqrt(rel.x*rel.x + rel.y*rel.y + rel.z*rel.z)

        -- If radius field is zero or negative, adopt the current distance
        if not self.radius or self.radius <= 0.0 then
            if rlen > 0.0001 then
                self.radius = rlen
            else
                self.radius = 1.0 -- fallback default
            end
        end

        -- Compute phi (polar) and theta (azimuth) from rel vector. 
        -- phi = angle from +Y (0..pi), theta = atan2(z, x)
        if rlen > 0.000001 then
            local ny = math.max(-1, math.min(1, rel.y / rlen))
            self.phi = math.acos(ny)
            self.theta = math.atan2(rel.z, rel.x)
        else
            -- fallback to fields or defaults
            self.theta = self.initialTheta or 0.0
            self.phi = self.initialPhi or (math.pi * 0.5)
        end

        -- safety clamp phi
        local eps = self.phiEpsilon or 0.01
        if self.phi < eps then self.phi = eps end
        if self.phi > math.pi - eps then self.phi = math.pi - eps end

        -- store previous world pos for Move(delta)
        -- compute the exact starting world position on the sphere surface using current angles
        local sinPhi = math.sin(self.phi)
        local startX = self.center.x + self.radius * sinPhi * math.cos(self.theta)
        local startY = self.center.y + self.radius * math.cos(self.phi)
        local startZ = self.center.z + self.radius * sinPhi * math.sin(self.theta)
        self._prevPos = { x = startX, y = startY, z = startZ }

        -- If the entity is not exactly at the computed surface point, we silently move it there now:
        local dx = startX - worldPos.x
        local dy = startY - worldPos.y
        local dz = startZ - worldPos.z
        if math.abs(dx) > 1e-6 or math.abs(dy) > 1e-6 or math.abs(dz) > 1e-6 then
            -- Move the entity to the computed start position
            if type(self.Move) == "function" then
                self:Move(dx, dy, dz)
            else
                -- If Move is not present, attempt to set position directly (best-effort)
                if self.SetPosition and type(self.SetPosition) == "function" then
                    self:SetPosition({ x = startX, y = startY, z = startZ })
                elseif self.set_position and type(self.set_position) == "function" then
                    self:set_position(startX, startY, startZ)
                elseif self.position then
                    self.position.x = startX; self.position.y = startY; self.position.z = startZ
                end
            end
        end
    end,

    Update = function(self, dt)
        -- advance angles
        self.theta = (self.theta or 0) + (self.thetaSpeed or 0) * dt
        self.phi = (self.phi or math.pi*0.5) + (self.phiSpeed or 0) * dt

        -- clamp phi to avoid passing exactly through poles (which can flip theta)
        local eps = self.phiEpsilon or 0.01
        if self.phi < eps then self.phi = eps end
        if self.phi > math.pi - eps then self.phi = math.pi - eps end

        -- compute new target position on sphere surface
        local sinPhi = math.sin(self.phi)
        local nx = self.center.x + self.radius * sinPhi * math.cos(self.theta)
        local ny = self.center.y + self.radius * math.cos(self.phi)
        local nz = self.center.z + self.radius * sinPhi * math.sin(self.theta)

        -- compute delta from previous known position and move
        local prev = self._prevPos or { x = nx, y = ny, z = nz } -- fallback if missing
        local dx = nx - prev.x
        local dy = ny - prev.y
        local dz = nz - prev.z

        if math.abs(dx) > 1e-9 or math.abs(dy) > 1e-9 or math.abs(dz) > 1e-9 then
            if type(self.Move) == "function" then
                self:Move(dx, dy, dz)
            else
                -- best-effort fallback to set position
                if self.SetPosition and type(self.SetPosition) == "function" then
                    self:SetPosition({ x = nx, y = ny, z = nz })
                elseif self.set_position and type(self.set_position) == "function" then
                    self:set_position(nx, ny, nz)
                elseif self.position then
                    self.position.x = nx; self.position.y = ny; self.position.z = nz
                end
            end
        end

        -- update prevPos
        self._prevPos = { x = nx, y = ny, z = nz }
    end,
}
