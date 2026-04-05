local Component = require("mono_helper")
return Component {
    fields = { name = "PickupListener" },
    Start = function(self)
        self:Subscribe("item_picked", function(ev)
            if ev and ev.item then
                if cpp_log then cpp_log("Picked: "..tostring(ev.item)) else print("Picked", ev.item) end
            end
        end)
    end
}
