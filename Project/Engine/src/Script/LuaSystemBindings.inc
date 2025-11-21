// LuaSystemBindings.inc
// Generic system bindings for static C++ systems (InputManager, etc.)
// Edit this file to add/remove system bindings.

// ============================================================================
// INPUT SYSTEM
// ============================================================================

BEGIN_SYSTEM_NAMESPACE("Input")

    // ---- Enums ----
    BEGIN_ENUM("Key")
        ENUM_VALUE("A", Input::Key::A)
        ENUM_VALUE("B", Input::Key::B)
        ENUM_VALUE("C", Input::Key::C)
        ENUM_VALUE("D", Input::Key::D)
        ENUM_VALUE("E", Input::Key::E)
        ENUM_VALUE("F", Input::Key::F)
        ENUM_VALUE("G", Input::Key::G)
        ENUM_VALUE("H", Input::Key::H)
        ENUM_VALUE("I", Input::Key::I)
        ENUM_VALUE("J", Input::Key::J)
        ENUM_VALUE("K", Input::Key::K)
        ENUM_VALUE("L", Input::Key::L)
        ENUM_VALUE("M", Input::Key::M)
        ENUM_VALUE("N", Input::Key::N)
        ENUM_VALUE("O", Input::Key::O)
        ENUM_VALUE("P", Input::Key::P)
        ENUM_VALUE("Q", Input::Key::Q)
        ENUM_VALUE("R", Input::Key::R)
        ENUM_VALUE("S", Input::Key::S)
        ENUM_VALUE("T", Input::Key::T)
        ENUM_VALUE("U", Input::Key::U)
        ENUM_VALUE("V", Input::Key::V)
        ENUM_VALUE("W", Input::Key::W)
        ENUM_VALUE("X", Input::Key::X)
        ENUM_VALUE("Y", Input::Key::Y)
        ENUM_VALUE("Z", Input::Key::Z)
        
        ENUM_VALUE("Num0", Input::Key::NUM_0)
        ENUM_VALUE("Num1", Input::Key::NUM_1)
        ENUM_VALUE("Num2", Input::Key::NUM_2)
        ENUM_VALUE("Num3", Input::Key::NUM_3)
        ENUM_VALUE("Num4", Input::Key::NUM_4)
        ENUM_VALUE("Num5", Input::Key::NUM_5)
        ENUM_VALUE("Num6", Input::Key::NUM_6)
        ENUM_VALUE("Num7", Input::Key::NUM_7)
        ENUM_VALUE("Num8", Input::Key::NUM_8)
        ENUM_VALUE("Num9", Input::Key::NUM_9)
        
        ENUM_VALUE("Space", Input::Key::SPACE)
        ENUM_VALUE("Enter", Input::Key::ENTER)
        ENUM_VALUE("Escape", Input::Key::ESCAPE)
        ENUM_VALUE("Tab", Input::Key::TAB)
        ENUM_VALUE("Backspace", Input::Key::BACKSPACE)
        ENUM_VALUE("Delete", Input::Key::DELETE_KEY)
        
        ENUM_VALUE("LeftShift", Input::Key::LEFT_SHIFT)
        ENUM_VALUE("RightShift", Input::Key::RIGHT_SHIFT)
        ENUM_VALUE("LeftControl", Input::Key::LEFT_CONTROL)
        ENUM_VALUE("RightControl", Input::Key::RIGHT_CONTROL)
        ENUM_VALUE("LeftAlt", Input::Key::LEFT_ALT)
        ENUM_VALUE("RightAlt", Input::Key::RIGHT_ALT)
        
        ENUM_VALUE("Left", Input::Key::LEFT)
        ENUM_VALUE("Right", Input::Key::RIGHT)
        ENUM_VALUE("Up", Input::Key::UP)
        ENUM_VALUE("Down", Input::Key::DOWN)
        
        ENUM_VALUE("F1", Input::Key::F1)
        ENUM_VALUE("F2", Input::Key::F2)
        ENUM_VALUE("F3", Input::Key::F3)
        ENUM_VALUE("F4", Input::Key::F4)
        ENUM_VALUE("F5", Input::Key::F5)
        ENUM_VALUE("F6", Input::Key::F6)
        ENUM_VALUE("F7", Input::Key::F7)
        ENUM_VALUE("F8", Input::Key::F8)
        ENUM_VALUE("F9", Input::Key::F9)
        ENUM_VALUE("F10", Input::Key::F10)
        ENUM_VALUE("F11", Input::Key::F11)
        ENUM_VALUE("F12", Input::Key::F12)
    END_ENUM()

    BEGIN_ENUM("MouseButton")
        ENUM_VALUE("Left", Input::MouseButton::LEFT)
        ENUM_VALUE("Right", Input::MouseButton::RIGHT)
        ENUM_VALUE("Middle", Input::MouseButton::MIDDLE)
    END_ENUM()

    // ---- Functions ----
    SYSTEM_FUNCTION("GetKey", InputManager::GetKey)
    SYSTEM_FUNCTION("GetKeyDown", InputManager::GetKeyDown)
    SYSTEM_FUNCTION("GetMouseButton", InputManager::GetMouseButton)
    SYSTEM_FUNCTION("GetMouseButtonDown", InputManager::GetMouseButtonDown)
    SYSTEM_FUNCTION("GetMouseX", InputManager::GetMouseX)
    SYSTEM_FUNCTION("GetMouseY", InputManager::GetMouseY)
    SYSTEM_FUNCTION("GetAnyKeyDown", InputManager::GetAnyKeyDown)
    SYSTEM_FUNCTION("GetAnyMouseButtonDown", InputManager::GetAnyMouseButtonDown)
    SYSTEM_FUNCTION("GetAnyInputDown", InputManager::GetAnyInputDown)

END_SYSTEM_NAMESPACE()

// ============================================================================
// FUTURE SYSTEMS (Examples - uncomment when needed)
// ============================================================================

/*
BEGIN_SYSTEM_NAMESPACE("Time")
    SYSTEM_FUNCTION("GetDeltaTime", TimeManager::GetDeltaTime)
    SYSTEM_FUNCTION("GetTime", TimeManager::GetTime)
    SYSTEM_FUNCTION("GetFrameCount", TimeManager::GetFrameCount)
END_SYSTEM_NAMESPACE()

BEGIN_SYSTEM_NAMESPACE("JoltPhysics")
    SYSTEM_FUNCTION("Raycast", PhysicsManager::Raycast)
    SYSTEM_FUNCTION("SetGravity", PhysicsManager::SetGravity)
    
    BEGIN_ENUM("Layer")
        ENUM_VALUE("Default", Physics::Layer::DEFAULT)
        ENUM_VALUE("Player", Physics::Layer::PLAYER)
        ENUM_VALUE("Enemy", Physics::Layer::ENEMY)
    END_ENUM()
END_SYSTEM_NAMESPACE()

BEGIN_SYSTEM_NAMESPACE("Scene")
    SYSTEM_FUNCTION("LoadScene", SceneManager::LoadScene)
    SYSTEM_FUNCTION("GetCurrentScene", SceneManager::GetCurrentScene)
END_SYSTEM_NAMESPACE()
*/