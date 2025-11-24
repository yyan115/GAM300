// LuaSystemBindings.inc
// System bindings using macro-based approach similar to components
// Edit this file to add/remove system bindings

// ============================================================================
// INPUT SYSTEM
// ============================================================================

BEGIN_SYSTEM("Input")
    
    BEGIN_CONSTANTS("Key")
        CONSTANT("A", Input::Key::A)
        CONSTANT("B", Input::Key::B)
        CONSTANT("C", Input::Key::C)
        CONSTANT("D", Input::Key::D)
        CONSTANT("E", Input::Key::E)
        CONSTANT("F", Input::Key::F)
        CONSTANT("G", Input::Key::G)
        CONSTANT("H", Input::Key::H)
        CONSTANT("I", Input::Key::I)
        CONSTANT("J", Input::Key::J)
        CONSTANT("K", Input::Key::K)
        CONSTANT("L", Input::Key::L)
        CONSTANT("M", Input::Key::M)
        CONSTANT("N", Input::Key::N)
        CONSTANT("O", Input::Key::O)
        CONSTANT("P", Input::Key::P)
        CONSTANT("Q", Input::Key::Q)
        CONSTANT("R", Input::Key::R)
        CONSTANT("S", Input::Key::S)
        CONSTANT("T", Input::Key::T)
        CONSTANT("U", Input::Key::U)
        CONSTANT("V", Input::Key::V)
        CONSTANT("W", Input::Key::W)
        CONSTANT("X", Input::Key::X)
        CONSTANT("Y", Input::Key::Y)
        CONSTANT("Z", Input::Key::Z)
        CONSTANT("Num0", Input::Key::NUM_0)
        CONSTANT("Num1", Input::Key::NUM_1)
        CONSTANT("Num2", Input::Key::NUM_2)
        CONSTANT("Num3", Input::Key::NUM_3)
        CONSTANT("Num4", Input::Key::NUM_4)
        CONSTANT("Num5", Input::Key::NUM_5)
        CONSTANT("Num6", Input::Key::NUM_6)
        CONSTANT("Num7", Input::Key::NUM_7)
        CONSTANT("Num8", Input::Key::NUM_8)
        CONSTANT("Num9", Input::Key::NUM_9)
        CONSTANT("Space", Input::Key::SPACE)
        CONSTANT("Enter", Input::Key::ENTER)
        CONSTANT("Tab", Input::Key::TAB)
        CONSTANT("Backspace", Input::Key::BACKSPACE)
        CONSTANT("Left", Input::Key::LEFT)
        CONSTANT("Right", Input::Key::RIGHT)
        CONSTANT("Up", Input::Key::UP)
        CONSTANT("Down", Input::Key::DOWN)
        CONSTANT("F1", Input::Key::F1)
        CONSTANT("F2", Input::Key::F2)
        CONSTANT("F3", Input::Key::F3)
        CONSTANT("F4", Input::Key::F4)
        CONSTANT("F5", Input::Key::F5)
        CONSTANT("F6", Input::Key::F6)
        CONSTANT("F7", Input::Key::F7)
        CONSTANT("F8", Input::Key::F8)
        CONSTANT("F9", Input::Key::F9)
        CONSTANT("F10", Input::Key::F10)
        CONSTANT("F11", Input::Key::F11)
        CONSTANT("F12", Input::Key::F12)
    END_CONSTANTS()
    
    BEGIN_CONSTANTS("MouseButton")
        CONSTANT("Left", Input::MouseButton::LEFT)
        CONSTANT("Right", Input::MouseButton::RIGHT)
        CONSTANT("Middle", Input::MouseButton::MIDDLE)
    END_CONSTANTS()
    
    FUNCTION("GetKey", InputWrappers::GetKey)
    FUNCTION("GetKeyDown", InputWrappers::GetKeyDown)
    FUNCTION("GetMouseButton", InputWrappers::GetMouseButton)
    FUNCTION("GetMouseButtonDown", InputWrappers::GetMouseButtonDown)
    FUNCTION("GetMouseX", InputManager::GetMouseX)
    FUNCTION("GetMouseY", InputManager::GetMouseY)
    FUNCTION("GetAnyKeyDown", InputManager::GetAnyKeyDown)
    FUNCTION("GetAnyMouseButtonDown", InputManager::GetAnyMouseButtonDown)
    FUNCTION("GetAnyInputDown", InputManager::GetAnyInputDown)

END_SYSTEM()

// ENGINE HELPERS
BEGIN_SYSTEM("Engine")
    FUNCTION("FindTransformByName", &Lua_FindTransformByName)
END_SYSTEM()

// ============================================================================
// FUTURE SYSTEMS (Examples)
// ============================================================================

/*
BEGIN_SYSTEM_NAMESPACE("Time")
    SYSTEM_FUNCTION("GetDeltaTime", TimeManager::GetDeltaTime)
    SYSTEM_FUNCTION("GetTime", TimeManager::GetTime)
END_SYSTEM_NAMESPACE()

BEGIN_SYSTEM_NAMESPACE("Physics")
    SYSTEM_FUNCTION("Raycast", PhysicsManager::Raycast)
    
    BEGIN_ENUM_NAMESPACE("Layer")
        ENUM_CONSTANT("Default", Physics::Layer::DEFAULT)
        ENUM_CONSTANT("Player", Physics::Layer::PLAYER)
    END_ENUM_NAMESPACE()
END_SYSTEM_NAMESPACE()
*/