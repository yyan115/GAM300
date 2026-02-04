-- CinematicCamera.lua 
-- Handles cinematic camera mode with position/rotation tracking and automatic timeout
-- FIXED: Uses Engine.GetTransformRotation instead of GetTransformWorldRotation

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local event_bus = _G.event_bus

-- Helper: Euler (deg) → Quaternion
local function eulerToQuat(x, y, z)
    local p = math.rad(x or 0) * 0.5
    local ya = math.rad(y or 0) * 0.5
    local r = math.rad(z or 0) * 0.5
    local sinP, cosP = math.sin(p), math.cos(p)
    local sinY, cosY = math.sin(ya), math.cos(ya)
    local sinR, cosR = math.sin(r), math.cos(r)
    return {
        w = cosP * cosY * cosR + sinP * sinY * sinR,
        x = sinP * cosY * cosR - cosP * sinY * sinR,
        y = cosP * sinY * cosR + sinP * cosY * sinR,
        z = cosP * cosY * sinR - sinP * sinY * cosR
    }
end

-- Helper: atan2 that works in both Lua 5.3 and 5.4
local function atan2(y, x)
    if math.atan2 then
        return math.atan2(y, x)
    else
        -- Lua 5.4: math.atan accepts two arguments
        return math.atan(y, x)
    end
end

-- Helper: Quaternion → Euler (deg) for debugging
local function quatToEuler(qw, qx, qy, qz)
    local sinr_cosp = 2 * (qw * qx + qy * qz)
    local cosr_cosp = 1 - 2 * (qx * qx + qy * qy)
    local roll = atan2(sinr_cosp, cosr_cosp)

    local sinp = 2 * (qw * qy - qz * qx)
    local pitch
    if math.abs(sinp) >= 1 then
        pitch = math.pi / 2 * (sinp >= 0 and 1 or -1)
    else
        pitch = math.asin(sinp)
    end

    local siny_cosp = 2 * (qw * qz + qx * qy)
    local cosy_cosp = 1 - 2 * (qy * qy + qz * qz)
    local yaw = atan2(siny_cosp, cosy_cosp)

    return math.deg(pitch), math.deg(yaw), math.deg(roll)
end

return Component {
    mixins = { TransformMixin },

    fields = {
        cinematicActive = false,
        targetTransformName = "",
        targetPosition = {x = -2, y = 1, z = -2},
        targetRotation = {x = 0, y = 0, z = 0},
        transitionSpeed = 0.5,
        useSequence = false,
        sequenceWaypoints = {},
        sequenceInterval = 3.0,
        sequenceLoop = false,
        
        -- Timer settings
        useDuration = false,
        cinematicDuration = 5.0,
        
        debugMode = true,
    },

    Awake = function(self)
        self._wasCinematicActive = false
        self._currentWaypointIndex = 1
        self._waypointTimer = 0.0
        self._durationTimer = 0.0
        
        print("[CinematicCamera] Initialized")
    end,

    Update = function(self, dt)
        -- Detect activation/deactivation
        if self.cinematicActive ~= self._wasCinematicActive then
            self._wasCinematicActive = self.cinematicActive
            
            if self.cinematicActive then
                print("[CinematicCamera] Cinematic mode ENABLED")
                self._durationTimer = 0.0
                
                if event_bus and event_bus.publish then
                    event_bus.publish("cinematic.active", true)
                end
                if self.useSequence then
                    self._currentWaypointIndex = 1
                    self._waypointTimer = 0.0
                end
            else
                print("[CinematicCamera] Cinematic mode DISABLED")
                if event_bus and event_bus.publish then
                    event_bus.publish("cinematic.active", false)
                end
            end
        end
        
        if not self.cinematicActive then
            return
        end
        
        -- Duration timer
        if self.useDuration then
            self._durationTimer = self._durationTimer + dt
            
            if self._durationTimer >= self.cinematicDuration then
                print(string.format("[CinematicCamera] Duration timeout (%.2fs) - disabling", 
                                  self._durationTimer))
                self.cinematicActive = false
                return
            end
        end
        
        -- Sequence mode
        if self.useSequence and self.sequenceWaypoints and #self.sequenceWaypoints > 0 then
            self._waypointTimer = self._waypointTimer + dt
            
            if self._waypointTimer >= self.sequenceInterval then
                self._waypointTimer = 0.0
                self._currentWaypointIndex = self._currentWaypointIndex + 1
                
                if self._currentWaypointIndex > #self.sequenceWaypoints then
                    if self.sequenceLoop then
                        self._currentWaypointIndex = 1
                    else
                        self.cinematicActive = false
                        return
                    end
                end
                
                self.targetTransformName = self.sequenceWaypoints[self._currentWaypointIndex]
            end
        end
        
        -- Get target position & rotation
        local targetPos = nil
        local targetRot = nil
        
        if self.targetTransformName and self.targetTransformName ~= "" then
            -- STRIP QUOTES AND WHITESPACE
            local cleanName = self.targetTransformName
            cleanName = cleanName:gsub('"', ''):gsub("'", ''):gsub("^%s*(.-)%s*$", "%1")
            
            if self.debugMode then
                print(string.format("[CinematicCamera] Searching for: '%s'", cleanName))
            end
            
            if Engine and Engine.FindTransformByName then
                local targetTransform = Engine.FindTransformByName(cleanName)
                
                if targetTransform then
                    if self.debugMode then
                        print("[CinematicCamera] Transform FOUND!")
                    end
                    
                    -- Get Position
                    if Engine.GetTransformWorldPosition then
                        local positionTable = Engine.GetTransformWorldPosition(targetTransform)
    
                        if positionTable and type(positionTable) == "table" then
                            local x = positionTable[1] or positionTable.x or positionTable._1
                            local y = positionTable[2] or positionTable.y or positionTable._2
                            local z = positionTable[3] or positionTable.z or positionTable._3
        
                            if x and y and z then
                                targetPos = {x = x, y = y, z = z}
                                if self.debugMode then
                                    print(string.format("[CinematicCamera] Position: (%.2f, %.2f, %.2f)", x, y, z))
                                end
                            end
                        end
                    end
                    
                    -- Get Rotation - TRY MULTIPLE METHODS
                    if self.debugMode then
                        print("[CinematicCamera] Attempting to get rotation...")
                    end
                    
                    -- METHOD 1: Try accessing worldRotation property directly from Transform
                    if targetTransform.localRotation then
                        local rot = targetTransform.localRotation
                        if self.debugMode then
                            print(string.format("[CinematicCamera] localRotation type: %s", type(rot)))
                            if type(rot) == "userdata" or type(rot) == "table" then
                                print(string.format("[CinematicCamera] localRotation.w=%s, .x=%s, .y=%s, .z=%s",
                                    tostring(rot.w), tostring(rot.x), tostring(rot.y), tostring(rot.z)))
                            end
                        end
                        
                        if rot and (rot.w or rot.x or rot.y or rot.z) then
                            targetRot = {
                                qw = rot.w or 1,
                                qx = rot.x or 0,
                                qy = rot.y or 0,
                                qz = rot.z or 0
                            }
                            
                            if self.debugMode then
                                local pitch, yaw, roll = quatToEuler(targetRot.qw, targetRot.qx, targetRot.qy, targetRot.qz)
                                print(string.format("[CinematicCamera] Rotation (quat): w=%.3f, x=%.3f, y=%.3f, z=%.3f", 
                                    targetRot.qw, targetRot.qx, targetRot.qy, targetRot.qz))
                                print(string.format("[CinematicCamera] Rotation (euler): y=%.1f, x=%.1f, z=%.1f", 
                                    pitch, yaw, roll))
                            end
                        end
                    end
                    
                    
                    if not targetRot and self.debugMode then
                        print("[CinematicCamera] WARNING: Could not extract rotation from transform!")
                    end
                else
                    print(string.format("[CinematicCamera] Transform '%s' NOT FOUND!", cleanName))
                end
            end
        end
        
        -- Fallback to manual position
        if not targetPos then
            targetPos = {
                x = self.targetPosition.x or 0,
                y = self.targetPosition.y or 0,
                z = self.targetPosition.z or 0
            }
        end
        
        -- Fallback to manual rotation
        if not targetRot then
            if self.debugMode then
                print(string.format("[CinematicCamera] Using manual rotation: x=%.1f, y=%.1f, z=%.1f", 
                    self.targetRotation.x, self.targetRotation.y, self.targetRotation.z))
            end
            local quat = eulerToQuat(
                self.targetRotation.x or 0,
                self.targetRotation.y or 0,
                self.targetRotation.z or 0
            )
            targetRot = {qw = quat.w, qx = quat.x, qy = quat.y, qz = quat.z}
        end

        -- Debug print position and Rotation
        if self.debugMode then
           print(string.format("[CinematicCamera] Target Pos: (%.2f, %.2f, %.2f)", 
                targetPos.x, targetPos.y, targetPos.z))
           print(string.format("[CinematicCamera] Target Rot (quat): w=%.3f, x=%.3f, y=%.3f, z=%.3f", 
                targetRot.qw, targetRot.qx, targetRot.qy, targetRot.qz))
        end
        
        -- Publish to camera
        if event_bus and event_bus.publish then
            event_bus.publish("cinematic.target", {
                position = targetPos,
                rotation = targetRot,
                transitionSpeed = self.transitionSpeed
            })
            
            if self.debugMode and targetRot then
                local pitch, yaw, roll = quatToEuler(targetRot.qw, targetRot.qx, targetRot.qy, targetRot.qz)
                print(string.format("[CinematicCamera] Publishing - Pos: (%.2f, %.2f, %.2f), Euler: (%.1f, %.1f, %.1f)", 
                    targetPos.x, targetPos.y, targetPos.z, pitch, yaw, roll))
            end
        end
    end,
}