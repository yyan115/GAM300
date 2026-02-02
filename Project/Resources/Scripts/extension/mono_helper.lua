-- mono_helper.lua (Refactored)
-- Simplified component authoring with automatic mixin support

local M = {}

_G.__instances = _G.__instances or {}

local event_bus = rawget(_G, "event_bus")
local scheduler = rawget(_G, "scheduler")

-- Logger helpers
local function log_info(msg) 
    if cpp_log then cpp_log("[Component] " .. msg) else print("[Component] " .. msg) end 
end

local function log_error(msg)
    if cpp_log then cpp_log("[Component:ERROR] " .. msg) else io.stderr:write("[Component:ERROR] " .. msg .. "\n") end
end

-- Safe call wrapper
local function safe_call(fn, ...)
    if type(fn) ~= "function" then return true end
    local ok, err = pcall(fn, ...)
    if not ok then log_error("Callback error: " .. tostring(err)) end
    return ok, err
end

-- Shallow copy
local function shallow_copy(t)
    local out = {}
    if not t then return out end
    for k, v in pairs(t) do out[k] = v end
    return out
end

-- Component constructor
return function(spec)
    spec = spec or {}
    
    local inst = {}
    
    -- SAFELY preserve entityId if it was already set by C++
    -- Check if this is being called as a result of C++ AttachScript
    local existing_entityId = nil
    if spec.entityId then
        existing_entityId = spec.entityId
    end

    -- Copy fields into instance
    inst.__field_list = shallow_copy(spec.fields or {})
    for k, v in pairs(inst.__field_list) do
        inst[k] = v
    end
    
    -- Restore entityId if it existed
    if existing_entityId then
        inst.entityId = existing_entityId
    end

    inst.__meta = spec.meta or {}
    inst._private = {}
    inst._subscriptions = {}
    inst.__created_at = os.time()
    
    -- Copy lifecycle methods and custom functions
    for k, v in pairs(spec) do
        if k ~= "fields" and k ~= "meta" and k ~= "mixins" then
            if type(v) == "function" then
                inst[k] = v
            end
        end
    end
    
    -- Apply mixins (modules that add functionality)
    if spec.mixins then
        for _, mixin in ipairs(spec.mixins) do
            if type(mixin) == "table" and type(mixin.apply) == "function" then
                mixin.apply(inst)
            elseif type(mixin) == "function" then
                mixin(inst)
            end
        end
    end
    
    -- Event subscription
    function inst:Subscribe(eventName, callback)
        if not event_bus or type(event_bus.subscribe) ~= "function" then
            log_error("Subscribe: event_bus not available")
            return nil
        end
        local token = event_bus.subscribe(eventName, callback)
        table.insert(self._subscriptions, token)
        return token
    end
    
    function inst:Unsubscribe(token)
        if not event_bus then return end
        pcall(event_bus.unsubscribe, token)
        for i = #self._subscriptions, 1, -1 do
            if self._subscriptions[i] == token then 
                table.remove(self._subscriptions, i) 
            end
        end
    end
    
    -- Timer helpers
    function inst:After(seconds, fn)
        if not scheduler or type(scheduler.after) ~= "function" then
            log_error("After: scheduler not available")
            return nil
        end
        return scheduler.after(seconds or 0, fn)
    end
    
    function inst:Every(interval, fn)
        if not scheduler or type(scheduler.every) ~= "function" then
            log_error("Every: scheduler not available")
            return nil
        end
        return scheduler.every(interval, fn)
    end
    
    -- Lifecycle wrapper (safe invocation)
    function inst:_call_lifecycle(name, ...)
        local fn = rawget(self, name)
        if type(fn) == "function" then
            return safe_call(fn, self, ...)
        end
        return true
    end
    
    function inst:AwakeSafe(...)    return self:_call_lifecycle("Awake", ...) end
    function inst:StartSafe(...)    return self:_call_lifecycle("Start", ...) end
    function inst:UpdateSafe(dt)    return self:_call_lifecycle("Update", dt) end
    function inst:DisableSafe(...)  return self:_call_lifecycle("OnDisable", ...) end
    
    -- Serialization (only registered fields)
    function inst:SerializeFields()
        local out = {}
        for k, _ in pairs(self.__field_list or {}) do
            out[k] = self[k]
        end
        return out
    end
    
    function inst:DeserializeFields(tbl)
        if type(tbl) ~= "table" then return end
        for k, _ in pairs(self.__field_list or {}) do
            if tbl[k] ~= nil then self[k] = tbl[k] end
        end
    end
    
    -- Cleanup
    function inst:Destroy()
        safe_call(self.OnDisable, self)
        
        -- Unsubscribe all events
        if event_bus then
            for _, token in ipairs(self._subscriptions) do
                pcall(event_bus.unsubscribe, token)
            end
            self._subscriptions = {}
        end
        
        -- Remove from global instances
        if _G.__instances then
            for i = #_G.__instances, 1, -1 do
                if _G.__instances[i] == self then 
                    table.remove(_G.__instances, i) 
                end
            end
        end
        
        safe_call(self.OnDestroy, self)
    end
    
    -- Register instance globally
    table.insert(_G.__instances, inst)
    
    -- Call Awake on creation
    inst:AwakeSafe()
    
    return inst
end