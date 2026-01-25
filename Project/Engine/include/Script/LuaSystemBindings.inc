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


// ============================================================================
// UNIFIED INPUT SYSTEM (NEW - Platform-Agnostic)
// ============================================================================
// This is the new unified input system that works on both desktop and Android.
// Game code should use this instead of the legacy Input system above.
//
// Usage examples:
// -- Check action (works on both desktop and Android)
// if Input.IsActionPressed("Jump") then
//     player:Jump()
// end
//
// -- Get axis (virtual joystick on Android, WASD on desktop)
// local axis = Input.GetAxis("Movement")
// player:Move(axis.x, axis.y)
//
// -- Full touch system (phases, IDs, entities)
// local touches = Input.GetTouches()
// for i, touch in ipairs(touches) do
//     print(touch.id, touch.phase, touch.entity)
//     -- touch.phase: "began", "moved", "stationary", "ended"
//     -- touch.entity: entity name if on UI, "" if not
// end

// Register TouchInfo class for Lua
luabridge::getGlobalNamespace(L)
    .beginClass<TouchInfo>("TouchInfo")
        .addProperty("id", &TouchInfo::id)
        .addProperty("phase", &TouchInfo::phase)
        .addProperty("position", &TouchInfo::position)
        .addProperty("startPosition", &TouchInfo::startPosition)
        .addProperty("delta", &TouchInfo::delta)
        .addProperty("entity", &TouchInfo::entity)
        .addProperty("duration", &TouchInfo::duration)
    .endClass();

BEGIN_SYSTEM("Input")
    // Actions
    FUNCTION("IsActionPressed", InputWrappers::IsActionPressed)
    FUNCTION("IsActionJustPressed", InputWrappers::IsActionJustPressed)
    FUNCTION("IsActionJustReleased", InputWrappers::IsActionJustReleased)

    // Axes (returns Vector2D object with .x and .y properties)
    FUNCTION("GetAxis", InputWrappers::GetAxis)

    // Entity-based touch position (Android joysticks)
    // Returns touch position relative to entity center - use for calculating joystick direction
    // Example: local pos = Input.GetActionTouchPosition("Movement")
    //          local dir = { x = pos.x / radius, y = pos.y / radius }
    FUNCTION("GetActionTouchPosition", InputWrappers::GetActionTouchPosition)

    // Camera drag (Android - unhandled touches become camera drag)
    // Example: if Input.IsDragging() then
    //              local delta = Input.GetDragDelta()
    //              camera:Rotate(delta.x, delta.y)
    //          end
    FUNCTION("IsDragging", InputWrappers::IsDragging)
    FUNCTION("GetDragDelta", InputWrappers::GetDragDelta)

    // Batch API (for Lua optimization)
    FUNCTION("GetAllActionStates", InputWrappers::GetAllActionStates)
    FUNCTION("GetAllAxisStates", InputWrappers::GetAllAxisStates)

    // Pointer abstraction (for UI - works with mouse OR touch)
    FUNCTION("IsPointerPressed", InputWrappers::IsPointerPressed)
    FUNCTION("IsPointerJustPressed", InputWrappers::IsPointerJustPressed)
    FUNCTION("GetPointerPosition", InputWrappers::GetPointerPosition)

    // Multi-touch (Android - for advanced touch handling)
    FUNCTION("GetTouchCount", InputWrappers::GetTouchCount)
    FUNCTION("GetTouchPosition", InputWrappers::GetTouchPosition)

    // Full touch system (phases, IDs, entities)
    // Returns array of TouchInfo objects with: id, phase, position, startPosition, delta, entity, duration
    FUNCTION("GetTouches", InputWrappers::GetTouches)
    FUNCTION("GetTouchById", InputWrappers::GetTouchById)
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
    // Bus/AudioMixerGroup controls (BGM, SFX, Master, etc.)
    FUNCTION("SetBusVolume", AudioManagerWrappers::SetBusVolume)
    FUNCTION("GetBusVolume", AudioManagerWrappers::GetBusVolume)
    FUNCTION("SetBusPaused", AudioManagerWrappers::SetBusPaused)
END_SYSTEM()

// ============================================================================
// PLATFORM SYSTEM
// ============================================================================
// Provides platform detection for conditional behavior in Lua scripts.
// Useful for showing/hiding touch controls on Android vs desktop.
//
// Usage examples:
// if Platform.IsAndroid() then
//     -- Show touch controls
//     self.entity:SetActive(true)
// else
//     -- Hide on desktop
//     self.entity:SetActive(false)
// end
//
// local name = Platform.GetName()  -- "Android", "Windows", "Linux", "macOS"

BEGIN_SYSTEM("Platform")
    FUNCTION("IsAndroid", PlatformWrappers::IsAndroid)
    FUNCTION("IsDesktop", PlatformWrappers::IsDesktop)
    FUNCTION("GetName", PlatformWrappers::GetPlatformName)
END_SYSTEM()

// ============================================================================
// CHARACTER CONTROLLER SYSTEM
// ============================================================================

// Register CharacterController as a class type first
luabridge::getGlobalNamespace(L)
    .beginClass<CharacterController>("CharacterControllerClass")
    .endClass();

BEGIN_SYSTEM("CharacterController")
    FUNCTION("Create", CharacterControllerWrappers::CreateController)

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
// TAG SYSTEM
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
// ENGINE HELPERS (Entity/Transform lookup)
// ============================================================================
BEGIN_SYSTEM("Engine")
    FUNCTION("FindTransformByName", &Lua_FindTransformByName)
    FUNCTION("GetTransformPosition", &Lua_GetTransformPosition)
    FUNCTION("GetTransformWorldPosition", &Lua_GetTransformWorldPosition)
    FUNCTION("GetClipIndexByName", &Lua_FindCurrentClipByName)      //TEMPHACK
    FUNCTION("GetTransformRotation", &Lua_GetTransformRotation)
    FUNCTION("GetEntityByName", &Lua_FindEntityByName)
    FUNCTION("GetGameCoordinate", &Lua_ScreenToGameCoordinates)
    FUNCTION("CreateEntityDup", &Lua_CreateEntityDup)
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