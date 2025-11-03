-- ui_helpers.lua
local UI = {}

function UI.show_tooltip(text)
    if cpp_log then cpp_log("[UI Tooltip] "..tostring(text)) else print("[UI Tooltip]", text) end
end

function UI.show_progress(id, fraction)
    if cpp_log then cpp_log(string.format("[UI Progress] %s %.2f", tostring(id), tonumber(fraction) or 0)) else print("UI Progress", id, fraction) end
end

return UI
