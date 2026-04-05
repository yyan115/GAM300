require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local event_bus = _G.event_bus

return Component {
    fields = {
        -- [1] = Normal Sprite, [2] = Highlighted Sprite
        spriteGUIDs = {},
        -- [1] = Hover SFX, [2] = Click SFX
        SFX = {},
    },

    Start = function(self)
        self._transform = self:GetComponent("Transform")
        self._sprite = self:GetComponent("SpriteRenderComponent")
        self._isHovered = false
        self._ScreenRequestClose = false

        --Disable Interactable Button 
        local YesEntity = Engine.GetEntityByName("YesButton")
        local YesButton = GetComponent(YesEntity, "ButtonComponent")
        YesButton.interactable = false
        local NoEntity = Engine.GetEntityByName("NoButton")
        local NoButton = GetComponent(NoEntity, "ButtonComponent")
        NoButton.interactable = false

        
        -- Get the parent Prompt UI to check if it's active
        self._QuitPromptEntity = Engine.GetEntityByName("QuitPromptUI")
        if self._QuitPromptEntity then
            self._promptActive = GetComponent(self._QuitPromptEntity, "ActiveComponent")
        end
    end,

    Update = function(self, dt)
            local isPromptActive = self._promptActive and self._promptActive.isActive

            if isPromptActive then
                -- 1. Run the hover logic for the Yes/No buttons
                --Disable Interactable Button 
                local YesEntity = Engine.GetEntityByName("YesButton")
                local YesButton = GetComponent(YesEntity, "ButtonComponent")
                YesButton.interactable = true   
                local NoEntity = Engine.GetEntityByName("NoButton")
                local NoButton = GetComponent(NoEntity, "ButtonComponent")
                NoButton.interactable = true
                self:_updateHoverState()
            end


        end,

    _updateHoverState = function(self)
        if not self._transform then return end

        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return end

        local mouseCoord = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        local inputX, inputY = mouseCoord[1], mouseCoord[2]

        local pos = self._transform.worldPosition
        local scale = self._transform.localScale
        
        -- AABB Detection
        local isHovering = inputX >= pos.x - (scale.x / 2) and 
                           inputX <= pos.x + (scale.x / 2) and 
                           inputY >= pos.y - (scale.y / 2) and 
                           inputY <= pos.y + (scale.y / 2)

        -- State Change: Enter Hover
        if isHovering and not self._isHovered then
            self._isHovered = true
            
            if event_bus and event_bus.publish then
                event_bus.publish("main_menu.hover", {})
            end
            
            -- Swap to Highlighted Sprite
            if self._sprite and self.spriteGUIDs[2] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[2])
            end

        -- State Change: Exit Hover
        elseif not isHovering and self._isHovered then
            self._isHovered = false
            
            -- Swap back to Normal Sprite
            if self._sprite and self.spriteGUIDs[1] then
                self._sprite:SetTextureFromGUID(self.spriteGUIDs[1])
            end
        end
    end,

    ClosePrompt = function(self)
            if event_bus and event_bus.publish then
                event_bus.publish("main_menu.click", {})
            end

            local YesEntity = Engine.GetEntityByName("YesButton")
            local YesButton = GetComponent(YesEntity, "ButtonComponent")
            YesButton.interactable = false
            local NoEntity = Engine.GetEntityByName("NoButton")
            local NoButton = GetComponent(NoEntity, "ButtonComponent")
            NoButton.interactable = false

            -- 1. Hide the Quit Prompt UI
            if self._QuitPromptEntity then
                local activeComp = GetComponent(self._QuitPromptEntity, "ActiveComponent")
                if activeComp then
                    activeComp.isActive = false
                end
            end

            -- 2. Restore ONLY interactability
            local mainMenuButtons = {"PlayGame", "Settings", "Credits", "ExitGame"}
            for _, name in ipairs(mainMenuButtons) do
                local ent = Engine.GetEntityByName(name)
                if ent and ent ~= -1 then
                    local btn = GetComponent(ent, "ButtonComponent")
                    if btn then 
                        btn.interactable = true 
                    end
                end
            end
        end,

    ExitGame = function(self)
        if event_bus and event_bus.publish then
            event_bus.publish("main_menu.click", {})
        end

        self._ScreenRequestClose = true
        if self._ScreenRequestClose == true then
            Screen.RequestClose()
        end
    end
}