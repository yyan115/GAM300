require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

--PLAY HIGHLIGHT SHOULD BE ENABLED AT THE START
--LAST HIGHLIST STATE SHOULD PERSIST

--NEED TO THINK HOW TO HANDLE THIS HIGHLIGHT FOR ANDROID 


local targetButtons = 
{   "PlayGame", 
    "Credits", 
    "ExitGame",
    "Settings" }


return Component {
    Start = function(self)
        self.lastState = 1      --1 = PLAY GAME
        self.buttonBounds = {} -- TABLE TO STORE ALL MIN/MAX DATA

        --GET ALL THE MIN MAX X Y FOR EACH BUTTON AND STORE IT
        for index, value in ipairs(targetButtons) do
            local targetEntity = Engine.GetEntityByName(value)
            if targetEntity then
                local transform = GetComponent(targetEntity, "Transform")

                local pos   = transform.localPosition
                local scale = transform.localScale

                --Store DATA
                self.buttonBounds[index] = {
                    name = value,            --Store the name as well
                    minX = pos.x - (scale.x / 2),
                    maxX = pos.x + (scale.x / 2),
                    minY = pos.y - (scale.y / 2),
                    maxY = pos.y + (scale.y / 2)                
                }
            end
        end
end,

    Update = function(self, dt)

        --GET GAME COORDINATE FOR MOUSE
        local mouseX = Input.GetMouseX()
        local mouseY = Input.GetMouseY()
        local mouseCoordinate = Engine.GetGameCoordinate(mouseX, mouseY)

        local inputX = mouseCoordinate[1]
        local inputY = mouseCoordinate[2]

        for index, bounds in ipairs(self.buttonBounds) do

            --HOVERING OVER A BUTTON
            if  inputX >= bounds.minX and inputX <= bounds.maxX and
                inputY >= bounds.minY and inputY <= bounds.maxY then
                    
                    --CHECK IF BUTTON HOVERED IS THE SAME AS PREVIOUS STATE
                    if index == self.lastState then return end      --EXIT IF SAME STATE

                    --SET NEW STATE AND ENABLE HIGHLIGHT ACCORDINGLY

                    --Get ButtonString based on index, Disable previous Component and enable new component

                    local oldButtonName = self.buttonBounds[self.lastState].name
                    local oldButtonEntity = Engine.GetEntityByName(oldButtonName)
                    self._oldButtonHighlight = GetComponent(oldButtonEntity, "SpriteRenderComponent")


                    local newButtonEntity = Engine.GetEntityByName(bounds.name)
                    self._newButtonHighlight = GetComponent(newButtonEntity, "SpriteRenderComponent")

                    if self._oldButtonHighlight and self._newButtonHighlight then

                        self._oldButtonHighlight.isVisible   = false
                        self._newButtonHighlight.isVisible  = true
                        self.lastState = index  --update state
                    end
                    -- print("Button is being hovered")


                end
            end


end,
}
--A way to access other entity   DONE
--Enable/Disable Entity   EITHER MAKE IT SO THAT LUA CAN ENABLE/DISABLE ENTITY OR JUST MAKE A TABLE COPY PASTE...

--Access Component Button -> Toggle "Interactable"      DONE


        -- local targetEntity = Engine.GetEntityByName("Testing Entity")

        -- self._targetCollider = GetComponent(targetEntity,"ColliderComponent")
        -- print("TargetENTITY HERE IS ", targetEntity)

        -- if self._targetCollider then
        --     print("I actually got it?")
        -- end

        -- self._targetCollider.enabled = false


        -- local targetEntity = Engine.GetEntityByName("PlayGame")

        -- self._targetButton = GetComponent(targetEntity,"ButtonComponent")
        -- print("TargetENTITY HERE IS ", targetEntity)

        -- if self._targetCollider then
        --     print("I actually got it?")
        -- end

        -- self._targetButton.interactable = false


        -- self._targetRender = GetComponent(targetEntity,"SpriteRenderComponent")
        -- if self._targetRender then
        --     print("i guess render works huh")
        -- end

        -- self._targetRender.alpha = 0    --this works too


        -- --TOGGLE ENTITY TEST lets go it works
        -- self._targetActive = GetComponent(targetEntity, "ActiveComponent")
        -- self._targetActive.isActive = false
