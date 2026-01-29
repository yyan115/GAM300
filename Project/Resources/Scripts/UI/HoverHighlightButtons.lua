require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

--PLAY HIGHLIGHT SHOULD BE ENABLED AT THE START
--LAST HIGHLIST STATE SHOULD PERSIST

--NEED TO THINK HOW TO HANDLE THIS HIGHLIGHT FOR ANDROID 

--IF CLICKED ON A BUTTON -> LAST STATE SHOULD BE THAT BUTTON (LOCKED HIGHLIGHT)

local targetButtons = 
{   "PlayGame", 
    "Credits", 
    "ExitGame",
    "Settings" }

local UIMenus = 
{
    "SettingsUI",
    "CreditsUI"
}

return Component {
    Start = function(self)
        self.lastState = 1      --1 = PLAY GAME
        self.buttonBounds = {} -- TABLE TO STORE ALL MIN/MAX DATA
        self.UIState = {}
        self._lockedState = false

        -- Cache audio component for hover SFX
        self._audio = self:GetComponent("AudioComponent")

        --GET ALL THE MIN MAX X Y FOR EACH BUTTON AND STORE IT
        for index, value in ipairs(targetButtons) do
            local targetEntity = Engine.GetEntityByName(value)
            if targetEntity then
                local transform = GetComponent(targetEntity, "Transform")
                local pos   = transform.localPosition
                local scale = transform.localScale

                local component = GetComponent(targetEntity, "SpriteRenderComponent")

                --Store DATA
                self.buttonBounds[index] = {
                    spriteComponent = component,            --Store the sprite component to use later
                    minX = pos.x - (scale.x / 2),
                    maxX = pos.x + (scale.x / 2),
                    minY = pos.y - (scale.y / 2),
                    maxY = pos.y + (scale.y / 2)                
                }
            end
        end

        --STORE COMPONENTS FOR EACH UI ENTITY
        for index, value in ipairs(UIMenus) do
            local UIEntity = Engine.GetEntityByName(value)
            if UIEntity then
                local activeComponent = GetComponent(UIEntity, "ActiveComponent")
                self.UIState[index] = {
                    component = activeComponent       
                }
            end
        end
    end,

Update = function(self, dt)

    --GET GAME COORDINATE FOR MOUSE (unified input system)
    local pointerPos = Input.GetPointerPosition()
    if not pointerPos then return end

    local mouseCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
    if not mouseCoordinate then return end

    local inputX = mouseCoordinate[1]
    local inputY = mouseCoordinate[2]
    if not inputX or not inputY then return end

    -- CHECK IF ANY UI IS OPEN
    local anyUIOpen = false
    for _, states in ipairs(self.UIState) do
        if states.component and states.component.isActive then
            anyUIOpen = true
            break
        end
    end

    -- If no UI is open, unlock the state
    if not anyUIOpen then
        self._lockedState = false
    end

    -- If locked, don't process hover logic
    if self._lockedState then
        return
    end

    for index, bounds in ipairs(self.buttonBounds) do

        --HOVERING OVER A BUTTON (skip if bounds are nil)
        if bounds.minX and bounds.maxX and bounds.minY and bounds.maxY and
           inputX >= bounds.minX and inputX <= bounds.maxX and
           inputY >= bounds.minY and inputY <= bounds.maxY then

                --IF BUTTON IS CLICKED, LOCK HIGHLIGHT FOR THE BUTTON.
                if Input.IsPointerPressed() then
                    self._lockedState = true
                end

                --CHECK IF BUTTON HOVERED IS THE SAME AS PREVIOUS STATE
                if index == self.lastState then return end

                --OLD BUTTON
                local oldButtonHighlight = self.buttonBounds[self.lastState].spriteComponent

                --NEW BUTTON
                local newButtonHighlight = self.buttonBounds[index].spriteComponent

                if oldButtonHighlight and newButtonHighlight then
                    oldButtonHighlight.isVisible = false
                    newButtonHighlight.isVisible = true
                    self.lastState = index  --update state

                    -- Play hover SFX
                    if self._audio then
                        self._audio:Play()
                    end
                end
        end
    end
end,}
