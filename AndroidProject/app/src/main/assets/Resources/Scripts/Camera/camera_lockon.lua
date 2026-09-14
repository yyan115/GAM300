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
    self._deadEnemies    = {}  -- track dead entity IDs to prevent re-locking

    -- Hits are queued here and resolved once per frame in M.update.
    -- deal_damage_to_entity is published once PER ENEMY per swing, so a swing
    -- that clips two enemies fires two events back to back. Assigning the
    -- target inside the subscriber made the last event through the bus win,
    -- which is why the camera snapped back and forth while fighting a group.
    self._lockonPending    = {}
    self._lockonHitCounts  = {}   -- entityId -> { count, firstHitAt }
    self._lockonEngagedAt  = 0.0  -- time of the last hit on the locked enemy
    self._lockonClock      = 0.0

    if event_bus and event_bus.subscribe then
        self._lockonDeathSub = event_bus.subscribe("enemy_died", function(data)
            if data and data.entityId then
                self._deadEnemies[data.entityId] = true
                if data.entityId == self._lockonEntityId then
                    M.breakLock(self)
                end
            end
        end)

        self._lockonDamageSub = event_bus.subscribe("deal_damage_to_entity", function(data)
            if not data or not data.entityId then return end
            -- Don't lock on to already-dead enemies
            if self._deadEnemies[data.entityId] then return end
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
                -- Acquire inside a TIGHTER radius than the one that breaks
                -- the lock. If both used the same distance an enemy sitting on
                -- the boundary would acquire and release repeatedly.
                if dist > (self.lockOnAcquireDistance or 12.0) then return end
                if not hasLineOfSight(self, ex, ey, ez) then return end
            end
            -- Queue the candidate; M.update decides. See M.init.
            -- Capped because M.update is skipped in some camera modes (chain
            -- aim, cinematics, cursor unlocked) and the queue would otherwise
            -- grow without bound while the player keeps swinging.
            local q = self._lockonPending
            if #q < 8 then q[#q + 1] = data.entityId end
        end)
    end
end

-- Call each frame BEFORE updateMouseLook.
-- When returning true the caller should skip normal mouse look (the lock-on
-- consumes the mouse axis to detect intentional camera movement).
-- Is this entity still a legal lock-on target RIGHT NOW?
--
-- Queued hits are resolved a frame after the event, so everything the
-- subscriber checked can have changed in between - most importantly the enemy
-- can have died from that very hit. Without re-checking here the camera locked
-- onto a corpse, or onto an entity already being torn down, which read as the
-- camera swinging to face nothing.
local function isLockable(self, id)
    if not id then return false end
    if self._deadEnemies[id] then return false end
    if not (Engine and Engine.GetEntityPosition) then return false end
    local ex, ey, ez = Engine.GetEntityPosition(id)
    if not ex then return false end
    local dx = ex - self._targetPos.x
    local dz = ez - self._targetPos.z
    if math.sqrt(dx * dx + dz * dz) > (self.lockOnAcquireDistance or 12.0) then return false end
    return hasLineOfSight(self, ex, ey, ez)
end

-- Resolve this frame's queued hits into at most one target change.
--
-- Policy, in order:
--   1. A hit on the enemy already engaged always wins and refreshes the
--      engagement. This is what stops a swing that clips a bystander from
--      stealing the camera.
--   2. Otherwise another enemy takes over only if the player means it: either
--      the current engagement has gone quiet for lockOnSwitchDelay, or that
--      enemy has been hit lockOnSwitchHits times inside lockOnSwitchWindow.
--   3. With no lock at all, any valid hit acquires immediately.
local function resolvePendingHits(self, dt)
    self._lockonClock = (self._lockonClock or 0.0) + dt

    local pending = self._lockonPending
    if #pending == 0 then return end

    local now          = self._lockonClock
    local switchWindow = self.lockOnSwitchWindow or 2.0
    local switchHits   = self.lockOnSwitchHits or 2
    local switchDelay  = self.lockOnSwitchDelay or 0.75
    local engaged      = self._lockonEntityId

    -- 1. engaged enemy hit again -> refresh, ignore everyone else this frame
    if engaged and not self._deadEnemies[engaged] then
        for i = 1, #pending do
            if pending[i] == engaged then
                self._lockonEngagedAt = now
                self._lockonPending   = {}
                return
            end
        end
    end

    -- Count hits per candidate inside the switch window.
    local counts = self._lockonHitCounts
    local candidate
    for i = 1, #pending do
        local id = pending[i]
        if not isLockable(self, id) then goto continue end
        local rec = counts[id]
        if not rec or (now - rec.firstHitAt) > switchWindow then
            rec = { count = 0, firstHitAt = now }
            counts[id] = rec
        end
        rec.count = rec.count + 1
        candidate = candidate or id
        ::continue::
    end
    self._lockonPending = {}
    if not candidate then return end

    if not engaged then
        self._lockonEntityId     = candidate
        self._lockonActive       = true
        self._lockonLOSLostTimer = 0.0
        self._lockonEngagedAt    = now
        return
    end

    -- 2. deliberate switch: stale engagement, or repeated hits on the new one
    local stale   = (now - (self._lockonEngagedAt or 0.0)) >= switchDelay
    local insists = (counts[candidate] and counts[candidate].count or 0) >= switchHits
    if stale or insists then
        self._lockonEntityId     = candidate
        self._lockonActive       = true
        self._lockonLOSLostTimer = 0.0
        self._lockonEngagedAt    = now
        counts[candidate]        = nil
    end
end

function M.update(self, dt)
    resolvePendingHits(self, dt)

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
    self._lockonEngagedAt    = 0.0
    self._lockonPending      = {}
    self._lockonHitCounts    = {}
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
    self._deadEnemies        = {}
    self._lockonPending      = {}
    self._lockonHitCounts    = {}
    self._lockonEngagedAt    = 0.0
end

return M
