require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        fadeDuration = 1.0,
        fadeScreenName = "MenuFadeScreen",
    },

    OnClickResetButton = function(self)
        --not sure why self._sliderData is empty when try accessing.. TEMP FIX
        local sliderMapping = {
            { notch = "MasterTip", bar = "MasterAudioBar", type = "master" },
            { notch = "BGMTip",    bar = "BGMAudioBar",    type = "bgm"    },
            { notch = "SFXTip",    bar = "SFXAudioBar",    type = "sfx"    }
        }

        for _, cfg in ipairs(sliderMapping) do
            local notchEnt = Engine.GetEntityByName(cfg.notch)
            local barEnt   = Engine.GetEntityByName(cfg.bar)

            if notchEnt and barEnt then
                local nTrans = GetComponent(notchEnt, "Transform")
                local bTrans = GetComponent(barEnt, "Transform")
                nTrans.localPosition.x = bTrans.localPosition.x
                nTrans.isDirty = true 
            end
        end
        --Set to 50% vol....



    end,    
    OnClickBackButton = function(self)
        --DISABLE SETTINGS UI SCREEN, ENABLE PAUSE UI SCREEN
        local SettingsUIEntity = Engine.GetEntityByName("SettingsUI")
        local SettingsComp = GetComponent(SettingsUIEntity, "ActiveComponent")
        SettingsComp.isActive = false

        local PauseUIEntity = Engine.GetEntityByName("PauseMenuUI")
        local PauseComp = GetComponent(PauseUIEntity, "ActiveComponent")
        PauseComp.isActive = true

    end,

    Start = function(self)
        self._buttonData = {} 
        self._sliderData = {}
        local buttonMapping = {
            { base = "ResetButton", hover = "HoveredResetButton" },
            { base = "BackButton", hover = "HoveredBackButton" }
        }

        for index, names in ipairs(buttonMapping) do
            local baseEnt = Engine.GetEntityByName(names.base)
            local hoverEnt = Engine.GetEntityByName(names.hover)

            if baseEnt and hoverEnt then
                local transform = GetComponent(baseEnt, "Transform")
                local pos = transform.localPosition
                local scale = transform.localScale
                local hoverSprite = GetComponent(hoverEnt, "SpriteRenderComponent")

                self._buttonData[index] = {
                    hoverSprite = hoverSprite,
                    minX = pos.x - (scale.x / 2),
                    maxX = pos.x + (scale.x / 2),
                    minY = pos.y - (scale.y / 2),
                    maxY = pos.y + (scale.y / 2)
                }

                -- Ensure they start hidden
                if hoverSprite then hoverSprite.isVisible = false end
            else
                print("Warning: Missing entities for " .. names.base)
            end
        end

-- 2. Setup Sliders 
        local sliderMapping = {
            { notch = "MasterTip", bar = "MasterAudioBar", type = "master" },
            { notch = "BGMTip",    bar = "BGMAudioBar",    type = "bgm"    },
            { notch = "SFXTip",    bar = "SFXAudioBar",    type = "sfx"    }
        }

        for _, cfg in ipairs(sliderMapping) do
            local notchEnt = Engine.GetEntityByName(cfg.notch)
            local barEnt   = Engine.GetEntityByName(cfg.bar)

            if notchEnt and barEnt then
                local nTrans = GetComponent(notchEnt, "Transform")
                local bTrans = GetComponent(barEnt, "Transform")
                
                -- Calculate bounds based on the specific bar for this slider
                local offsetX = bTrans.localScale.x / 2.0
                local offsetY = bTrans.localScale.y / 2.0 + 20

                table.insert(self._sliderData, {
                    notchTrans = nTrans,
                    barTrans   = bTrans,
                    type       = cfg.type,
                    minX = bTrans.localPosition.x - offsetX,
                    maxX = bTrans.localPosition.x + offsetX,
                    minY = bTrans.localPosition.y - offsetY,
                    maxY = bTrans.localPosition.y + offsetY
                })
                
            else
                print("Warning: Missing slider entities for " .. cfg.type)
            end
        end
    end,

    Update = function(self, dt)
        if not self._buttonData then return end

        local pointerPos = Input.GetPointerPosition()
        if not pointerPos then return end

        local mouseCoordinate = Engine.GetGameCoordinate(pointerPos.x, pointerPos.y)
        local inputX, inputY = mouseCoordinate[1], mouseCoordinate[2]

        -- Step 1: Hide EVERY hover sprite first
        for _, data in pairs(self._buttonData) do
            if data.hoverSprite then
                data.hoverSprite.isVisible = false
            end
        end

        -- Step 2: Check if mouse is over any button
        for _, data in pairs(self._buttonData) do
            if inputX >= data.minX and inputX <= data.maxX and
               inputY >= data.minY and inputY <= data.maxY then
                
                -- Show only this one
                if data.hoverSprite then
                    data.hoverSprite.isVisible = true
                end
            end
        end

-- Check if the user is actually clicking/holding
        if Input.IsPointerPressed() then
            for _, s in ipairs(self._sliderData) do

                if inputX >= s.minX and inputX <= s.maxX and
                   inputY >= s.minY and inputY <= s.maxY then
                    
                    -- Clamp within the bar boundaries
                    local clampedX = math.max(s.minX, math.min(inputX, s.maxX))
                    s.notchTrans.localPosition.x = clampedX
                    s.notchTrans.isDirty = true

                    local range = s.maxX - s.minX
                    local vol = (clampedX - s.minX) / range
                    
                --SNAPPING: Clean up the edges
                    if vol < 0.01 then vol = 0 
                    elseif vol > 0.99 then vol = 1.0 end

                    -- Set the volume based on the slider type
                    break 
                end
            end
        end
    end
}