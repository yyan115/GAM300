require("extension.engine_bootstrap")

local Component = require("extension.mono_helper")

local TransformMixin = require("extension.transform_mixin")


return Component {
    mixins = { TransformMixin },

    fields = {
    },

    Start = function(self)
        self._isGamePaused = false
        self._pauseTimer = 0  -- Initialize timer
        
        local pauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        self._pauseComp = GetComponent(pauseUIEntity, "ActiveComponent")
        
        local settingsUIEntity = Engine.GetEntityByName("SettingsUI")
        self._settingsComp = GetComponent(settingsUIEntity, "ActiveComponent")

        local confirmUIEntity = Engine.GetEntityByName("ConfirmationPromptUI")
        self._confirmComp = GetComponent(confirmUIEntity, "ActiveComponent")

        --NEED TO GET COMPONENT FOR PAUSEMENU UI, SETTINGS UI, CONFIRMATION PROMPT

        --IF ALL INACTIVE -> OPEN PAUSEMENU UI
        --IF PAUSE MENU ACTIVE -> CLOSE, -> IF SETTING OR CONFIRM PROMPT UI IS ACTIVE , ESC GOES INTO PAUSEMENU UI

    end,

    Update = function(self, dt)
        if not self._pauseComp or not self._settingsComp or not self._confirmComp then
            return
        end     

        -- Reduce the timer by the time passed since last frame
        if self._pauseTimer > 0 then
            self._pauseTimer = self._pauseTimer - dt
        end

        local isPressed = Input.IsActionJustPressed("Pause")
        -- Only fire if the button is pressed AND our cooldown has expired  --TO STOP IT FROM FIRING TWICE
        if isPressed and self._pauseTimer <= 0 then
            -- if not self._pauseComp.isActive and not self._settingsComp.isActive and not self._pauseComp.isActive then
            --     self._pauseComp.isActive = true
            -- end

            -- --TOGGLE PAUSE MENU LOGIC VIA ESC KEY
            -- if self._pauseComp.isActive then
            --     self._pauseComp.isActive = false
            -- end

            -- if self._settingsComp.isActive then
            --     self._settingsComp.isActive = false
            --     self._pauseComp.isActive    = true
            -- end
            -- if self._confirmComp.isActive then
            --     self._confirmComp.isActive = false
            --     self._pauseComp.isActive = true
            -- end
            -- -- Set a small cooldown (e.g., 0.1 seconds) to swallow the "double fire"
            self._pauseTimer = 0.1 

            if self._confirmComp.isActive then
                self._confirmComp.isActive = false
                self._pauseComp.isActive = true  -- Go back to Pause menu

            -- 2. Check the Middle Layer (Settings)
            elseif self._settingsComp.isActive then
                self._settingsComp.isActive = false
                self._pauseComp.isActive = true  -- Go back to Pause menu

            -- 3. Check the Base Layer (Pause Menu itself)
            elseif self._pauseComp.isActive then
                self._pauseComp.isActive = false
                Time.SetPaused(false)            -- Resume game
            else
                self._pauseComp.isActive = true
                Time.SetPaused(true)             -- Pause game
            end
        end
    end,
}