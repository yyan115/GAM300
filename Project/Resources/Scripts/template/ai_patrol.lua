local Component = require("mono_helper")
local FSM = require("simple_fsm")
return Component {
    fields = { name = "Patrol", speed = 1.0, points = { {x=0,y=0},{x=5,y=0} }, idx = 1 },
    Start = function(self)
        local def = {
            move = {
                on_enter = function(s) s.target = self.points[self.idx] end,
                update = function(s, dt)
                    local p = self.position or {x=0,y=0}
                    local t = s.target or {x=0,y=0}
                    local dx = t.x - p.x
                    local dy = t.y - p.y
                    local dist = math.sqrt(dx*dx + dy*dy)
                    if dist < 0.1 then
                        self.idx = (self.idx % #self.points) + 1
                        s:change("move")
                    else
                        self.position = self.position or {x=0,y=0}
                        self.position.x = self.position.x + (dx/dist) * (self.speed or 1) * dt
                        self.position.y = self.position.y + (dy/dist) * (self.speed or 1) * dt
                    end
                end
            }
        }
        self._fsm = FSM.new(def, "move")
    end,
    Update = function(self, dt)
        if self._fsm then self._fsm:update(dt) end
    end
}
