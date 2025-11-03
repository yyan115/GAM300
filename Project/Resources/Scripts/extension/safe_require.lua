-- safe_require.lua
-- Simple safe wrapper for require. usage: local ok, mod = safe_require("name")

local function safe_require(name, no_throw)
    local ok, mod = pcall(require, name)
    if not ok then
        if cpp_log then cpp_log(string.format("safe_require: require('%s') failed: %s", tostring(name), tostring(mod)))
        else print("safe_require:", name, mod) end
        if no_throw then return nil, tostring(mod) end
        return nil, tostring(mod)
    end
    return mod, nil
end

local function reload(name)
    if package and package.loaded and package.loaded[name] then package.loaded[name] = nil end
    return safe_require(name, true)
end

return { require = safe_require, reload = reload }
