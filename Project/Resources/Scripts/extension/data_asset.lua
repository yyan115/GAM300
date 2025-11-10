-- data_asset.lua
-- Loads a data asset that is a Lua chunk returning a table, or JSON if cjson available.

local DA = {}

function DA.load_lua(path)
    -- path should be resolvable by module loader; we use require-like semantics
    local ok, chunkOrErr = pcall(function() return loadfile(path) end)
    if not ok or not chunkOrErr then
        return nil, chunkOrErr
    end
    local ok2, res = pcall(chunkOrErr)
    if not ok2 then return nil, res end
    return res, nil
end

function DA.load_json_string(str)
    if type(str) ~= "string" then return nil, "not a string" end
    if rawget(_G, "cjson") then
        local ok, res = pcall(function() return cjson.decode(str) end)
        if not ok then return nil, res end
        return res, nil
    else
        return nil, "no json library (cjson) available"
    end
end

return DA
