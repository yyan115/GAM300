-- mono_helper.lua
-- Minimal, safe helper for authoring components with fields+simple methods.
-- Usage:
--   local Component = require("mono_helper")
--   return Component {
--      fields = { name = "Bob", health = 100 },
--      meta = { health = { type="number", min=0, max=100 } },
--      Start = function(self) end,
--      Update = function(self, dt) end
--   }

local M = {}

_G.__instances = _G.__instances or {}

local event_bus = rawget(_G, "event_bus")
local scheduler = rawget(_G, "scheduler")

local function log(fmt, ...)
    local s = string.format(fmt, ...)
    if cpp_log then cpp_log(s) else print(s) end
end

local function safe_call(fn, ...)
    if type(fn) ~= "function" then return true end
    local ok, err = pcall(fn, ...)
    if not ok then
        log("[mono_helper] error in callback: %s", tostring(err))
    end
    return ok
end

local function shallow_copy(t)
    local out = {}
    if not t then return out end
    for k, v in pairs(t) do out[k] = v end
    return out
end

return function(spec)
    spec = spec or {}
    local inst = {}

    inst.__field_list = shallow_copy(spec.fields or {})
    for k,v in pairs(inst.__field_list) do inst[k] = v end
    inst.__meta = spec.meta or {}
    inst._private = inst._private or {}
    inst._subscriptions = {}
    inst.__created_at = os.time()

    function inst:Subscribe(eventName, callback)
        if not event_bus or type(event_bus.subscribe) ~= "function" then
            log("[mono_helper] Subscribe: no event_bus available")
            return
        end
        local token = event_bus.subscribe(eventName, callback)
        table.insert(self._subscriptions, token)
        return token
    end

    function inst:Unsubscribe(token)
        if not event_bus or type(event_bus.unsubscribe) ~= "function" then return end
        event_bus.unsubscribe(token)
        for i=#self._subscriptions,1,-1 do
            if self._subscriptions[i] == token then table.remove(self._subscriptions, i) end
        end
    end

    function inst:After(seconds, fn)
        if not scheduler or type(scheduler.after) ~= "function" then
            -- fallback: naive coroutine timer that requires scheduler.tick to run each frame
            local id = scheduler.after(seconds or 0, fn)
            return id
        end
        return scheduler.after(seconds, fn)
    end

    function inst:Every(interval, fn)
        if not scheduler or type(scheduler.every) ~= "function" then
            log("[mono_helper] Every: scheduler not installed")
            return nil
        end
        return scheduler.every(interval, fn)
    end

    function inst:Destroy()
        safe_call(self.OnDisable, self)
        if event_bus and type(event_bus.unsubscribe) == "function" then
            for _, token in ipairs(self._subscriptions) do event_bus.unsubscribe(token) end
            self._subscriptions = {}
        end
        if _G.__instances then
            for i = #_G.__instances, 1, -1 do
                if _G.__instances[i] == self then table.remove(_G.__instances, i) end
            end
        end
        safe_call(self.OnDestroy, self)
    end

    function inst:_call_lifecycle(name, ...)
        local fn = rawget(self, name)
        if type(fn) == "function" then
            return safe_call(fn, self, ...)
        end
        return true
    end

    function inst:OnHotReload()
        if type(self.on_reload) == "function" then
            safe_call(self.on_reload, self)
        end
    end

    function inst:SerializeFields()
        local out = {}
        for k,_ in pairs(self.__field_list or {}) do
            out[k] = self[k]
        end
        return out
    end

    function inst:AwakeSafe()    self:_call_lifecycle("Awake") end
    function inst:StartSafe()    self:_call_lifecycle("Start") end
    function inst:UpdateSafe(dt) self:_call_lifecycle("Update", dt) end
    function inst:DisableSafe()  self:_call_lifecycle("OnDisable") end

    function inst:Require(name)
        local ok, mod = pcall(require, name)
        if not ok then
            log("[mono_helper] require(%s) failed: %s", name, tostring(mod))
            return nil
        end
        return mod
    end

    table.insert(_G.__instances, inst)
    inst:AwakeSafe()
    return inst
end
