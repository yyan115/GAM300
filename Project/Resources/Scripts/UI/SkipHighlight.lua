require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    Start = function(self)


        local entityName = "SkipUI"

        local skipEntity = Engine.GetEntityByName(entityName)

        -- Get the transform and sprite component of the button this script is attached to
        local transform = GetComponent(skipEntity, "Transform")
        local pos   = transform.localPosition
        local scale = transform.localScale
        self.buttonBounds = {} -- TABLE TO STORE ALL MIN/MAX DATA

        -- Store bounds for this button
        self.minX = pos.x - (scale.x / 2)
        self.maxX = pos.x + (scale.x / 2)
        self.minY = pos.y - (scale.y / 2)
        self.maxY = pos.y + (scale.y / 2)



        local targetEntityName = "HoverSkip"
        local defaultEntity = "DefaultSkip"
        local hightlight_Entity = Engine.GetEntityByName(targetEntityName)
        local defaultHightlight_Entity = Engine.GetEntityByName(defaultEntity)

        self.defaultSprite      = GetComponent(defaultHightlight_Entity, "SpriteRenderComponent")
        self.highlightSprite    = GetComponent(hightlight_Entity, "SpriteRenderComponent")

    end,

    Update = function(self, dt)

        if not self.defaultSprite or not self.highlightSprite then 
            return 
        end




        -- GET GAME COORDINATE FOR MOUSE
        local mousePos = Input.GetPointerPosition()
        local mouseCoordinate = Engine.GetGameCoordinate(mousePos.x, mousePos.y)

        -- CHECK IF COORDINATE IS VALID
        if not mouseCoordinate or not mouseCoordinate[1] or not mouseCoordinate[2] then
            return
        end

        local inputX = mouseCoordinate[1]
        local inputY = mouseCoordinate[2]


        -- CHECK IF HOVERING OVER THIS BUTTON
        if  inputX >= self.minX and inputX <= self.maxX and
            inputY >= self.minY and inputY <= self.maxY then
                -- -- TURN ON HIGHLIGHT
                self.defaultSprite.isVisible = false
                self.highlightSprite.isVisible = true
        else
                self.highlightSprite.isVisible = false
                self.defaultSprite.isVisible = true
        end
    end,
}