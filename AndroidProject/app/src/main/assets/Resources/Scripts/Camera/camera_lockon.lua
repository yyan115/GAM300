-- Combat camera: retain the engaged target until it becomes invalid or the
-- player deliberately takes over. Resolve hit candidates once per frame so a
-- multi-target attack cannot choose a different camera target for every hit.
local utils = require("Camera.camera_utils")
local CameraInput = require("Camera.camera_input")
local event_bus = _G.event_bus
local M = {}

local function shortestDelta(from, to)
    return (to - from + 180.0) % 360.0 - 180.0
end

local function clearPending(self)
    for id in pairs(self._lockonPending) do self._lockonPending[id] = nil end
end

local function suspended(self)
    if self._chainAiming or self._cinematicActive then return true end
    for _, active in pairs(self._lockonModes) do
        if active then return true end
    end
    return false
end

-- Use the world-geometry LOS query. A general ray can hit the player's own
-- capsule at its origin and hide the wall behind it, or treat another enemy as
-- an obstruction. Neither should determine whether the camera can keep a lock.
local function hasLineOfSight(self, ex, ey, ez)
    if not (Physics and Physics.RaycastLOS) then return false end
    local height = self.lockOnLOSHeight or 1.0
    local ox, oy, oz = self._targetPos.x, self._targetPos.y + height, self._targetPos.z
    local dx, dy, dz = ex - ox, ey + height - oy, ez - oz
    local distance = math.sqrt(dx * dx + dy * dy + dz * dz)
    if distance < 0.01 then return true end
    local hit = Physics.RaycastLOS(ox, oy, oz, dx / distance, dy / distance, dz / distance, distance)
    return not hit or hit < 0 or hit >= distance
end

local function targetPosition(self, id, range)
    if not id or self._deadEnemies[id] then return nil end
    if not (Engine and Engine.GetEntityPosition and Engine.GetEntityTag) then return nil end
    if Engine.IsEntityActive and not Engine.IsEntityActive(id) then return nil end
    local tag = Engine.GetEntityTag(id)
    if tag ~= "Enemy" and tag ~= "Boss" then return nil end
    local x, y, z = Engine.GetEntityPosition(id)
    if not x then return nil end
    local dx, dz = x - self._targetPos.x, z - self._targetPos.z
    local distanceSquared = dx * dx + dz * dz
    if distanceSquared > range * range then return nil end
    return x, y, z, distanceSquared
end

function M.breakLock(self, keepPending)
    self._lockonActive = false
    self._lockonEntityId = nil
    self._lockonLOSLostTimer = 0.0
    self._lockonEngagedAt = 0.0
    if self._lockonPending and not keepPending then clearPending(self) end
end

function M.init(self)
    M.cleanup(self)
    self._lockonPending = {}
    self._lockonModes = {}
    self._lockonClock = 0.0
    self._lockonManualUntil = 0.0
    self._deadEnemies = self._deadEnemies or {}
    self._lockonSubscriptions = {}
    M.breakLock(self)

    -- Desktop keeps manual camera control and does not collect lock candidates.
    if not self.lockOnEnabled or not (event_bus and event_bus.subscribe) then return end
    local function subscribe(name, callback)
        self._lockonSubscriptions[#self._lockonSubscriptions + 1] = event_bus.subscribe(name, callback)
    end
    local function died(data)
        if not data or not data.entityId then return end
        local id = data.entityId
        self._deadEnemies[id] = true
        self._lockonPending[id] = nil
        if id == self._lockonEntityId then
            -- Other survivors hit in this frame remain eligible for acquisition.
            M.breakLock(self, true)
        end
    end
    subscribe("enemy_died", died)
    subscribe("boss_killed", died)
    subscribe("deal_damage_to_entity", function(data)
        if not data or not data.entityId or self._deadEnemies[data.entityId] then return end
        if suspended(self) or self._lockonClock < self._lockonManualUntil then return end
        self._lockonPending[data.entityId] = true
    end)

    local function mode(name, active)
        self._lockonModes[name] = active == true
        if active then M.breakLock(self) end
    end
    -- Keep separate mode flags: changing CameraFollow's flags here would affect
    -- its own rising-edge callbacks, which initialize chain aim and cinematics.
    subscribe("chain.aim_camera", function(p) mode("chain", p and p.active) end)
    subscribe("cinematic.active", function(active) mode("cinematic", active) end)
    subscribe("flythrough.active", function(active) mode("flythrough", active) end)
    subscribe("game_paused", function(active) mode("paused", active) end)
    subscribe("playerDead", function(dead) mode("dead", dead) end)
    local function respawn()
        mode("dead", false)
        self._lockonManualUntil = self._lockonClock
        M.breakLock(self)
    end
    subscribe("respawnPlayer", function(active) if active then respawn() end end)
    subscribe("playerRespawned", respawn)
end

local function resolvePendingHits(self)
    local pending = self._lockonPending
    if self._lockonActive then
        if pending[self._lockonEntityId] then self._lockonEngagedAt = self._lockonClock end
        -- Incidental hits and multi-hit spells cannot steal a valid engagement.
        clearPending(self)
        return
    end

    local bestId, bestAngle, bestDistance
    for id in pairs(pending) do
        local x, y, z, distance = targetPosition(self, id, self.lockOnAcquireDistance or 12.0)
        if x and hasLineOfSight(self, x, y, z) then
            local yaw = math.deg(utils.atan2(x - self._targetPos.x, z - self._targetPos.z)) + 180.0
            local angle = math.abs(shortestDelta(self._yaw, yaw))
            -- Prefer the hit nearest the current view, then distance and entity
            -- ID. The result is independent of event delivery/table iteration order.
            if not bestId or angle < bestAngle
                or (angle == bestAngle and (distance < bestDistance
                    or (distance == bestDistance and id < bestId))) then
                bestId, bestAngle, bestDistance = id, angle, distance
            end
        end
    end
    clearPending(self)
    if bestId then
        self._lockonEntityId = bestId
        self._lockonActive = true
        self._lockonLOSLostTimer = 0.0
        self._lockonEngagedAt = self._lockonClock
    end
end

-- Return true only when this module owns yaw for this frame. Otherwise ordinary
-- look input runs in the same frame, including the drag that releases a lock.
function M.update(self, dt)
    if not self.lockOnEnabled then return false end
    dt = math.max(dt, 0.0)
    self._lockonClock = self._lockonClock + dt
    if suspended(self) then
        M.breakLock(self)
        return false
    end

    local look = Input and Input.GetAxis and Input.GetAxis("Look")
    if look then
        local sensitivity = CameraInput.lookSensitivity(self)
        local degrees = math.sqrt(look.x * look.x + look.y * look.y) * sensitivity
        if degrees > (self.lockOnManualThreshold or 0.3) then
            self._lockonManualUntil = self._lockonClock + (self.lockOnManualCooldown or 0.6)
            M.breakLock(self)
            return false
        end
    end
    if self._lockonClock < self._lockonManualUntil then
        M.breakLock(self)
        return false
    end

    if self._lockonActive then
        local x, y, z = targetPosition(self, self._lockonEntityId, self.lockOnBreakDistance or 15.0)
        if not x then
            M.breakLock(self, true)
        elseif hasLineOfSight(self, x, y, z) then
            self._lockonLOSLostTimer = 0.0
        else
            self._lockonLOSLostTimer = self._lockonLOSLostTimer + dt
            if self._lockonLOSLostTimer >= (self.lockOnLOSGrace or 0.5) then M.breakLock(self, true) end
        end
    end
    resolvePendingHits(self)
    if not self._lockonActive then return false end

    local x, _, z = Engine.GetEntityPosition(self._lockonEntityId)
    local targetYaw = math.deg(utils.atan2(x - self._targetPos.x, z - self._targetPos.z)) + 180.0
    local delta = shortestDelta(self._yaw, targetYaw)
    local smooth = 1.0 - math.exp(-(self.lockOnRotSpeed or 20.0) * dt)
    local maxStep = (self.lockOnMaxYawSpeed or 120.0) * dt
    self._yaw = self._yaw + utils.clamp(delta * smooth, -maxStep, maxStep)

    -- Movement must use the camera's new heading even though normal mouse-look
    -- publication is skipped while lock-on owns the rotation.
    _G.CAMERA_YAW = self._yaw
    if event_bus and event_bus.publish then event_bus.publish("camera_yaw", self._yaw) end
    return true
end

function M.cleanup(self)
    if event_bus and event_bus.unsubscribe then
        for _, subscription in ipairs(self._lockonSubscriptions or {}) do
            event_bus.unsubscribe(subscription)
        end
    end
    self._lockonSubscriptions = nil
    self._lockonModes = {}
    M.breakLock(self)
end

return M
