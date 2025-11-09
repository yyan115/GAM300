local Component = require("mono_helper")
local IM = require("inspector_meta")
return Component {
    fields = {
        name = "Chest",
        locked = true,
        items = { "gold", "ruby" }
    },
    meta = {
        locked = IM.boolean(),
        items = IM.string()
    }
}
