local Component = require("mono_helper")
return Component {
    fields = {
        name = "Mover",
        speed = 1.5,
        position = { x = 0, y = 0, z = 0 }
    },
    Update = function(self, dt)
        self.position.x = self.position.x + (dt * (self.speed or 0))
    end
}
