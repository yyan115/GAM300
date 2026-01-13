-- Resources/Scripts/Gameplay/StateMachine.lua
local StateMachine = {}
StateMachine.__index = StateMachine

function StateMachine.new(owner)
    return setmetatable({
        owner = owner,
        current = nil,        -- state module
        currentName = nil,    -- string key
        timeInState = 0,
        enabled = true,
    }, StateMachine)
end

function StateMachine:Change(stateName, stateModule)
    if not self.enabled then return end
    if self.currentName == stateName then return end

    local owner = self.owner

    if self.current and self.current.Exit then
        self.current:Exit(owner)
    end

    self.current = stateModule
    self.currentName = stateName
    self.timeInState = 0

    if self.current and self.current.Enter then
        self.current:Enter(owner)
    end
end

function StateMachine:ForceChange(stateName, stateModule)
    if not self.enabled then return end

    local owner = self.owner

    if self.current and self.current.Exit then
        self.current:Exit(owner)
    end

    self.current = stateModule
    self.currentName = stateName
    self.timeInState = 0

    if self.current and self.current.Enter then
        self.current:Enter(owner)
    end
end

function StateMachine:Update(dt)
    if not self.enabled or not self.current then return end

    self.timeInState = self.timeInState + dt

    -- 🔎 DEBUG: always show active state
    print(string.format(
        "[FSM] State=%s  Time=%.2f",
        tostring(self.currentName),
        self.timeInState
    ))

    if self.current.Update then
        self.current:Update(self.owner, dt)
    end
end


return StateMachine
