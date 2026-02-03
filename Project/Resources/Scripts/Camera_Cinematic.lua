-- CinematicCamera.lua 
-- Strips quotes from names and handles errors safely

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
        debugMode = true,
    },

    Awake = function(self)
        self._wasCinematicActive = false
        self._currentWaypointIndex = 1
        self._waypointTimer = 0.0
        
        print("[CinematicCamera] Initialized")
    end,

    Update = function(self, dt)
        if self.cinematicActive ~= self._wasCinematicActive then
            self._wasCinematicActive = self.cinematicActive
            
            if self.cinematicActive then
                print("[CinematicCamera] Cinematic mode ENABLED")
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
                print(string.format("[CinematicCamera] Searching for: '%s' (cleaned from: '%s')", 
                                  cleanName, self.targetTransformName))
            end
            
            if Engine and Engine.FindTransformByName then
                local targetTransform = Engine.FindTransformByName(cleanName)
                
                if targetTransform then
                    if self.debugMode then
                        print("[CinematicCamera] Transform FOUND!")
                    end
                    
                    if Engine.GetTransformWorldPosition then
                        print("[CinematicCamera] Getting position...")
    
                        -- GetTransformWorldPosition returns a TABLE (because C++ returns std::tuple)
                        local positionTable = Engine.GetTransformWorldPosition(targetTransform)
    
                        print(string.format("[CinematicCamera] Position table: %s (type: %s)", 
                                          tostring(positionTable), type(positionTable)))
    
                        if positionTable and type(positionTable) == "table" then
                            -- Try different table formats
                            local x = positionTable[1] or positionTable.x or positionTable._1
                            local y = positionTable[2] or positionTable.y or positionTable._2
                            local z = positionTable[3] or positionTable.z or positionTable._3
        
                            print(string.format("[CinematicCamera] Extracted: x=%s, y=%s, z=%s", 
                                              tostring(x), tostring(y), tostring(z)))
        
                            if x and y and z then
                                targetPos = {x = x, y = y, z = z}
                                if self.debugMode then
                                    print(string.format("[CinematicCamera] Position: (%.2f, %.2f, %.2f)", x, y, z))
                                end
                            else
                                print("[CinematicCamera] Could not extract x,y,z from table")
            
                                -- Debug: Print all table contents
                                print("[CinematicCamera] Table contents:")
                                for k, v in pairs(positionTable) do
                                    print(string.format("  [%s] = %s", tostring(k), tostring(v)))
                                end
                            end
                        else
                            print("[CinematicCamera] GetTransformWorldPosition did not return a table!")
                        end
                    end
                    
                    if Engine.GetTransformWorldRotation then
                        local rotationTable = Engine.GetTransformWorldRotation(targetTransform)
    
                        if rotationTable and type(rotationTable) == "table" then
                            -- std::tuple might be indexed as [1], [2], [3], [4]
                            -- or as _1, _2, _3, _4
                            -- or as w, x, y, z
                            local qw = rotationTable[1] or rotationTable.w or rotationTable._1
                            local qx = rotationTable[2] or rotationTable.x or rotationTable._2
                            local qy = rotationTable[3] or rotationTable.y or rotationTable._3
                            local qz = rotationTable[4] or rotationTable.z or rotationTable._4
        
                            if qw and qx and qy and qz then
                                targetRot = {qw = qw, qx = qx, qy = qy, qz = qz}
                                if self.debugMode then
                                    print(string.format("[CinematicCamera] Rotation: (%.2f, %.2f, %.2f, %.2f)", qw, qx, qy, qz))
                                end
                            end
                        end
                    end
                else
                    print(string.format("[CinematicCamera] Transform '%s' NOT FOUND!", cleanName))
                end
            end
        end
        
        -- Fallback to manual
        if not targetPos then
            targetPos = {
                x = self.targetPosition.x or 0,
                y = self.targetPosition.y or 0,
                z = self.targetPosition.z or 0
            }
        end
        
        if not targetRot then
            local quat = eulerToQuat(
                self.targetRotation.x or 0,
                self.targetRotation.y or 0,
                self.targetRotation.z or 0
            )
            targetRot = {qw = quat.w, qx = quat.x, qy = quat.y, qz = quat.z}
        end
        
        -- Publish to camera
        if event_bus and event_bus.publish then
            event_bus.publish("cinematic.target", {
                position = targetPos,
                rotation = targetRot,
                transitionSpeed = self.transitionSpeed
            })
        end
    end,
}