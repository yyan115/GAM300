require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },

    fields = {
        x = 250,
        y = 620,
        width = 10,
        height = 10,
    },

    Awake = function(self)
        self._isHovering = false
    end,

    _isMouseInside = function(self, mx, my)
        return mx >= self.x and mx <= self.x + self.width and
               my >= self.y and my <= self.y + self.height
    end,

    _enableHoverComponent = function(self)
        print("Hover ON")
    end,

    _disableHoverComponent = function(self)
        print("Hover OFF")
    end,

    Update = function(self, dt)
        local mx = Input and Input.GetMouseX and Input.GetMouseX() or 0
        local my = Input and Input.GetMouseY and Input.GetMouseY() or 0

        local currentlyHovering = self:_isMouseInside(mx, my)

        if currentlyHovering and not self._isHovering then
            self._isHovering = true
            self:_enableHoverComponent()
        elseif not currentlyHovering and self._isHovering then
            self._isHovering = false
            self:_disableHoverComponent()
        end
    end,
}
