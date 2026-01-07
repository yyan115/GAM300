// LuaSystemBindings.inc
// System bindings using macro-based approach similar to components
// Edit this file to add/remove system bindings
//
// OPTIMIZATION NOTES:
// - Only essential functions are exposed to reduce Lua binding overhead
// - Static functions are preferred over instance methods where possible
// - Wrapper functions are used to simplify API (e.g., passing x,y,z instead of Vector3D)

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
        CONSTANT("Escape", Input::Key::ESC)
        CONSTANT("Delete", Input::Key::DELETE_)
        CONSTANT("Left", Input::Key::LEFT)
        CONSTANT("Right", Input::Key::RIGHT)
        CONSTANT("Up", Input::Key::UP)
        CONSTANT("Down", Input::Key::DOWN)
        CONSTANT("Shift", Input::Key::SHIFT)
        CONSTANT("Ctrl", Input::Key::CTRL)
        CONSTANT("Alt", Input::Key::ALT)
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
    FUNCTION("GetScrollX", InputManager::GetScrollX)
    FUNCTION("GetScrollY", InputManager::GetScrollY)
    FUNCTION("ConsumeScroll", InputManager::ConsumeScroll)
    FUNCTION("GetAnyKeyDown", InputManager::GetAnyKeyDown)
    FUNCTION("GetAnyMouseButtonDown", InputManager::GetAnyMouseButtonDown)
    FUNCTION("GetAnyInputDown", InputManager::GetAnyInputDown)

END_SYSTEM()

// ============================================================================
// TIME SYSTEM
// ============================================================================

BEGIN_SYSTEM("Time")
    FUNCTION("GetDeltaTime", TimeWrappers::GetDeltaTime)
    FUNCTION("GetFixedDeltaTime", TimeWrappers::GetFixedDeltaTime)
    FUNCTION("GetFPS", TimeWrappers::GetFPS)
    FUNCTION("GetTimeScale", TimeWrappers::GetTimeScale)
    FUNCTION("SetTimeScale", TimeWrappers::SetTimeScale)
    FUNCTION("GetScaledDeltaTime", TimeWrappers::GetScaledDeltaTime)
END_SYSTEM()

// ============================================================================
// SCENE MANAGER 
// ============================================================================

BEGIN_SYSTEM("Scene")
    FUNCTION("Load", SceneWrappers::LoadScene)
    FUNCTION("GetCurrentName", SceneWrappers::GetCurrentSceneName)
    FUNCTION("GetCurrentPath", SceneWrappers::GetCurrentScenePath)
END_SYSTEM()

// ============================================================================
// WINDOW/SCREEN SYSTEM
// ============================================================================

BEGIN_SYSTEM("Screen")
    FUNCTION("GetWidth", WindowWrappers::GetWindowWidth)
    FUNCTION("GetHeight", WindowWrappers::GetWindowHeight)
    FUNCTION("GetViewportWidth", WindowWrappers::GetViewportWidth)
    FUNCTION("GetViewportHeight", WindowWrappers::GetViewportHeight)
    FUNCTION("IsFocused", WindowWrappers::IsWindowFocused)
    FUNCTION("IsMinimized", WindowWrappers::IsWindowMinimized)
    FUNCTION("SetTitle", WindowWrappers::SetWindowTitle)
    FUNCTION("RequestClose", WindowWrappers::RequestClose)
    FUNCTION("SetCursorLocked", WindowWrappers::SetCursorLocked)
    FUNCTION("IsCursorLocked", WindowWrappers::IsCursorLocked)
END_SYSTEM()

// ============================================================================
// DEBUG DRAW SYSTEM
// ============================================================================

BEGIN_SYSTEM("Debug")
    FUNCTION("DrawLine", DebugDrawWrappers::DrawLine)
    FUNCTION("DrawCube", DebugDrawWrappers::DrawCube)
    FUNCTION("DrawSphere", DebugDrawWrappers::DrawSphere)
END_SYSTEM()

// ============================================================================
// AUDIO MANAGER (Global audio controls)
// ============================================================================

BEGIN_SYSTEM("Audio")
    FUNCTION("StopAll", AudioManagerWrappers::StopAll)
    FUNCTION("SetMasterVolume", AudioManagerWrappers::SetMasterVolume)
    FUNCTION("GetMasterVolume", AudioManagerWrappers::GetMasterVolume)
    FUNCTION("SetGlobalPaused", AudioManagerWrappers::SetGlobalPaused)
END_SYSTEM()

// ============================================================================
// CHARACTER CONTROLLER SYSTEM
// ============================================================================

// Register CharacterController as a class type first
luabridge::getGlobalNamespace(L)
    .beginClass<CharacterController>("CharacterControllerClass")
    .endClass();

BEGIN_SYSTEM("CharacterController")
    // Constructor
    FUNCTION("new", CharacterControllerWrappers::Create)
    // Initialise
    FUNCTION("Initialise", CharacterControllerWrappers::Initialise)
    // Methods
    FUNCTION("Move", CharacterControllerWrappers::Move)
    FUNCTION("Jump", CharacterControllerWrappers::Jump)
    FUNCTION("Update", CharacterControllerWrappers::Update)
    FUNCTION("GetPosition", CharacterControllerWrappers::GetPosition)
    FUNCTION("IsGrounded", CharacterControllerWrappers::IsGrounded)
    FUNCTION("SetVelocity", CharacterControllerWrappers::SetVelocity)
    FUNCTION("GetVelocity", CharacterControllerWrappers::GetVelocity)
    FUNCTION("GetGravity", CharacterControllerWrappers::GetGravity)
    FUNCTION("SetGravity", CharacterControllerWrappers::SetGravity)
    FUNCTION("Destroy", CharacterControllerWrappers::Destroy)
END_SYSTEM()

BEGIN_SYSTEM("PhysicsSystem")
    FUNCTION("GetSystem", PhysicsSystemWrappers::GetSystem)
END_SYSTEM()

// ============================================================================
// PHYSICS RAYCAST SYSTEM
// ============================================================================

BEGIN_SYSTEM("Physics")
    // Test function to verify binding works
    FUNCTION("Test", PhysicsSystemWrappers::TestBinding)
    // Raycast(originX, originY, originZ, dirX, dirY, dirZ, maxDistance)
    // Returns: distance (float, -1.0 if no hit)
    FUNCTION("Raycast", PhysicsSystemWrappers::Raycast)
END_SYSTEM()


// ============================================================================
// RIGIDBODY SYSTEM
// ============================================================================
BEGIN_SYSTEM("RigidBodyComponent")
    FUNCTION("AddForce", RigidBodySystemWrappers::AddForce)
    FUNCTION("AddTorque", RigidBodySystemWrappers::AddTorque)
    FUNCTION("AddImpulse", RigidBodySystemWrappers::AddImpulse)
END_SYSTEM()


// ============================================================================
// TAG SYSTEMW
// ============================================================================

BEGIN_SYSTEM("Tag")
    FUNCTION("GetName", TagWrappers::GetTagName)
    FUNCTION("GetIndex", TagWrappers::GetTagIndex)
    FUNCTION("GetCount", TagWrappers::GetTagCount)
    FUNCTION("Compare", TagWrappers::CompareTag)
    FUNCTION("CompareNames", TagWrappers::CompareTagNames)
END_SYSTEM()

// ============================================================================
// LAYER SYSTEM
// ============================================================================

BEGIN_SYSTEM("Layer")
    FUNCTION("GetName", LayerWrappers::GetLayerName)
    FUNCTION("GetIndex", LayerWrappers::GetLayerIndex)
    FUNCTION("IsInLayer", LayerWrappers::IsInLayer)
    FUNCTION("GetMask", LayerWrappers::GetLayerMask)
    FUNCTION("MasksIntersect", LayerWrappers::LayerMasksIntersect)
END_SYSTEM()


// ============================================================================
// ANIMATION SYSTEM
// ============================================================================

BEGIN_SYSTEM("Animation")
    FUNCTION("PlayClip",    AnimationWrappers::PlayClip)
    FUNCTION("PlayOnce",    AnimationWrappers::PlayOnce)
    FUNCTION("Pause",       AnimationWrappers::Pause)
    FUNCTION("Stop",        AnimationWrappers::Stop) 
    FUNCTION("SetSpeed",    AnimationWrappers::SetSpeed)
    FUNCTION("SetLooping",  AnimationWrappers::SetLooping)
    FUNCTION("IsPlaying",   AnimationWrappers::IsPlaying)
END_SYSTEM()


// ============================================================================
// ENGINE HELPERS (Entity/Transform lookup)
// ============================================================================
BEGIN_SYSTEM("Engine")
    FUNCTION("FindTransformByName", &Lua_FindTransformByName)
    FUNCTION("GetTransformPosition", &Lua_GetTransformPosition)
    FUNCTION("GetClipIndexByName", &Lua_FindCurrentClipByName)      //TEMPHACK
    FUNCTION("GetTransformRotation", &Lua_GetTransformRotation)
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