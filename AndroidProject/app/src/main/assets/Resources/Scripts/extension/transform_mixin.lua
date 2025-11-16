-- transform_mixin.lua
-- Provides automatic Transform component management for scripts
-- Handles caching, fetching, and dirty flag management transparently

local M = {}

-- Safe tostring for Vector3D-like objects
local function vec3_str(v)
    if v == nil then return "nil" end
    local t = type(v)
    if t == "table" or t == "userdata" then
        local x = v.x or 0
        local y = v.y or 0
        local z = v.z or 0
        return string.format("(%.2f, %.2f, %.2f)", x, y, z)
    end
    return tostring(v)
end

-- Helper to create a Vector3D (checks if constructor is available)
local function make_vec3(x, y, z)
    if _G.Vector3D then
        return _G.Vector3D.new and _G.Vector3D.new(x, y, z) or _G.Vector3D()
    end
    -- Fallback: return table (for systems without Vector3D constructor)
    return { x = x, y = y, z = z }
end

-- Helper to create a Quaternion
local function make_quat(w, x, y, z)
    if _G.Quaternion then
        return _G.Quaternion.new and _G.Quaternion.new(w, x, y, z) or _G.Quaternion()
    end
    -- Fallback: return table
    return { w = w, x = x, y = y, z = z }
end

-- Apply the Transform mixin to an instance
function M.apply(instance)
    -- Private cache for transform component
    instance._transform_cache = nil
    instance._transform_fetched = false
    
    -- Get Transform component (cached)
    function instance:GetTransform()
        if self._transform_fetched then
            return self._transform_cache
        end
        
        if self.GetComponent then
            self._transform_cache = self:GetComponent("Transform")
            self._transform_fetched = true
        end
        
        return self._transform_cache
    end
    
    -- Move the entity by a delta vector
    function instance:Move(dx, dy, dz)
        local t = self:GetTransform()
        if not t then
            print("[Transform] Warning: No Transform component available")
            return false
        end
        
        local pos = t.localPosition
        if not pos then
            print("[Transform] Warning: Transform has no localPosition")
            return false
        end
        
        -- Calculate new position
        local newX = (pos.x or 0) + (dx or 0)
        local newY = (pos.y or 0) + (dy or 0)
        local newZ = (pos.z or 0) + (dz or 0)
        
        -- Check if we can modify in-place (userdata) or need to reassign
        local posType = type(pos)
        if posType == "userdata" then
            -- Modify userdata in-place (LuaBridge allows this)
            pos.x = newX
            pos.y = newY
            pos.z = newZ
        else
            -- For tables, create new Vector3D or table
            t.localPosition = make_vec3(newX, newY, newZ)
        end
        
        t.isDirty = true
        return true
    end
    
    -- Set absolute position
    function instance:SetPosition(x, y, z)
        local t = self:GetTransform()
        if not t then
            print("[Transform] Warning: No Transform component available")
            return false
        end
        
        -- Check current type
        local pos = t.localPosition
        if type(pos) == "userdata" then
            -- Modify in-place
            pos.x = x or 0
            pos.y = y or 0
            pos.z = z or 0
        else
            -- Create new object
            t.localPosition = make_vec3(x or 0, y or 0, z or 0)
        end
        
        t.isDirty = true
        return true
    end
    
    -- Get current position (returns x, y, z or nil)
    function instance:GetPosition()
        local t = self:GetTransform()
        if not t or not t.localPosition then return nil end
        
        local pos = t.localPosition
        return pos.x or 0, pos.y or 0, pos.z or 0
    end
    
    -- Get current rotation (returns w, x, y, z or nil)
    function instance:GetRotation()
        local t = self:GetTransform()
        if not t or not t.localRotation then return nil end
        
        local rot = t.localRotation
        return rot.w or 1, rot.x or 0, rot.y or 0, rot.z or 0
    end
    
    -- Set rotation (quaternion: w, x, y, z)
    function instance:SetRotation(w, x, y, z)
        local t = self:GetTransform()
        if not t then 
            print("[Transform] Warning: No Transform component available")
            return false 
        end
        
        local rot = t.localRotation
        if type(rot) == "userdata" then
            -- Modify in-place
            rot.w = w or 1
            rot.x = x or 0
            rot.y = y or 0
            rot.z = z or 0
        else
            -- Create new object
            t.localRotation = make_quat(w or 1, x or 0, y or 0, z or 0)
        end
        
        t.isDirty = true
        return true
    end
    
    -- Get current scale (returns x, y, z or nil)
    function instance:GetScale()
        local t = self:GetTransform()
        if not t or not t.localScale then return nil end
        
        local scale = t.localScale
        return scale.x or 1, scale.y or 1, scale.z or 1
    end
    
    -- Set scale
    function instance:SetScale(x, y, z)
        local t = self:GetTransform()
        if not t then 
            print("[Transform] Warning: No Transform component available")
            return false 
        end
        
        -- If only one parameter, use uniform scale
        if y == nil and z == nil then
            y, z = x, x
        end
        
        local scale = t.localScale
        if type(scale) == "userdata" then
            -- Modify in-place
            scale.x = x or 1
            scale.y = y or 1
            scale.z = z or 1
        else
            -- Create new object
            t.localScale = make_vec3(x or 1, y or 1, z or 1)
        end
        
        t.isDirty = true
        return true
    end
    
    -- Debug: print current transform state
    function instance:DebugTransform()
        local t = self:GetTransform()
        if not t then
            print("[Transform] No transform available")
            return
        end
        
        print("[Transform] Position: " .. vec3_str(t.localPosition))
        print("[Transform] Rotation: " .. vec3_str(t.localRotation))
        print("[Transform] Scale: " .. vec3_str(t.localScale))
        print("[Transform] isDirty: " .. tostring(t.isDirty))
    end
end

return M