require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    Button = {
    x = 250,
    y = 620,
    width = 10,
    height = 10,

    isHovering = false,

    -- Replace with your engine's component toggle functions
    EnableHoverComponent = function(self)
        print("Hover ON")
    end,

    DisableHoverComponent = function(self)
        print("Hover OFF")
    end
}
    
function Button:IsMouseInside(mx, my)
    return mx >= self.x and mx <= self.x + self.width and
           my >= self.y and my <= self.y + self.height
end
           
function Button:Update(dt)
    -- Replace with engine API call
    local mx, my = Input.GetMousePosition()      

    local currentlyHovering = self:IsMouseInside(mx, my)

    if currentlyHovering and not self.isHovering then
        -- Mouse just entered
        self.isHovering = true
        self:EnableHoverComponent()

    elseif not currentlyHovering and self.isHovering then
        -- Mouse just exited
        self.isHovering = false
        self:DisableHoverComponent()
    end
end
}
