require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local event_bus = _G.event_bus

return Component {

    fields = {
        fadeInDuration  = 1.0,  -- fade in 1 sec
        fadeOutDuration = 1.0,  -- fade out 1 sec
        entryDisplayTime = 3.0, -- hold visible for 3 sec

        RoomLabelText = "RoomLabelText",
        RoomLabelImages = {
            "RoomLabelImage_TempleHall",
            "RoomLabelImage_Library",
            "RoomLabelImage_Statue",
            "RoomLabelImage_WeaponRoom",
            "RoomLabelImage_BattleHall",
        },
        LabelStr = {
            "The Temple Hall",
            "The Library",
            "The Guardian Statue Room",
            "The Weapon Room",
            "The Battle Hall",
        },
    },

    Start = function(self)
        self._timer = 0
        self._state = "hidden"
        self._currentIndex = -1
        self._fadeInSpeed = 1.0 / self.fadeInDuration
        self._fadeOutSpeed = 1.0 / self.fadeOutDuration

        -- Cache label TextRenderComponent
        local labelEnt = Engine.GetEntityByName(self.RoomLabelText)
        if labelEnt then
            self._labelText = GetComponent(labelEnt, "TextRenderComponent")
            if self._labelText then
                self._labelText.alpha = 0.0 -- start hidden
            end
        end

        -- Cache sprite renderers
        self._sprites = {}
        for i, name in ipairs(self.RoomLabelImages) do
            local ent = Engine.GetEntityByName(name)
            if ent then
                local sprite = GetComponent(ent, "SpriteRenderComponent")
                if sprite then
                    sprite.alpha = 0.0
                    self._sprites[i] = sprite
                end
            end
        end

        -- Subscribe to triggers
        if event_bus and event_bus.subscribe then
            self._triggerSub = event_bus.subscribe("room_trigger_entered", function(payload)
                local idx = payload and payload.roomIndex
                if idx and self._sprites[idx] then
                    -- Hide previous sprite
                    if self._currentIndex >= 1 and self._sprites[self._currentIndex] then
                        self._sprites[self._currentIndex].alpha = 0.0
                    end

                    -- Update new current
                    self._currentIndex = idx
                    self._timer = 0
                    self._state = "fadein"

                    local sprite = self._sprites[self._currentIndex]
                    sprite.alpha = 0.0 -- start fade in

                    if self._labelText then
                        self._labelText.text = self.LabelStr[idx]
                        self._labelText.alpha = 0.0 -- fade in text together
                    end
                end
            end)
        end
    end,

    Update = function(self, dt)
        if self._state == "hidden" then return end
        local sprite = self._sprites[self._currentIndex]
        if not sprite then return end
        local text = self._labelText

        if self._state == "fadein" then
            local alpha = sprite.alpha + self._fadeInSpeed * dt
            if alpha >= 1.0 then
                alpha = 1.0
                self._state = "visible"
                self._timer = 0
            end
            sprite.alpha = alpha
            if text then text.alpha = alpha end
            return
        end

        if self._state == "visible" then
            self._timer = self._timer + dt
            if self._timer >= self.entryDisplayTime then
                self._state = "fadeout"
            end
            return
        end

        if self._state == "fadeout" then
            local alpha = sprite.alpha - self._fadeOutSpeed * dt
            if alpha <= 0 then
                alpha = 0
                self._state = "hidden"
            end
            sprite.alpha = alpha
            if text then text.alpha = alpha end
        end
    end,

    OnDisable = function(self)
        if event_bus and event_bus.unsubscribe and self._triggerSub then
            event_bus.unsubscribe(self._triggerSub)
        end
    end,
}