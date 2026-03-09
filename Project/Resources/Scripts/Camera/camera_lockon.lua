-- Camera/camera_lockon.lua
-- Combat lock-on: when the player hits an enemy the camera orbits behind the
-- player so the enemy stays centered on screen.  The lock breaks when the
-- player moves the mouse, moves too far from the enemy, or line-of-sight
-- to the enemy is blocked by geometry.

local utils = require("Camera.camera_utils")
local atan2 = utils.atan2

local event_bus = _G.event_bus

local M = {}

local function shortestDelta(from, to)
    local d = (to - from) % 360.0
    if d > 180.0 then d = d - 360.0 end
    return d
end

-- Returns true if there is an unobstructed line from the player to the enemy.
-- Casts from player center-mass toward the enemy; if geometry is hit before
-- reaching the enemy the line-of-sight is blocked.
local function hasLineOfSight(self, ex, ey, ez)
    if not Physics then return true end

    -- Raycast origin: player position raised to roughly center-mass
    local ox = self._targetPos.x
    local oy = self._targetPos.y + (self.lockOnLOSHeight or 1.0)
    local oz = self._targetPos.z

    -- Target: enemy position (also raised slightly above feet)
    local tx = ex
    local ty = ey + (self.lockOnLOSHeight or 1.0)
    local tz = ez

    local dx = tx - ox
    local dy = ty - oy
    local dz = tz - oz
    local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
    if dist < 0.01 then return true end

    local ndx, ndy, ndz = dx / dist, dy / dist, dz / dist

    -- Prefer RaycastFull (returns bodyId so we can tell if we hit the enemy
    -- itself rather than a wall), fall back to basic Raycast.
    if Physics.RaycastFull then
        local ok, hit, hitDist = pcall(function()
            return Physics.RaycastFull(ox, oy, oz, ndx, ndy, ndz, dist)
        end)
        if ok and hit and hitDist and hitDist > 0 and hitDist < dist - 0.3 then
            return false -- something between player and enemy
        end
        return true
    elseif Physics.Raycast then
        local ok, hitDist = pcall(function()
            return Physics.Raycast(ox, oy, oz, ndx, ndy, ndz, dist)
        end)
        if ok and hitDist and hitDist > 0 and hitDist < dist - 0.3 then
            return false
        end
        return true
    end

    return true -- no physics available, assume clear
end

-- Call once from CameraFollow.Awake
function M.init(self)
    self._lockonActive   = false
    self._lockonEntityId = nil
    self._lockonLOSLostTimer = 0.0

    if event_bus and event_bus.subscribe then
        self._lockonDeathSub = event_bus.subscribe("enemy_died", function(data)
            if data and data.entityId and data.entityId == self._lockonEntityId then
                M.breakLock(self)
            end
        end)

        self._lockonDamageSub = event_bus.subscribe("deal_damage_to_entity", function(data)
            if not data or not data.entityId then return end
            -- Don't activate during chain aim or cinematics
            if self._chainAiming or self._cinematicActive then return end
            -- Verify the hit entity is actually an enemy (not a wall/prop/ground)
            if Engine and Engine.FindEntitiesWithScript then
                local isEnemy = false
                for _, scriptName in ipairs(self.enemyComponents or {}) do
                    local entities = Engine.FindEntitiesWithScript(scriptName)
                    if entities then
                        for i = 1, #entities do
                            if entities[i] == data.entityId then
                                isEnemy = true
                                break
                            end
                        end
                    end
                    if isEnemy then break end
                end
                if not isEnemy then return end
            end
            -- Verify the enemy exists and is within range + line-of-sight
            if Engine and Engine.GetEntityPosition then
                local ex, ey, ez = Engine.GetEntityPosition(data.entityId)
                if not ex then return end
                local dx = ex - self._targetPos.x
                local dz = ez - self._targetPos.z
                local dist = math.sqrt(dx * dx + dz * dz)
                if dist > (self.lockOnBreakDistance or 15.0) then return end
                if not hasLineOfSight(self, ex, ey, ez) then return end
            end
            self._lockonEntityId     = data.entityId
            self._lockonActive       = true
            self._lockonLOSLostTimer = 0.0
        end)
    end
end

-- Call each frame BEFORE updateMouseLook.
-- When returning true the caller should skip normal mouse look (the lock-on
-- consumes the mouse axis to detect intentional camera movement).
function M.update(self, dt)
    if not self._lockonActive or not self._lockonEntityId then
        return false
    end

    -- Break if another mode took over
    if self._chainAiming or self._cinematicActive then
        M.breakLock(self)
        return false
    end

    -- Read the mouse axis — if the player moves the mouse beyond a small
    -- dead-zone, break the lock and let normal mouse look resume next frame.
    if Input and Input.GetAxis then
        local lookAxis = Input.GetAxis("Look")
        if lookAxis then
            local mag = math.sqrt(lookAxis.x * lookAxis.x + lookAxis.y * lookAxis.y)
            if mag > (self.lockOnMouseThreshold or 2.0) then
                M.breakLock(self)
                return false
            end
        end
    end

    -- Verify the enemy still exists
    if not (Engine and Engine.GetEntityPosition) then
        M.breakLock(self)
        return false
    end
    local ex, ey, ez = Engine.GetEntityPosition(self._lockonEntityId)
    if not ex then
        M.breakLock(self)
        return false
    end

    -- Break if the player is too far from the enemy
    local dx = ex - self._targetPos.x
    local dz = ez - self._targetPos.z
    local dist = math.sqrt(dx * dx + dz * dz)
    if dist > (self.lockOnBreakDistance or 15.0) then
        M.breakLock(self)
        return false
    end

    -- Break if line-of-sight is blocked for too long (small grace period so
    -- brief occlusions like the player's own model don't flicker the lock).
    if not hasLineOfSight(self, ex, ey, ez) then
        self._lockonLOSLostTimer = self._lockonLOSLostTimer + dt
        if self._lockonLOSLostTimer > (self.lockOnLOSGrace or 0.5) then
            M.breakLock(self)
            return false
        end
    else
        self._lockonLOSLostTimer = 0.0
    end

    -- Target yaw: place the camera on the opposite side of the player from
    -- the enemy so the camera looks past the player toward the enemy.
    local enemyAngle = math.deg(atan2(dx, dz))
    local targetYaw  = enemyAngle + 180.0

    -- Snap-then-smooth rotation: immediately cover most of the gap so the
    -- enemy stays centered even when the player or enemy is moving quickly.
    local deltaYaw = shortestDelta(self._yaw, targetYaw)
    local snapFraction = self.lockOnSnapFraction or 0.85
    local smoothT = 1.0 - math.exp(-(self.lockOnRotSpeed or 20.0) * dt)
    local t = snapFraction + (1.0 - snapFraction) * smoothT
    if t > 1.0 then t = 1.0 end
    self._yaw = self._yaw + deltaYaw * t

    return true
end

function M.breakLock(self)
    self._lockonActive       = false
    self._lockonEntityId     = nil
    self._lockonLOSLostTimer = 0.0
end

-- Call from CameraFollow.OnDisable
function M.cleanup(self)
    if event_bus and event_bus.unsubscribe then
        if self._lockonDamageSub then
            event_bus.unsubscribe(self._lockonDamageSub)
            self._lockonDamageSub = nil
        end
        if self._lockonDeathSub then
            event_bus.unsubscribe(self._lockonDeathSub)
            self._lockonDeathSub = nil
        end
    end
    self._lockonActive       = false
    self._lockonEntityId     = nil
    self._lockonLOSLostTimer = 0.0
end

return M
