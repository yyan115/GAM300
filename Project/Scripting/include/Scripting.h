// include/Scripting.h
#pragma once
#include <cstdint>

// DLL export/import macro - Scripting is compiled into Engine.dll
#ifdef _WIN32
    #ifdef ENGINE_EXPORTS
        #define SCRIPTING_API __declspec(dllexport)
    #else
        #define SCRIPTING_API __declspec(dllimport)
    #endif
#else
    #ifdef ENGINE_EXPORTS
        #define SCRIPTING_API __attribute__((visibility("default")))
    #else
        #define SCRIPTING_API
    #endif
#endif

// Public integration surface for the scripting subsystem.
//
// - Keeps the engine/editor decoupled from Lua internals:
//     * CreateInstanceFromFile returns an opaque int instance id (registry ref),
//       but editors should test validity with IsValidInstance() rather than
//       comparing to LUA_NOREF.
//     * Host log handler takes a single formatted string (std::string).
// - All APIs are main-thread only unless noted otherwise.
//
// Minimal usage:
//   Scripting::Init();
//   Scripting::SetHostLogHandler([](const std::string& s){ ENGINE_PRINT(s.c_str()); });
//   int inst = Scripting::CreateInstanceFromFile("path.lua");
//   if (Scripting::IsValidInstance(inst)) { Scripting::CallInstanceFunction(inst, "Awake"); }
//   Scripting::DestroyInstance(inst);
//   Scripting::Shutdown();

#include <string>
#include <vector>
#include <functional>

extern "C" { struct lua_State; }

namespace Scripting {

    using HostLogFn = std::function<void(const std::string&)>; // scripts pass one formatted string
    using ReadAllTextFn = std::function<bool(const std::string& path, std::string& out)>;

    using EnvironmentId = uint32_t;
    static constexpr EnvironmentId InvalidEnvironmentId = 0u;

    struct InitOptions {
        bool createNewVM = true;
        bool openLibs = true; // luaL_openlibs
    };

    // Initialize/shutdown
    SCRIPTING_API bool Init(const InitOptions& opts = InitOptions{});
    SCRIPTING_API void Shutdown();

    // If the engine wants to provide its own lua_State, call SetLuaState() before other calls.
    // If Init was called with createNewVM == true, the library will own the VM and Shutdown() will close it.
    SCRIPTING_API void SetLuaState(lua_State* L);
    SCRIPTING_API lua_State* GetLuaState(); // may return nullptr if uninitialized

    // Per-frame tick. Main-thread only. Must be called frequently to advance coroutines.
    SCRIPTING_API void Tick(float dtSeconds);

    // Create/destroy instances (returns a registry-ref-like int). Use IsValidInstance() to check.
    SCRIPTING_API int CreateInstanceFromFile(const std::string& scriptPath);
    SCRIPTING_API void DestroyInstance(int instanceRef);
    SCRIPTING_API bool IsValidInstance(int instanceRef);

    // Call named function on instance (no variadic args support in this helper).
    // Returns true on success (function executed with no errors).
    SCRIPTING_API bool CallInstanceFunction(int instanceRef, const std::string& funcName);

    // Host logger injection (single string parameter). The binding `cpp_log(s)` will call this handler.
    // If not set, messages go to ENGINE_PRINT with LogLevel::Info.
    SCRIPTING_API void SetHostLogHandler(HostLogFn fn);

    // Allow engine to override script file reading (editor may read from virtual FS).
    SCRIPTING_API void SetFileSystemReadAllText(ReadAllTextFn fn);

    // Host -> Scripting: callback to resolve a component for a given entity.
    // Semantic: callback receives the lua_State* and must push exactly one Lua value
    // (the component representation) on the Lua stack and return true on success.
    // If the callback returns false or pushes nothing, the scripting runtime will return nil.
    using HostGetComponentFn = std::function<bool(lua_State* L, uint32_t entityId, const std::string& compName)>;

    // Set handler (engine should call this early in initialization).
    SCRIPTING_API void SetHostGetComponentHandler(HostGetComponentFn fn);

    // Bind a scripting instance registry-ref to an entity id. This will:
    //  - set instance.entityId = <entityId>
    //  - set instance:GetComponent(name) helper that forwards to GetComponent(entityId, name)
    // Call from engine after creating/attaching a ScriptComponent and after the instance exists.
    SCRIPTING_API bool BindInstanceToEntity(int instanceRef, uint32_t entityId);

    // Serializer/inspector wrappers (thin)
    SCRIPTING_API std::string SerializeInstanceToJson(int instanceRef);
    SCRIPTING_API bool DeserializeJsonToInstance(int instanceRef, const std::string& json);

    // StatePreserver wrappers
    SCRIPTING_API void RegisterInstancePreserveKeys(int instanceRef, const std::vector<std::string>& keys);
    SCRIPTING_API std::string ExtractInstancePreserveState(int instanceRef);
    SCRIPTING_API bool ReinjectInstancePreserveState(int instanceRef, const std::string& json);

    // Hot-reload helpers
    SCRIPTING_API void EnableHotReload(bool enable);
    SCRIPTING_API void RequestReloadNow();

    // Coroutine scheduler control (optional)
    SCRIPTING_API void InitializeCoroutineScheduler();
    SCRIPTING_API void ShutdownCoroutineScheduler();

} // namespace Scripting
