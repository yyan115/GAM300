-- =============================================================================
-- QUIT PROMPT OVERLAY
-- =============================================================================
-- The quit confirmation for scenes that do not have one of their own.
--
-- 01_MainMenu owns a QuitPromptUI and 04_Level borrows the pause menu's
-- ConfirmationPromptUI, so both answer a close request already. The splash
-- screen, the two cutscenes and the loading screen have no UI at all beyond
-- their own, and a close request there used to end the game on the spot.
--
-- This instantiates Prefabs/QuitPromptUI.prefab on demand and drives it
-- directly. The prefab deliberately carries no ScriptComponent and no
-- ButtonComponent: hit testing here is the same arithmetic QuitPromptButton
-- uses, and doing it here means there are no bindings to remap when the prefab
-- is instantiated at runtime.
--
-- A scene opts in with two lines in its own controller:
--
--     local QuitPrompt = require("UI.QuitPromptOverlay")
--     ...
--     QuitPrompt.Update(dt)          -- in the controller's Update
-- =============================================================================

local M = {}

-- Set true to trace the hit test to stderr. Off in a shipped build.
_G.QUIT_PROMPT_DEBUG = _G.QUIT_PROMPT_DEBUG or false

local PREFAB = "../../Resources/Prefabs/QuitPromptUI.prefab"

-- Normal and hover art, the same four textures the main menu prompt uses.
local YES_NORMAL = "007c7b811bd2e089-00047d15be00b1f5"
local YES_HOVER  = "007c7b810cab88f7-00047d15de00b1fb"
local NO_NORMAL  = "007c7b815635396f-00047d170800b201"
local NO_HOVER   = "007c7b817820b87f-00047d16ec00b1fd"

local state = {
    rootId = nil,
    shown = false,
    buttons = nil,
    cursorWasLocked = false,
}

local function findChild(root, name)
    if not (Engine and Engine.FindChildByName) then return nil end
    local ok, id = pcall(Engine.FindChildByName, root, name)
    if ok and id and id ~= -1 then return id end
    return nil
end

local function setSprite(entity, guid)
    if not entity then return end
    local sprite = GetComponent(entity, "SpriteRenderComponent")
    if sprite and sprite.SetTextureFromGUID then sprite:SetTextureFromGUID(guid) end
end

-- Draw above whatever the scene is already drawing. The prompt's own sorting
-- comes from the main menu, where everything sits on layer 0, and the loading
-- screen puts its background on layer 1 and its bar on layer 2, so the prompt
-- was created and then covered by the background. Raising the layer and
-- leaving the orders alone keeps the tint, plate, text and buttons stacked the
-- way they are in the menu.
local OVERLAY_LAYER = 100

local function raise(entity)
    if not entity then return end
    local sprite = GetComponent(entity, "SpriteRenderComponent")
    if sprite then sprite.sortingLayer = OVERLAY_LAYER end
    local text = GetComponent(entity, "TextRenderComponent")
    if text then text.sortingLayer = OVERLAY_LAYER end
end

local function build()
    if state.rootId then return true end
    if not (Prefab and Prefab.InstantiatePrefab) then return false end
    local ok, id = pcall(Prefab.InstantiatePrefab, PREFAB)
    if not ok or not id or id == -1 then return false end
    state.rootId = id

    for _, name in ipairs({"QuitPromptTint", "QuitPromptBG", "QuitPromptText",
                           "QuitPromptYes", "QuitPromptNo"}) do
        raise(findChild(state.rootId, name))
    end

    state.buttons = {}
    for _, def in ipairs({
        { name = "QuitPromptYes", normal = YES_NORMAL, hover = YES_HOVER, answer = "yes" },
        { name = "QuitPromptNo",  normal = NO_NORMAL,  hover = NO_HOVER,  answer = "no"  },
    }) do
        local entity = findChild(state.rootId, def.name)
        if entity then
            local transform = GetComponent(entity, "Transform")
            if transform then
                state.buttons[#state.buttons + 1] = {
                    entity = entity,
                    transform = transform,
                    normal = def.normal,
                    hover = def.hover,
                    answer = def.answer,
                    hovered = false,
                }
            end
        end
    end
    return #state.buttons == 2
end

local function setActive(active)
    if not state.rootId then return end
    local comp = GetComponent(state.rootId, "ActiveComponent")
    if comp then comp.isActive = active end
end

function M.IsShown()
    return state.shown
end

function M.Show()
    if state.shown then return true end
    if not build() then return false end

    for _, b in ipairs(state.buttons) do
        b.hovered = false
        setSprite(b.entity, b.normal)
    end
    setActive(true)
    state.shown = true

    -- The prompt is answered with the mouse, so the pointer has to be visible
    -- whatever the scene was doing with it.
    if Screen and Screen.IsCursorLocked then
        state.cursorWasLocked = Screen.IsCursorLocked()
        if state.cursorWasLocked then Screen.SetCursorLocked(false) end
    end
    return true
end

function M.Hide()
    if not state.shown then return end
    setActive(false)
    state.shown = false
    if state.cursorWasLocked and Screen and Screen.SetCursorLocked then
        Screen.SetCursorLocked(true)
    end
    state.cursorWasLocked = false
end

-- Scene change: the entities went with the old scene, so forget them rather
-- than hand a stale id to GetComponent.
function M.Forget()
    state.rootId = nil
    state.buttons = nil
    state.shown = false
    state.cursorWasLocked = false
end

local function covers(b, x, y)
    local pos = b.transform.worldPosition
    local scale = b.transform.localScale
    return x >= pos.x - (scale.x / 2) and x <= pos.x + (scale.x / 2)
       and y >= pos.y - (scale.y / 2) and y <= pos.y + (scale.y / 2)
end

local function pointerOver(b)
    local pointer = Input.GetPointerPosition()
    if not pointer then return false end
    local coord = Engine.GetGameCoordinate(pointer.x, pointer.y)
    if not coord then return false end
    return covers(b, coord[1], coord[2])
end

-- Call every frame from the scene's own controller. Says it can show a prompt,
-- takes a close request when one arrives, and runs the prompt while it is up.
function M.Update(dt)
    if not (Screen and Screen.KeepCloseHandler) then return end
    Screen.KeepCloseHandler()

    if Screen.ConsumeCloseRequest() then
        if not M.Show() then
            -- Nothing to show, so do not sit on the request: close as before.
            Screen.RequestClose()
            return
        end
    end

    if not state.shown then return end

    -- A second close request while the prompt is up is handled in the engine
    -- and ends the game, so there is nothing to do for it here.
    if Screen.IsClosePromptOpen and not Screen.IsClosePromptOpen() then
        M.Hide()
        return
    end

    -- Take the click from the window system rather than from the button state,
    -- so a click that begins and ends between two frames still counts. The
    -- loading screen runs at about six frames a second while it loads, which
    -- is short enough to swallow an ordinary click whole.
    local clicked, clickX, clickY = false, nil, nil
    if Input.ConsumeClick and Input.ConsumeClick() then
        local at = Input.GetClickPosition()
        local coord = at and Engine.GetGameCoordinate(at.x, at.y)
        if coord then
            clicked, clickX, clickY = true, coord[1], coord[2]
        end
    end
    if _G.QUIT_PROMPT_DEBUG then
        local p = Input.GetPointerPosition()
        local c = p and Engine.GetGameCoordinate(p.x, p.y)
        local b1 = state.buttons[1]
        io.stderr:write(string.format(
            "[QuitPrompt] pointer=(%s,%s) game=(%s,%s) clicked=%s yes=(%s,%s) scale=(%s,%s)\n",
            tostring(p and p.x), tostring(p and p.y),
            tostring(c and c[1]), tostring(c and c[2]), tostring(clicked),
            tostring(b1 and b1.transform.worldPosition.x),
            tostring(b1 and b1.transform.worldPosition.y),
            tostring(b1 and b1.transform.localScale.x),
            tostring(b1 and b1.transform.localScale.y)))
    end
    for _, b in ipairs(state.buttons) do
        local over = pointerOver(b)
        if over ~= b.hovered then
            b.hovered = over
            setSprite(b.entity, over and b.hover or b.normal)
        end
        if clicked and covers(b, clickX, clickY) then
            if b.answer == "yes" then
                Screen.RequestClose()
            else
                Screen.CancelClose()
                M.Hide()
            end
            return
        end
    end
end

return M
