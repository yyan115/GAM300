-- inspector_meta.lua
-- Utility helper to build editor metadata structures.

local IM = {}

function IM.number(minv, maxv, step)
    return { type = "number", min = minv, max = maxv, step = step }
end

function IM.boolean()
    return { type = "boolean" }
end

function IM.string()
    return { type = "string" }
end

function IM.asset(assetType)
    return { type = "asset", assetType = assetType }
end

function IM.enum(list)
    return { type = "enum", choices = list }
end

return IM
