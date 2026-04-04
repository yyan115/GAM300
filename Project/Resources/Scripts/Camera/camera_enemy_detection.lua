-- Camera/camera_enemy_detection.lua
-- Enemy proximity detection and automatic action mode triggering

local M = {}

-- Returns the closest enemy entity ID and its XZ distance from the player.
-- Uses Engine.FindEntitiesWithScript (C++ cached) for performance.
function M.findClosestEnemy(self)
    if not self._hasTarget or not Engine
    or not Engine.FindEntitiesWithScript
    or not Engine.GetEntityPosition then
        return nil, math.huge
    end

    local playerX = self._targetPos.x
    local playerZ = self._targetPos.z
    local closestDist  = math.huge
    local closestEnemy = nil

    for _, scriptName in ipairs(self.enemyComponents or {}) do
        local entities = Engine.FindEntitiesWithScript(scriptName)
        if entities and #entities > 0 then
            for i = 1, #entities do
                local entityId = entities[i]
                local x, y, z = Engine.GetEntityPosition(entityId)
                if x and y and z then
                    local dx   = x - playerX
                    local dz   = z - playerZ
                    local dist = math.sqrt(dx * dx + dz * dz)
                    if dist < closestDist then
                        closestDist  = dist
                        closestEnemy = entityId
                    end
                end
            end
        end
    end

    return closestEnemy, closestDist
end

-- Returns the XZ distance from the player to a specific enemy entity.
function M.getDistanceToEnemy(self, enemyId)
    if not enemyId or not Engine or not Engine.GetEntityPosition then
        return math.huge
    end
    local x, y, z = Engine.GetEntityPosition(enemyId)
    if not x or not y or not z then return math.huge end

    local dx = x - self._targetPos.x
    local dz = z - self._targetPos.z
    return math.sqrt(dx * dx + dz * dz)
end

-- Drives action mode on/off based on enemy proximity each frame.
function M.updateEnemyProximity(self, dt)
    if not self.enableEnemyDetection then return end

    -- ── CASE 1: Action mode is active and was triggered by an enemy ──────────
    if self._actionModeActive and self._enemyTriggeredActionMode then
        if self._triggeringEnemyId then
            local dist = M.getDistanceToEnemy(self, self._triggeringEnemyId)

            if self.debugEnemyDetection then
                --print(string.format("[CameraFollow] Action mode active - dist to trigger enemy: %.1f", dist))
            end

            if dist > self.enemyDisengageRange then
                self._enemyDisengageTimer = self._enemyDisengageTimer + dt

                if self.debugEnemyDetection and self._enemyDisengageTimer > 0.1 then
                    --print(string.format("[CameraFollow] Enemy beyond disengage range - timer: %.1fs / %.1fs",
                    --    self._enemyDisengageTimer, self.enemyDisengageDelay))
                end

                if self._enemyDisengageTimer >= self.enemyDisengageDelay then
                    self._actionModeActive        = false
                    self._enemyTriggeredActionMode = false
                    self._enemyDisengageTimer      = 0.0
                    self._triggeringEnemyId        = nil
                    if self.debugEnemyDetection then
                        --print("[CameraFollow] Action Mode DISABLED - enemy left range")
                    end
                end
            else
                -- Enemy returned within range; reset timer
                if self._enemyDisengageTimer > 0 and self.debugEnemyDetection then
                    --print("[CameraFollow] Enemy returned within range - timer reset")
                end
                self._enemyDisengageTimer = 0.0
            end
        else
            -- Lost track of triggering enemy; search for any nearby enemy
            local closestEnemy, closestDist = M.findClosestEnemy(self)
            if closestEnemy and closestDist <= self.enemyDisengageRange then
                self._triggeringEnemyId   = closestEnemy
                self._enemyDisengageTimer = 0.0
                if self.debugEnemyDetection then
                    --print(string.format("[CameraFollow] Locked onto new enemy: %d at %.1fu", closestEnemy, closestDist))
                end
            else
                self._actionModeActive        = false
                self._enemyTriggeredActionMode = false
                self._triggeringEnemyId        = nil
                if self.debugEnemyDetection then
                    --print("[CameraFollow] Action Mode DISABLED - no enemies found")
                end
            end
        end
        return -- Skip search phase while action mode is already running
    end

    -- ── CASE 2: Not in action mode — scan for enemies ────────────────────────
    local closestEnemy, closestDist = M.findClosestEnemy(self)

    local wasInRange      = self._enemyInRange
    self._enemyInRange    = closestEnemy ~= nil and closestDist <= self.enemyDetectionRange

    if self.debugEnemyDetection and self._enemyInRange ~= wasInRange then
        --print(string.format("[CameraFollow] Enemy proximity changed: %s (%.1fu)",
        --    tostring(self._enemyInRange), closestDist))
    end

    if self._enemyInRange then
        self._enemyDisengageTimer = 0.0
        if not self._actionModeActive then
            self._actionModeActive        = true
            self._enemyTriggeredActionMode = true
            self._triggeringEnemyId        = closestEnemy
            if self.debugEnemyDetection then
                --print(string.format("[CameraFollow] Action Mode ENABLED by enemy %d at %.1fu",
                --    closestEnemy, closestDist))
            end
        end
    end
end

return M
