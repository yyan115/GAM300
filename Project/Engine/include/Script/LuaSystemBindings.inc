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
// KEYBOARD INPUT (RAW - PC/DESKTOP ONLY)
// ============================================================================
// WARNING: Prefer Input.IsActionPressed() / Input.GetAxis() from the unified
// input system (see below). These raw keyboard functions:
//   - Are PC/desktop only — always return false on Android
//   - Bypass input config (keys are hardcoded, not remappable)
// Only use when you have a specific reason, e.g. debug tools, editor features,
// or PC-exclusive shortcuts that don't belong in the action config.
//
// Usage:
//   if Keyboard.IsKeyPressed(Keyboard.Key.Space) then ... end
//   if Keyboard.IsMouseButtonPressed(Keyboard.Mouse.Left) then ... end
//   local x, y = Keyboard.GetMouseX(), Keyboard.GetMouseY()

BEGIN_SYSTEM("Keyboard")
    // "pressed" — true only on the frame the key first goes down (one-shot)
    FUNCTION("IsKeyPressed", KeyboardWrappers::IsKeyPressed)
    FUNCTION("IsDigitPressed", KeyboardWrappers::IsDigitPressed)
    // "held" — true every frame the key is physically down
    FUNCTION("IsKeyHeld", KeyboardWrappers::IsKeyHeld)
    FUNCTION("IsDigitHeld", KeyboardWrappers::IsDigitHeld)
    FUNCTION("IsMouseButtonPressed", KeyboardWrappers::IsMouseButtonPressed)
    FUNCTION("GetMouseX", KeyboardWrappers::GetMouseX)
    FUNCTION("GetMouseY", KeyboardWrappers::GetMouseY)

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
        CONSTANT("Zero", Input::Key::NUM_0)
        CONSTANT("One", Input::Key::NUM_1)
        CONSTANT("Two", Input::Key::NUM_2)
        CONSTANT("Three", Input::Key::NUM_3)
        CONSTANT("Four", Input::Key::NUM_4)
        CONSTANT("Five", Input::Key::NUM_5)
        CONSTANT("Six", Input::Key::NUM_6)
        CONSTANT("Seven", Input::Key::NUM_7)
        CONSTANT("Eight", Input::Key::NUM_8)
        CONSTANT("Nine", Input::Key::NUM_9)
        CONSTANT("Digit0", Input::Key::NUM_0)
        CONSTANT("Digit1", Input::Key::NUM_1)
        CONSTANT("Digit2", Input::Key::NUM_2)
        CONSTANT("Digit3", Input::Key::NUM_3)
        CONSTANT("Digit4", Input::Key::NUM_4)
        CONSTANT("Digit5", Input::Key::NUM_5)
        CONSTANT("Digit6", Input::Key::NUM_6)
        CONSTANT("Digit7", Input::Key::NUM_7)
        CONSTANT("Digit8", Input::Key::NUM_8)
        CONSTANT("Digit9", Input::Key::NUM_9)
        CONSTANT("Space", Input::Key::SPACE)
        CONSTANT("Enter", Input::Key::ENTER)
        CONSTANT("Escape", Input::Key::ESC)
        CONSTANT("Tab", Input::Key::TAB)
        CONSTANT("Backspace", Input::Key::BACKSPACE)
        CONSTANT("Delete", Input::Key::DELETE_)
        CONSTANT("Up", Input::Key::UP)
        CONSTANT("Down", Input::Key::DOWN)
        CONSTANT("Left", Input::Key::LEFT)
        CONSTANT("Right", Input::Key::RIGHT)
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
        CONSTANT("Shift", Input::Key::SHIFT)
        CONSTANT("Ctrl", Input::Key::CTRL)
        CONSTANT("Alt", Input::Key::ALT)
    END_CONSTANTS()

    BEGIN_CONSTANTS("Mouse")
        CONSTANT("Left",   Input::MouseButton::LEFT)
        CONSTANT("Right",  Input::MouseButton::RIGHT)
        CONSTANT("Middle", Input::MouseButton::MIDDLE)
    END_CONSTANTS()
END_SYSTEM()

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

    // Scroll wheel (desktop only; returns delta this frame, positive = scroll up)
    FUNCTION("GetScrollY", InputWrappers::GetScrollY)

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
    FUNCTION("GetUnscaledDeltaTime", TimeWrappers::GetUnscaledDeltaTime)
    FUNCTION("GetFixedDeltaTime", TimeWrappers::GetFixedDeltaTime)
    FUNCTION("GetFPS", TimeWrappers::GetFPS)
    FUNCTION("GetTimeScale", TimeWrappers::GetTimeScale)
    FUNCTION("SetTimeScale", TimeWrappers::SetTimeScale)
    FUNCTION("GetScaledDeltaTime", TimeWrappers::GetScaledDeltaTime)
    FUNCTION("SetPaused", TimeWrappers::SetPaused)
    FUNCTION("IsPaused", TimeWrappers::IsPaused)
END_SYSTEM()

// ============================================================================
// SCENE MANAGER 
// ============================================================================

BEGIN_SYSTEM("Scene")
    FUNCTION("Load", SceneWrappers::LoadScene)
    FUNCTION("LoadAsync", SceneWrappers::LoadSceneAsync)
    FUNCTION("GetLoadProgress", SceneWrappers::GetLoadProgress)
    FUNCTION("IsLoading", SceneWrappers::IsLoading)
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
// GAME SETTINGS (Persistent audio/graphics settings with JSON serialization)
// ============================================================================
// Usage examples:
// GameSettings.Init()                    -- Initialize (call once at startup)
// GameSettings.SetMasterVolume(0.5)      -- Set volume (0.0-1.0)
// local vol = GameSettings.GetMasterVolume()
// GameSettings.SetGamma(2.2)             -- Set gamma (1.0-3.0)
// GameSettings.SaveIfDirty()             -- Save if changed (call when closing menu)
// GameSettings.ResetToDefaults()         -- Reset all settings

BEGIN_SYSTEM("GameSettings")
    // Initialization
    FUNCTION("Init", GameSettingsWrappers::Init)
    FUNCTION("ResetToDefaults", GameSettingsWrappers::ResetToDefaults)

    // Save functions (call when closing settings menu or on scene transitions)
    FUNCTION("Save", GameSettingsWrappers::Save)
    FUNCTION("SaveIfDirty", GameSettingsWrappers::SaveIfDirty)

    // Audio settings
    FUNCTION("SetMasterVolume", GameSettingsWrappers::SetMasterVolume)
    FUNCTION("GetMasterVolume", GameSettingsWrappers::GetMasterVolume)
    FUNCTION("SetBGMVolume", GameSettingsWrappers::SetBGMVolume)
    FUNCTION("GetBGMVolume", GameSettingsWrappers::GetBGMVolume)
    FUNCTION("SetSFXVolume", GameSettingsWrappers::SetSFXVolume)
    FUNCTION("GetSFXVolume", GameSettingsWrappers::GetSFXVolume)

    // Graphics settings
    FUNCTION("SetGamma", GameSettingsWrappers::SetGamma)
    FUNCTION("GetGamma", GameSettingsWrappers::GetGamma)
    FUNCTION("SetExposure", GameSettingsWrappers::SetExposure)
    FUNCTION("GetExposure", GameSettingsWrappers::GetExposure)
    FUNCTION("SetToneMappingMode", GameSettingsWrappers::SetToneMappingMode)
    FUNCTION("GetToneMappingMode", GameSettingsWrappers::GetToneMappingMode)
    FUNCTION("SetVSync", GameSettingsWrappers::SetVSync)
    FUNCTION("GetVSync", GameSettingsWrappers::GetVSync)
    FUNCTION("SetFullscreen", GameSettingsWrappers::SetFullscreen)
    FUNCTION("GetFullscreen", GameSettingsWrappers::GetFullscreen)

    // Bloom
    FUNCTION("SetBloomEnabled", GameSettingsWrappers::SetBloomEnabled)
    FUNCTION("GetBloomEnabled", GameSettingsWrappers::GetBloomEnabled)
    FUNCTION("SetBloomThreshold", GameSettingsWrappers::SetBloomThreshold)
    FUNCTION("GetBloomThreshold", GameSettingsWrappers::GetBloomThreshold)
    FUNCTION("SetBloomIntensity", GameSettingsWrappers::SetBloomIntensity)
    FUNCTION("GetBloomIntensity", GameSettingsWrappers::GetBloomIntensity)
    FUNCTION("SetBloomScatter", GameSettingsWrappers::SetBloomScatter)
    FUNCTION("GetBloomScatter", GameSettingsWrappers::GetBloomScatter)

    // Vignette
    FUNCTION("SetVignetteEnabled", GameSettingsWrappers::SetVignetteEnabled)
    FUNCTION("GetVignetteEnabled", GameSettingsWrappers::GetVignetteEnabled)
    FUNCTION("SetVignetteIntensity", GameSettingsWrappers::SetVignetteIntensity)
    FUNCTION("GetVignetteIntensity", GameSettingsWrappers::GetVignetteIntensity)
    FUNCTION("SetVignetteSmoothness", GameSettingsWrappers::SetVignetteSmoothness)
    FUNCTION("GetVignetteSmoothness", GameSettingsWrappers::GetVignetteSmoothness)
    FUNCTION("SetVignetteColor", GameSettingsWrappers::SetVignetteColor)
    FUNCTION("GetVignetteColor", GameSettingsWrappers::GetVignetteColor)

    // Color Grading
    FUNCTION("SetColorGradingEnabled", GameSettingsWrappers::SetColorGradingEnabled)
    FUNCTION("GetColorGradingEnabled", GameSettingsWrappers::GetColorGradingEnabled)
    FUNCTION("SetCGBrightness", GameSettingsWrappers::SetCGBrightness)
    FUNCTION("GetCGBrightness", GameSettingsWrappers::GetCGBrightness)
    FUNCTION("SetCGContrast", GameSettingsWrappers::SetCGContrast)
    FUNCTION("GetCGContrast", GameSettingsWrappers::GetCGContrast)
    FUNCTION("SetCGSaturation", GameSettingsWrappers::SetCGSaturation)
    FUNCTION("GetCGSaturation", GameSettingsWrappers::GetCGSaturation)
    FUNCTION("SetCGTint", GameSettingsWrappers::SetCGTint)
    FUNCTION("GetCGTint", GameSettingsWrappers::GetCGTint)

    // Chromatic Aberration
    FUNCTION("SetCAEnabled", GameSettingsWrappers::SetCAEnabled)
    FUNCTION("GetCAEnabled", GameSettingsWrappers::GetCAEnabled)
    FUNCTION("SetCAIntensity", GameSettingsWrappers::SetCAIntensity)
    FUNCTION("GetCAIntensity", GameSettingsWrappers::GetCAIntensity)
    FUNCTION("SetCAPadding", GameSettingsWrappers::SetCAPadding)
    FUNCTION("GetCAPadding", GameSettingsWrappers::GetCAPadding)

    // SSAO
    FUNCTION("SetSSAOEnabled", GameSettingsWrappers::SetSSAOEnabled)
    FUNCTION("GetSSAOEnabled", GameSettingsWrappers::GetSSAOEnabled)
    FUNCTION("SetSSAORadius", GameSettingsWrappers::SetSSAORadius)
    FUNCTION("GetSSAORadius", GameSettingsWrappers::GetSSAORadius)
    FUNCTION("SetSSAOBias", GameSettingsWrappers::SetSSAOBias)
    FUNCTION("GetSSAOBias", GameSettingsWrappers::GetSSAOBias)
    FUNCTION("SetSSAOIntensity", GameSettingsWrappers::SetSSAOIntensity)
    FUNCTION("GetSSAOIntensity", GameSettingsWrappers::GetSSAOIntensity)

    // Default values (for UI reset)
    FUNCTION("GetDefaultMasterVolume", GameSettingsWrappers::GetDefaultMasterVolume)
    FUNCTION("GetDefaultBGMVolume", GameSettingsWrappers::GetDefaultBGMVolume)
    FUNCTION("GetDefaultSFXVolume", GameSettingsWrappers::GetDefaultSFXVolume)
    FUNCTION("GetDefaultGamma", GameSettingsWrappers::GetDefaultGamma)
    FUNCTION("GetDefaultExposure", GameSettingsWrappers::GetDefaultExposure)
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
    FUNCTION("SetPosition", CharacterControllerWrappers::SetPosition)
    FUNCTION("GetPosition", CharacterControllerWrappers::GetPosition)
    FUNCTION("IsGrounded", CharacterControllerWrappers::IsGrounded)
    FUNCTION("SetVelocity", CharacterControllerWrappers::SetVelocity)
    FUNCTION("GetVelocity", CharacterControllerWrappers::GetVelocity)
    FUNCTION("GetGravity", CharacterControllerWrappers::GetGravity)
    FUNCTION("SetGravity", CharacterControllerWrappers::SetGravity)
    FUNCTION("DestroyByEntity", CharacterControllerWrappers::DestroyByEntity)
    FUNCTION("DisableCollision", CharacterControllerWrappers::DisableCollision)
    FUNCTION("UpdateAll", CharacterControllerWrappers::UpdateAll)
    FUNCTION("ClearAll", CharacterControllerWrappers::ClearAll)
    FUNCTION("SetJuggleMode", CharacterControllerWrappers::SetJuggleMode)
    FUNCTION("IsJuggling", CharacterControllerWrappers::IsJuggling)
    FUNCTION("SetMass", CharacterControllerWrappers::SetMass)
    FUNCTION("GetMass", CharacterControllerWrappers::GetMass)
    FUNCTION("SetImmovable", CharacterControllerWrappers::SetImmovable)
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
    FUNCTION("RaycastFull", PhysicsSystemWrappers::RaycastFull)
    FUNCTION("RaycastGetEntity", PhysicsSystemWrappers::RaycastGetEntity)
    FUNCTION("CheckDistance", PhysicsSystemWrappers::CheckDistance)
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
    FUNCTION("FindTransformByID", &Lua_FindTransformByID)
    FUNCTION("GetTransformPosition", &Lua_GetTransformPosition)
    FUNCTION("GetTransformWorldPosition", &Lua_GetTransformWorldPosition)
    FUNCTION("GetClipIndexByName", &Lua_FindCurrentClipByName)      //TEMPHACK
    FUNCTION("GetTransformRotation", &Lua_GetTransformRotation)
    FUNCTION("GetEntityByName", &Lua_FindEntityByName)
    //FUNCTION("GetEntitiesByTag", Lua_FindEntitiesByTag) //DONE & INITIALIZED DIRECTLY IN SCRIPT SYSTEMS FOR VECTOR TABLE
    FUNCTION("GetEntitiesByTag", Lua_GetEntitiesByTag)
    FUNCTION("GetGameCoordinate", &Lua_ScreenToGameCoordinates)
    FUNCTION("CreateEntityDup", &Lua_CreateEntityDup)
    FUNCTION("DestroyEntityDup", &Lua_DestroyEntityDup)
    FUNCTION("DestroyEntity", &Lua_DestroyEntity)
    FUNCTION("FindAnimatorByName", &Lua_FindAnimatorByName)

    FUNCTION("FindEntitiesWithScript", &EntityQueryWrappers::FindEntitiesWithScript)
    FUNCTION("GetEntityPosition", &EntityQueryWrappers::GetEntityPosition)
    FUNCTION("GetEntityName", &EntityQueryWrappers::GetEntityName)
    FUNCTION("GetEntityTag", &EntityQueryWrappers::GetEntityTag)
    FUNCTION("GetEntityLayer", &EntityQueryWrappers::GetEntityLayer)
    FUNCTION("IsEntityActive", &EntityQueryWrappers::IsEntityActive)
    FUNCTION("GetChildrenEntities", &EntityQueryWrappers::GetChildrenEntities)
    FUNCTION("GetChildAtIndex", &EntityQueryWrappers::GetChildAtIndex)
    FUNCTION("GetChildCount", &EntityQueryWrappers::GetChildCount)
    FUNCTION("FindChildByName", &EntityQueryWrappers::FindChildEntityByName)
    FUNCTION("GetParentEntity", &EntityQueryWrappers::GetParentEntity)
    FUNCTION("SetParentEntity", &EntityQueryWrappers::SetParentEntity)

    FUNCTION("UpdateCacheTiming", &EntityQueryWrappers::UpdateCacheTiming)
    FUNCTION("UpdateEnemyCache", &EntityQueryWrappers::UpdateEnemyCache)
    FUNCTION("SetCacheUpdateInterval", &EntityQueryWrappers::SetCacheUpdateInterval)
    FUNCTION("ClearEnemyCaches", &EntityQueryWrappers::ClearEnemyCaches)
    FUNCTION("GetCacheInfo", &EntityQueryWrappers::GetCacheInfo)

    FUNCTION("FindAudioCompByName", &Lua_FindAudioCompByName)
    FUNCTION("FindAudioCompByID", &Lua_FindAudioCompByID)

    FUNCTION("SetModelOpacity", &MaterialWrappers::SetModelOpacity)
    FUNCTION("GetModelOpacity", &MaterialWrappers::GetModelOpacity)
END_SYSTEM()



// ============================================================================
// NAV SYSTEM
// ============================================================================
BEGIN_SYSTEM("Nav")
    FUNCTION("RequestPathXZ", NavWrappers::RequestPathXZ)
    FUNCTION("GetGroundY", NavWrappers::GetGroundY)
END_SYSTEM()

// ============================================================================
// PREFAB INSTANTIATION
// ============================================================================
BEGIN_SYSTEM("Prefab")
    FUNCTION("InstantiatePrefab", &PrefabWrappers::InstantiatePrefab)
END_SYSTEM()

// ============================================================================
// DIALOGUE MANAGER
// ============================================================================

BEGIN_SYSTEM("DialogueManager")
    FUNCTION("StartDialogue", &DialogueManagerWrappers::StartDialogue)
    FUNCTION("StopDialogue", &DialogueManagerWrappers::StopDialogue)
    FUNCTION("ScrollNext", &DialogueManagerWrappers::ScrollNext)
    FUNCTION("IsDialogueActive", &DialogueManagerWrappers::IsDialogueActive)
    FUNCTION("IsAnyDialogueActive", &DialogueManagerWrappers::IsAnyDialogueActive)
    FUNCTION("GetCurrentIndex", &DialogueManagerWrappers::GetCurrentIndex)
    FUNCTION("GetCurrentEntryAutoTime", &DialogueManagerWrappers::GetCurrentEntryAutoTime)
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
