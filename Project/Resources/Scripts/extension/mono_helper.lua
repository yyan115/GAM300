-- mono_helper.lua
-- Improved helper for authoring component-like instances.
-- Usage:
--   local Component = require("mono_helper")
--   return Component {
--       fields = { name = "Bob", health = 100 },
--       meta = { health = { type="number", min=0, max=100 } },
--       usePrototype = false, -- optional: if true, methods remain on spec and instance __index -> spec
--       Start = function(self) end,
--       Update = function(self, dt) end
--   }

local M = {}

_G.__instances = _G.__instances or {}

local event_bus = rawget(_G, "event_bus")
local scheduler = rawget(_G, "scheduler")

-- local logger helper: prefer cpp_log if available
local function log_info(fmt, ...)
    local ok, s = pcall(string.format, fmt, ...)
    if not ok then s = fmt end
    if cpp_log then cpp_log("[mono_helper] " .. s) else print("[mono_helper] " .. s) end
end
local function log_warn(fmt, ...)
    local ok, s = pcall(string.format, fmt, ...)
    if not ok then s = fmt end
    if cpp_log then cpp_log("[mono_helper:WARN] " .. s) else io.stderr:write("[mono_helper:WARN] " .. s .. "\n") end
end
local function log_error(fmt, ...)
    local ok, s = pcall(string.format, fmt, ...)
    if not ok then s = fmt end
    if cpp_log then cpp_log("[mono_helper:ERROR] " .. s) else io.stderr:write("[mono_helper:ERROR] " .. s .. "\n") end
end

-- Safe pcall wrapper; returns (true) on success or (false, err) on failure.
local function safe_call(fn, ...)
    if type(fn) ~= "function" then
        return true
    end
    local ok, err = pcall(fn, ...)
    if not ok then
        log_error("error in callback: %s", tostring(err))
    end
    return ok, err
end

local function shallow_copy(t)
    local out = {}
    if not t then return out end
    for k, v in pairs(t) do out[k] = v end
    return out
end

-- Return constructor
return function(spec)
    spec = spec or {}

    -- configuration
    local usePrototype = spec.usePrototype or false

    -- instance table
    local inst = {}

    -- fields + meta
    inst.__field_list = shallow_copy(spec.fields or {})
    for k, v in pairs(inst.__field_list) do
        inst[k] = v
    end
    inst.__meta = spec.meta or {}

    -- private state
    inst._private = inst._private or {}
    inst._subscriptions = inst._subscriptions or {}
    inst.__created_at = os.time()

    -- optionally copy lifecycle methods (or leave on spec if usePrototype)
    if not usePrototype then
        for k, v in pairs(spec) do
            if k ~= "fields" and k ~= "meta" and k ~= "usePrototype" then
                if type(v) == "function" then
                    inst[k] = v
                end
            end
        end
    else
        -- set prototype via metatable so instance will fallback to spec for methods/fields not present
        setmetatable(inst, { __index = spec })
    end

    -- Subscribe / Unsubscribe helpers
    function inst:Subscribe(eventName, callback)
        if not event_bus or type(event_bus.subscribe) ~= "function" then
            log_warn("Subscribe: no event_bus available")
            return nil
        end
        local token = event_bus.subscribe(eventName, callback)
        table.insert(self._subscriptions, token)
        return token
    end

    function inst:Unsubscribe(token)
        if not event_bus or type(event_bus.unsubscribe) ~= "function" then return end
        pcall(event_bus.unsubscribe, token)
        for i = #self._subscriptions, 1, -1 do
            if self._subscriptions[i] == token then table.remove(self._subscriptions, i) end
        end
    end

    -- Timer helpers (use scheduler when available)
    function inst:After(seconds, fn)
        if not scheduler or type(scheduler.after) ~= "function" then
            log_warn("After: scheduler not installed")
            return nil
        end
        return scheduler.after(seconds or 0, function() safe_call(fn) end)
    end

    function inst:Every(interval, fn)
        if not scheduler or type(scheduler.every) ~= "function" then
            log_warn("Every: scheduler not installed")
            return nil
        end
        return scheduler.every(interval, fn)
    end

    -- Destroy / cleanup
    function inst:Destroy()
        -- call OnDisable if present
        if self.OnDisable then
            safe_call(self.OnDisable, self)
        else
            -- allow prototype methods to be called too (if usePrototype)
            if type(self.OnDisable) == "nil" and usePrototype and type(spec.OnDisable) == "function" then
                safe_call(spec.OnDisable, self)
            end
        end

        -- unsubscribe
        if event_bus and type(event_bus.unsubscribe) == "function" then
            for _, token in ipairs(self._subscriptions) do
                pcall(event_bus.unsubscribe, token)
            end
            self._subscriptions = {}
        end

        -- remove from global instances
        if _G.__instances then
            for i = #_G.__instances, 1, -1 do
                if _G.__instances[i] == self then table.remove(_G.__instances, i) end
            end
        end

        -- call OnDestroy if present (safe)
        if self.OnDestroy then
            safe_call(self.OnDestroy, self)
        else
            if usePrototype and type(spec.OnDestroy) == "function" then
                safe_call(spec.OnDestroy, self)
            end
        end

        -- cancel scheduler handles if stored (common pattern)
        if self._everyId and scheduler and type(scheduler.cancel) == "function" then
            pcall(scheduler.cancel, self._everyId)
            self._everyId = nil
        end
    end

    -- Lifecycle invocation helper:
    -- 1) prefer rawget(self, name) (preserve semantics for raw fields)
    -- 2) otherwise perform normal lookup (so __index / prototype works)
    -- This combination preserves older behavior while supporting prototypes.
    function inst:_call_lifecycle(name, ...)
        local fn = rawget(self, name)
        if type(fn) == "function" then
            return safe_call(fn, self, ...)
        end
        -- fallback to normal lookup (respect metatable __index)
        local alt = self[name]
        if type(alt) == "function" then
            return safe_call(alt, self, ...)
        end
        return true
    end

    -- explicit lifecycle wrappers (safe)
    function inst:AwakeSafe(...)    return self:_call_lifecycle("Awake", ...) end
    function inst:StartSafe(...)    return self:_call_lifecycle("Start", ...) end
    function inst:UpdateSafe(dt)    return self:_call_lifecycle("Update", dt) end
    function inst:DisableSafe(...)  return self:_call_lifecycle("OnDisable", ...) end

    -- hot-reload helper
    function inst:OnHotReload()
        if type(self.on_reload) == "function" then
            safe_call(self.on_reload, self)
            return
        end
        if usePrototype and type(spec.on_reload) == "function" then
            safe_call(spec.on_reload, self)
        end
    end

    -- Serialization helpers (shallow: only registered __field_list keys)
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

    -- Safe require wrapper
    function inst:Require(name)
        local ok, mod = pcall(require, name)
        if not ok then
            log_warn("require(%s) failed: %s", tostring(name), tostring(mod))
            return nil
        end
        return mod
    end

    -- utility
    function inst:ToString()
        return string.format("<Component created=%d>", tonumber(self.__created_at) or 0)
    end

    -- register in global instances list
    table.insert(_G.__instances, inst)

    -- call Awake safely on creation (matches prior behavior)
    inst:AwakeSafe()

    return inst
end
