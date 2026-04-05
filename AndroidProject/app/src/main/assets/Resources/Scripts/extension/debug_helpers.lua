-- debug_helpers.lua
local D = {}

local function repr(val, indent, visited)
    indent = indent or 0
    visited = visited or {}
    local pad = string.rep("  ", indent)
    if type(val) == "table" then
        if visited[val] then return "<cycle>" end
        visited[val] = true
        local s = "{\n"
        for k, v in pairs(val) do
            s = s .. pad .. "  ["..tostring(k).."] = " .. repr(v, indent + 1, visited) .. ",\n"
        end
        s = s .. pad .. "}"
        return s
    else
        return tostring(val)
    end
end

function D.pp(t)
    if cpp_log then cpp_log(repr(t)) else print(repr(t)) end
end

return D
