#pragma once
/*
 * File: Script.h
 * @brief Bridge asset that exposes a script file/instance to the engine via the public Scripting API.
 *
 * This class is an engine-side asset wrapper that manages a script file and a single runtime
 * "instance" created by the scripting subsystem. It intentionally uses *only* the public
 * Scripting API (Scripting::CreateInstanceFromFile, ::DestroyInstance, ::CallInstanceFunction,
 * ::SerializeInstanceToJson, ::DeserializeJsonToInstance, ::SetFileSystemReadAllText, etc.) and
 * purposefully does NOT touch Lua internals or ScriptingRuntime internals. This keeps the engine
 * decoupled from the runtime implementation and enforces the intended public surface.
 *
 * Responsibilities
 *  - Hold metadata (path, options) for a script asset.
 *  - Request/create/destroy opaque instances through Scripting::CreateInstanceFromFile / DestroyInstance.
 *  - Provide convenient engine-facing helpers for calling instance functions (no-arg calls).
 *  - Support state snapshot/restore via public serialization and state-preserver APIs.
 *  - Register an engine filesystem callback (via Scripting::SetFileSystemReadAllText) if none is set.
 *
 * Lifecycle & ownership
 *  - Engine MUST call Scripting::Init(...) before creating Script instances.
 *  - Script::LoadResource / LoadFromFile mark the asset loaded and will attempt to create an instance.
 *  - Script::DestroyInstance will call Scripting::CallInstanceFunction(instance, "OnShutdown") (best-effort)
 *    then Scripting::DestroyInstance(instance).
 *  - Reload: Script extracts preserve-state if requested, destroys the instance, creates a new instance,
 *    and reinjects preserve-state (using only the Scripting public API).
 *
 * Threading
 *  - The Scripting public API is MAIN-THREAD ONLY. Script uses a mutex to protect its own state,
 *    but this does NOT make Lua calls safe from background threads. Only call Scripting APIs from
 *    the main thread (or otherwise follow your runtime's documented thread model).
 *
 * Error handling & logging
 *  - Script logs failures (create/call/serialize) and fails gracefully if the scripting subsystem
 *    is not initialized. Consumers should handle boolean returns accordingly.
 *
 * Design notes / rationale
 *  - Do not call lua_* or touch ScriptingRuntime internals here. If you need richer call semantics
 *    (typed arguments, returns), add a matching API to the Scripting public header and delegate to it.
 *  - The class is a thin adapter for engine-level usage (asset manager, editor, runtime). Hot-reload and
 *    preserve-state are implemented using the public state-preserver / serializer functions.
 *
 * Example usage
 *   // Engine bootstrap
 *   Scripting::Init({ .createNewVM = true, .openLibs = true });
 *   Scripting::SetHostLogHandler([](const std::string &s){ ENGINE_LOG(s); });
 *   Scripting::SetFileSystemReadAllText(vfsReadAllTextFn); // optional (editor VFS)
 *
 *   // Load script asset (via AssetManager)
 *   auto s = std::make_shared<Script>("player.lua");
 *   s->LoadFromFile("Resources/Scripts/player.lua");
 *   if (s->IsInstanceValid()) s->Call("OnSpawn");
 *
 *   // Hot reload while preserving some keys
 *   s->RegisterPreserveKeys({ "position", "health" });
 *   s->ReloadResource("Resources/Scripts/player.lua");
 *
 * Common pitfalls
 *  - Calling CreateInstanceFromFile before Scripting::Init -> instance creation fails.
 *  - Not setting the FS callback early enough -> Module loader can't find modules in editor VFS.
 *  - Registering preserve keys that don't exist -> no effect; validate keys in tools.
 *
 * @author  <your name>
 * @date    <yyyy-mm-dd>
 * @copyright <your org / license>
 */

#include <string>
#include <memory>
#include <optional>
#include <mutex>
#include <filesystem>
#include "Asset Manager/Asset.hpp"
#include "../Engine.h"

// Scripting public API header (only the public surface)
#include "../../Scripting/include/Scripting.h"

class Script : public IAsset {
public:
    struct Options {
        std::string entryFunction = "OnInit";
        bool autoInvokeEntry = true;
        // Keys to preserve across reloads (optional)
        std::vector<std::string> preserveKeys;
    };

    // ctors/dtor
    ENGINE_API Script();
    ENGINE_API Script(const std::string& name);
    ENGINE_API Script(std::shared_ptr<AssetMeta> meta);
    ENGINE_API ~Script();

    Script(const Script&) = delete;
    Script& operator=(const Script&) = delete;

    // IAsset overrides
    std::string CompileToResource(const std::string& assetPath, bool forAndroid = false) override;
    bool LoadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
    bool ReloadResource(const std::string& resourcePath, const std::string& assetPath = "") override;
    std::shared_ptr<AssetMeta> ExtendMetaFile(const std::string& assetPath,
        std::shared_ptr<AssetMeta> currentMetaData,
        bool forAndroid = false) override;

    // engine-facing API
    ENGINE_API bool LoadFromFile(const std::filesystem::path& filePath, const Options& opts = {});
    ENGINE_API bool CreateInstance();             // call CreateInstanceFromFile if loaded
    ENGINE_API void DestroyInstance();            // calls Scripting::DestroyInstance
    ENGINE_API bool IsInstanceValid() const;      // uses Scripting::IsValidInstance

    // Call functions on the instance (no args). Returns false on error or missing instance/method.
    ENGINE_API bool Call(const std::string& functionName) const;

    // Serialize/restore helpers (uses Scripting::SerializeInstanceToJson / DeserializeJsonToInstance)
    ENGINE_API std::string SerializeInstance() const;
    ENGINE_API bool DeserializeInstance(const std::string& json) const;

    // State-preserver helpers (wrap RegisterInstancePreserveKeys / Extract/Reinject)
    ENGINE_API void RegisterPreserveKeys(const std::vector<std::string>& keys);
    ENGINE_API std::string ExtractPreserveState() const;
    ENGINE_API bool ReinjectPreserveState(const std::string& json) const;

    ENGINE_API const std::string& GetScriptPath() const { return m_scriptPath; }
    ENGINE_API int GetInstanceId() const { return m_instanceId; }
    ENGINE_API void SetOptions(const Options& o) { m_options = o; }
    ENGINE_API const Options& GetOptions() const { return m_options; }

private:
    // Use only Scripting public API
    bool CreateInstanceInternal(); // called under mutex; returns success

    // register engine FS reader into Scripting subsystem (idempotent)
    static void EnsureFsCallbackRegistered();

private:
    std::string m_name;
    std::string m_scriptPath;
    Options m_options;

    // scripting instance id (opaque int returned by CreateInstanceFromFile)
    // instanceRef == -1 indicates none, but use Scripting::IsValidInstance() when checking.
    int m_instanceId = -1;

    bool m_loaded = false;

    mutable std::mutex m_mutex;

    // ensure the FS callback is registered only once across all Script instances
    static std::atomic<bool> s_fsRegistered;
};
