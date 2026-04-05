-- simple_fsm.lua
-- Usage:
-- local fsm = FSM.new({ idle = { on_enter = ..., update = ... }, walk = { ... } }, "idle")
local FSM = {}

function FSM.new(defs, start)
    local self = { state = start or next(defs), defs = defs or {}, t = 0 }
    function self:change(s)
        if self.state == s then return end
        local old = self.state
        if self.defs[old] and self.defs[old].on_exit then pcall(self.defs[old].on_exit, self) end
        self.state = s
        self.t = 0
        if self.defs[s] and self.defs[s].on_enter then pcall(self.defs[s].on_enter, self) end
    end
    function self:update(dt)
        self.t = self.t + (dt or 0)
        if self.defs[self.state] and self.defs[self.state].update then pcall(self.defs[self.state].update, self, dt) end
    end
    function self:send(name, payload)
        if self.defs[self.state] and self.defs[self.state][name] then pcall(self.defs[self.state][name], self, payload) end
    end
    return self
end

return FSM
