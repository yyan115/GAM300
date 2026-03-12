require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

local event_bus = _G.event_bus

--[[
    TutorialPrompts.lua
    Standalone tutorial — fades in/out 2 sprites sequentially.
    No dialogue system needed.

    Sprite 1: MovementControlsSprite — waits for WASD or Space
    Sprite 2: DashControlsSprite     — waits for player to actually dash
]]

return Component {
    fields = {
        moveSpriteEntityName = "MovementControlsSprite",
        dashSpriteEntityName = "DashControlsSprite",

        fadeDuration         = 0.5,      -- seconds to fade in/out
        startDelay           = 1.0,      -- wait before showing first sprite
        dismissDelay         = 1.0,      -- wait after input before fading out
    },

    Start = function(self)
        self._phase       = "wait_start"
        self._timer       = 0
        self._moveSprite  = nil
        self._dashSprite  = nil
        self._moveEnt     = nil
        self._dashEnt     = nil
        self._dashDetected = false

        -- Subscribe to dash_executed event (only fires when player actually dashes,
        -- not just pressing Shift — requires dash charges, not stunned, etc.)
        if event_bus and event_bus.subscribe then
            self._dashSub = event_bus.subscribe("dash_executed", function()
                self._dashDetected = true
            end)
        end

        -- Find sprite entities, activate them (they default to SetActive false),
        -- then hide them visually (alpha 0) until we're ready to fade in.
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
            -- Keep dash entity inactive until we need it
            local activeComp = GetComponent(dashEnt, "ActiveComponent")
            if activeComp then activeComp.isActive = false end
        end
    end,

    Update = function(self, dt)
        local phase = self._phase
        if phase == "done" then return end

        self._timer = self._timer + dt

        -- 1) Wait before showing anything
        if phase == "wait_start" then
            if self._timer >= self.startDelay then
                self._phase = "fade_in_1"
                self._timer = 0
                if self._moveSprite then
                    self._moveSprite.isVisible = true
                end
            end

        -- 2) Fade in movement sprite
        elseif phase == "fade_in_1" then
            local t = math.min(self._timer / self.fadeDuration, 1.0)
            if self._moveSprite then self._moveSprite.alpha = t end
            if t >= 1.0 then
                self._phase = "show_1"
            end

        -- 3) Wait for WASD or Space
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

        -- 4) Dismiss delay after movement detected
        elseif phase == "dismiss_1" then
            if self._timer >= self.dismissDelay then
                self._phase = "fade_out_1"
                self._timer = 0
            end

        -- 5) Fade out movement sprite
        elseif phase == "fade_out_1" then
            local t = math.min(self._timer / self.fadeDuration, 1.0)
            if self._moveSprite then self._moveSprite.alpha = 1.0 - t end
            if t >= 1.0 then
                -- Hide and deactivate movement sprite
                if self._moveSprite then
                    self._moveSprite.alpha = 0.0
                    self._moveSprite.isVisible = false
                end
                if self._moveEnt then
                    local ac = GetComponent(self._moveEnt, "ActiveComponent")
                    if ac then ac.isActive = false end
                end
                -- Activate and prepare dash sprite
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

        -- 6) Fade in dash sprite
        elseif phase == "fade_in_2" then
            local t = math.min(self._timer / self.fadeDuration, 1.0)
            if self._dashSprite then self._dashSprite.alpha = t end
            if t >= 1.0 then
                self._phase = "show_2"
            end

        -- 7) Wait for player to actually dash (dash_executed event)
        elseif phase == "show_2" then
            if self._dashDetected then
                self._dashDetected = false
                self._phase = "dismiss_2"
                self._timer = 0
            end

        -- 8) Dismiss delay after dash detected
        elseif phase == "dismiss_2" then
            if self._timer >= self.dismissDelay then
                self._phase = "fade_out_2"
                self._timer = 0
            end

        -- 9) Fade out dash sprite
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
        -- Unsubscribe from dash event
        if self._dashSub and event_bus and event_bus.unsubscribe then
            event_bus.unsubscribe("dash_executed", self._dashSub)
            self._dashSub = nil
        end
    end,
}
