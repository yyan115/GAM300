require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

return Component {
    fields = {
        moveSpriteEntityName = "MovementControlsSprite",
        dashSpriteEntityName = "DashControlsSprite",

        fadeDuration         = 0.5,
        startDelay           = 1.0,
        dismissDelay         = 1.0,

        dashDisplayTime      = 3.0, -- NEW: how long dash tooltip stays visible
    },

    Start = function(self)
        self._phase       = "wait_start"
        self._timer       = 0
        self._moveSprite  = nil
        self._dashSprite  = nil
        self._moveEnt     = nil
        self._dashEnt     = nil

        -- self._dashDetected = false

        --[[  COMMENTED OUT: dash detection event
        if event_bus and event_bus.subscribe then
            self._dashSub = event_bus.subscribe("dash_executed", function()
                self._dashDetected = true
            end)
        end
        ]]

        local moveEnt = Engine.GetEntityByName(self.moveSpriteEntityName)
        if moveEnt then
            self._moveEnt = moveEnt
            local activeComp = GetComponent(moveEnt, "ActiveComponent")
            if activeComp then activeComp.isActive = true end
            self._moveSprite = GetComponent(moveEnt, "SpriteRenderComponent")
            if self._moveSprite then
                self._moveSprite.alpha = 0.0
                self._moveSprite.isVisible = false
            end
        end

        local dashEnt = Engine.GetEntityByName(self.dashSpriteEntityName)
        if dashEnt then
            self._dashEnt = dashEnt
            local activeComp = GetComponent(dashEnt, "ActiveComponent")
            if activeComp then activeComp.isActive = false end
        end
    end,

    Update = function(self, dt)
        local phase = self._phase
        if phase == "done" then return end

        self._timer = self._timer + dt

        if phase == "wait_start" then
            if self._timer >= self.startDelay then
                self._phase = "fade_in_1"
                self._timer = 0
                if self._moveSprite then
                    self._moveSprite.isVisible = true
                end
            end

        elseif phase == "fade_in_1" then
            local t = math.min(self._timer / self.fadeDuration, 1.0)
            if self._moveSprite then self._moveSprite.alpha = t end
            if t >= 1.0 then
                self._phase = "show_1"
            end

        elseif phase == "show_1" then
            local moved = false
            local axis = Input.GetAxis("Movement")
            if axis and (math.abs(axis.x) > 0.1 or math.abs(axis.y) > 0.1) then
                moved = true
            end
            if Keyboard.IsKeyPressed(Keyboard.Key.Space) then
                moved = true
            end
            if moved then
                self._phase = "dismiss_1"
                self._timer = 0
            end

        elseif phase == "dismiss_1" then
            if self._timer >= self.dismissDelay then
                self._phase = "fade_out_1"
                self._timer = 0
            end

        elseif phase == "fade_out_1" then
            local t = math.min(self._timer / self.fadeDuration, 1.0)
            if self._moveSprite then self._moveSprite.alpha = 1.0 - t end
            if t >= 1.0 then
                if self._moveSprite then
                    self._moveSprite.alpha = 0.0
                    self._moveSprite.isVisible = false
                end
                if self._moveEnt then
                    local ac = GetComponent(self._moveEnt, "ActiveComponent")
                    if ac then ac.isActive = false end
                end

                if self._dashEnt then
                    local ac = GetComponent(self._dashEnt, "ActiveComponent")
                    if ac then ac.isActive = true end
                    self._dashSprite = GetComponent(self._dashEnt, "SpriteRenderComponent")
                end

                if self._dashSprite then
                    self._dashSprite.alpha = 0.0
                    self._dashSprite.isVisible = true
                end

                self._phase = "fade_in_2"
                self._timer = 0
            end

        elseif phase == "fade_in_2" then
            local t = math.min(self._timer / self.fadeDuration, 1.0)
            if self._dashSprite then self._dashSprite.alpha = t end
            if t >= 1.0 then
                self._phase = "show_2"
                self._timer = 0
            end

        -- OLD: wait for dash input
        --[[ 
        elseif phase == "show_2" then
            if self._dashDetected then
                self._dashDetected = false
                self._phase = "dismiss_2"
                self._timer = 0
            end
        ]]

        -- NEW: just stay visible for 3 seconds
        elseif phase == "show_2" then
            if self._timer >= self.dashDisplayTime then
                self._phase = "fade_out_2"
                self._timer = 0
            end

        elseif phase == "fade_out_2" then
            local t = math.min(self._timer / self.fadeDuration, 1.0)
            if self._dashSprite then self._dashSprite.alpha = 1.0 - t end
            if t >= 1.0 then
                if self._dashSprite then
                    self._dashSprite.alpha = 0.0
                    self._dashSprite.isVisible = false
                end
                if self._dashEnt then
                    local ac = GetComponent(self._dashEnt, "ActiveComponent")
                    if ac then ac.isActive = false end
                end
                self._phase = "done"
            end
        end
    end,

    OnDisable = function(self)
        --[[ COMMENTED OUT: dash event unsubscribe
        if self._dashSub and event_bus and event_bus.unsubscribe then
            event_bus.unsubscribe("dash_executed", self._dashSub)
            self._dashSub = nil
        end
        ]]
    end,
}