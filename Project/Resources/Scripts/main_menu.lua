-- main_menu.lua
-- Attach this script to an entity in your main menu scene
-- Handles cursor visibility and menu navigation

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        gameplayScenePath = "Resources/Scenes/IntroCutScene.scene",  -- Path to gameplay scene
    },

    Awake = function(self)
        -- Ensure cursor is visible and unlocked in main menu
        if Screen and Screen.SetCursorLocked then
            Screen.SetCursorLocked(false)
        end
    end,

    Update = function(self, dt)
        -- Keep cursor unlocked while in menu
        if Screen and Screen.IsCursorLocked and Screen.SetCursorLocked then
            if Screen.IsCursorLocked() then
                Screen.SetCursorLocked(false)
            end
        end
    end,

    -- Call this from a UI button to start the game
    StartGame = function(self)
        if Scene and Scene.Load then
            Scene.Load(self.gameplayScenePath)
        end
    end,

    -- Call this from a UI button to quit the game
    QuitGame = function(self)
        if Screen and Screen.RequestClose then
            Screen.RequestClose()
        end
    end,

    OnDisable = function(self)
        -- Nothing to clean up
    end,
}
