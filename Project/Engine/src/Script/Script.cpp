#include "pch.h"
#include "Script/Script.hpp"
#include "Logging.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <Asset Manager/AssetManager.hpp>

// define static
std::atomic<bool> Script::s_fsRegistered{ false };

namespace {
    // small helper that reads file content from disk
    static bool ReadAllTextFromDisk(const std::string& path, std::string& out) {
        std::ifstream ifs(path, std::ios::in | std::ios::binary);
        if (!ifs) return false;
        std::ostringstream ss;
        ss << ifs.rdbuf();
        out = ss.str();
        return true;
    }

    // engine-provided wrapper for Scripting::SetFileSystemReadAllText
    static Scripting::ReadAllTextFn MakeReadAllTextFn() {
        return [](const std::string& p, std::string& out) -> bool {
            // You can extend this to consult your VFS / asset manager instead of raw disk.
            return ReadAllTextFromDisk(p, out);
            };
    }
}

void Script::EnsureFsCallbackRegistered() {
    bool expected = false;
    if (s_fsRegistered.compare_exchange_strong(expected, true)) {
        // first time - install engine FS hook into scripting subsystem
        try {
            Scripting::SetFileSystemReadAllText(MakeReadAllTextFn());
        }
        catch (...) {
            // set flag back? We'll keep registered=true to avoid repeated exceptions.
            // Log a warning through engine logging macro
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "Script::EnsureFsCallbackRegistered - SetFileSystemReadAllText threw");
        }
    }
}

Script::Script() = default;
Script::Script(const std::string& name) : m_name(name) {}
Script::Script(std::shared_ptr<AssetMeta> meta) {
    if (!meta) return;

    // Use the sourceFilePath as canonical script path if available, otherwise compiledFilePath.
    // Many asset pipelines store the original path in sourceFilePath; prefer that.
    if (!meta->sourceFilePath.empty()) {
        m_scriptPath = meta->sourceFilePath;
    }
    else if (!meta->compiledFilePath.empty()) {
        m_scriptPath = meta->compiledFilePath;
    }

    // Name: use the filename from sourceFilePath if present, otherwise fall back to compiled path.
    std::filesystem::path p = m_scriptPath.empty() ? std::filesystem::path() : std::filesystem::path(m_scriptPath);
    m_name = p.empty() ? std::string("script") : p.stem().string();

    // Optionally store version info or last compile time for debugging
    // You can use meta->version / meta->lastCompileTime if needed.
    m_loaded = !m_scriptPath.empty();
}

Script::~Script() {
    DestroyInstance();
}

std::string Script::CompileToResource(const std::string& assetPath, bool forAndroid) {
    // by default, use the script file as-is. If you have a compilation step (bytecode), do it here.
    if (!forAndroid)
        return assetPath;
    else {
        std::string assetPathAndroid = assetPath.substr(assetPath.find("Resources"));
        assetPathAndroid = (AssetManager::GetInstance().GetAndroidResourcesPath() / assetPathAndroid).generic_string();

        // Ensure parent directories exist
        std::filesystem::path outputPath(assetPathAndroid);
        std::filesystem::create_directories(outputPath.parent_path());

        try {
            // Copy the audio file to the Android assets location
            std::filesystem::copy_file(assetPath, assetPathAndroid, std::filesystem::copy_options::overwrite_existing);
        }
        catch (const std::filesystem::filesystem_error& e) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[Script] Failed to copy script file for Android: ", e.what(), "\n");
            return std::string{};
        }

        return assetPathAndroid;
    }
}

bool Script::LoadResource(const std::string& resourcePath, const std::string& assetPath) {
    std::lock_guard<std::mutex> lk(m_mutex);

    // Prefer 'resourcePath' (compiled resource), fallback to assetPath (source)
    m_scriptPath = !resourcePath.empty() ? resourcePath : assetPath;
    if (m_scriptPath.empty()) return false;

    m_loaded = true;
    EnsureFsCallbackRegistered();

    // Attempt to create instance immediately (engine should already have initialized Scripting)
    return CreateInstanceInternal();
}


bool Script::ReloadResource(const std::string& resourcePath, const std::string& assetPath) {
    assetPath;
    std::lock_guard<std::mutex> lk(m_mutex);
    // update path if provided
    if (!resourcePath.empty()) m_scriptPath = resourcePath;
    if (m_scriptPath.empty()) return false;

    // Preserve state if configured
    std::string preserved;
    if (Scripting::IsValidInstance(m_instanceId) && !m_options.preserveKeys.empty()) {
        // Extract preserve state via public API
        preserved = ExtractPreserveState();
    }

    // Destroy old instance
    if (Scripting::IsValidInstance(m_instanceId)) {
        Scripting::DestroyInstance(m_instanceId);
        m_instanceId = -1;
    }

    m_loaded = true;
    EnsureFsCallbackRegistered();

    // Create a fresh instance
    if (!CreateInstanceInternal()) return false;

    // Reinject preserved state if any
    if (!preserved.empty()) {
        ReinjectPreserveState(preserved);
    }

    return true;
}

std::shared_ptr<AssetMeta> Script::ExtendMetaFile(const std::string& assetPath,
    std::shared_ptr<AssetMeta> currentMetaData,
    bool forAndroid) {
    // If caller already gave an AssetMeta, just return it (we don't modify unknown fields).
    if (currentMetaData) {
        // If compiled path missing, set it to compiled form of assetPath (no compilation step here).
        if (currentMetaData->compiledFilePath.empty()) {
            currentMetaData->compiledFilePath = CompileToResource(assetPath, forAndroid);
            //currentMetaData->lastCompileTime = std::chrono::system_clock::now();
        }
        return currentMetaData;
    }

    // Create a minimal meta with sensible defaults.
    auto meta = std::make_shared<AssetMeta>();
    meta->sourceFilePath = assetPath;
    meta->compiledFilePath = CompileToResource(assetPath, forAndroid);
    meta->androidCompiledFilePath = forAndroid ? meta->compiledFilePath : std::string();
    meta->version = 0;
    //meta->lastCompileTime = std::chrono::system_clock::now();

    // Note: We do not invent a GUID here; if your pipeline needs real GUIDs,
    // call meta->PopulateAssetMeta(...) from the asset builder with a real GUID.
    // Or let the asset manager populate it at import time.

    return meta;
}


bool Script::LoadFromFile(const std::filesystem::path& filePath, const Options& opts) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (filePath.empty()) return false;
    if (!std::filesystem::exists(filePath)) return false;

    m_scriptPath = filePath.string();
    m_options = opts;
    m_loaded = true;
    EnsureFsCallbackRegistered();

    // Create an instance immediately for ease-of-use. If engine wants defer, call CreateInstance manually.
    return CreateInstanceInternal();
}

bool Script::CreateInstance() {
    std::lock_guard<std::mutex> lk(m_mutex);
    return CreateInstanceInternal();
}

bool Script::CreateInstanceInternal() {
    // Respect the Scripting public API: do not access internals.
    if (!m_loaded || m_scriptPath.empty()) return false;

    // Ensure scripting subsystem initialized (engine-level responsibility)
    // Here we check GetLuaState() as a lightweight probe; the API may return nullptr if not initialized.
    if (!Scripting::GetLuaState()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "Script::CreateInstanceInternal - Scripting not initialized");
        return false;
    }

    int inst = Scripting::CreateInstanceFromFile(m_scriptPath);
    if (!Scripting::IsValidInstance(inst)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "Script::CreateInstanceInternal - CreateInstanceFromFile failed for ", m_scriptPath.c_str());
        m_instanceId = -1;
        return false;
    }

    m_instanceId = inst;

    // register preserve keys if provided
    if (!m_options.preserveKeys.empty()) {
        Scripting::RegisterInstancePreserveKeys(m_instanceId, m_options.preserveKeys);
    }

    // optionally invoke entry function
    if (m_options.autoInvokeEntry && !m_options.entryFunction.empty()) {
        // best-effort; log only on error
        bool ok = Scripting::CallInstanceFunction(m_instanceId, m_options.entryFunction);
        if (!ok) {
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "Script::CreateInstanceInternal - entry call failed: ", m_options.entryFunction.c_str());
        }
    }

    return true;
}

void Script::DestroyInstance() {
    std::lock_guard<std::mutex> lk(m_mutex);

    // Fast path: nothing to do
    if (m_instanceId < 0) {
        m_instanceId = -1;
        return;
    }

    // If scripting subsystem has no runtime / lua state, do not call into it.
    // Scripting::GetLuaState() returns nullptr if not initialized / already shutdown.
    if (!Scripting::GetLuaState()) {
        // runtime gone — avoid calling into Scripting; just drop our reference and return.
        ENGINE_PRINT(EngineLogging::LogLevel::Warn,
            "Script::DestroyInstance: scripting runtime not available; dropping instance id=", m_instanceId);
        m_instanceId = -1;
        return;
    }

    // Optional: ensure we are on the main thread. If your engine exposes a way to check that,
    // use it here. For example: if (!Engine::IsMainThread()) { schedule to main thread; return; }
    // The mutex here does NOT make Lua calls thread-safe.
#if defined(ENABLE_MAIN_THREAD_CHECK)
    if (!Engine::IsMainThread()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "Script::DestroyInstance called off main thread; scheduling destroy on main thread");
        // Schedule a lambda on the main thread that calls DestroyInstance() again
        Engine::ScheduleOnMainThread([this_ptr = shared_from_this()]() { this_ptr->DestroyInstance(); });
        return;
    }
#endif

    // Best-effort call OnShutdown (if provided). Protect call with return-value checks.
    if (Scripting::IsValidInstance(m_instanceId)) {
        bool called = Scripting::CallInstanceFunction(m_instanceId, "OnShutdown");
        if (!called) {
            ENGINE_PRINT(EngineLogging::LogLevel::Debug, "Script::DestroyInstance: OnShutdown not found or call failed for instance ", m_instanceId);
        }

        // Destroy the instance through the public API.
        Scripting::DestroyInstance(m_instanceId);
    }

    m_instanceId = -1;
}

bool Script::IsInstanceValid() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return Scripting::IsValidInstance(m_instanceId);
}

bool Script::Call(const std::string& functionName) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!Scripting::IsValidInstance(m_instanceId)) return false;
    if (functionName.empty()) return false;
    return Scripting::CallInstanceFunction(m_instanceId, functionName);
}

std::string Script::SerializeInstance() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!Scripting::IsValidInstance(m_instanceId)) return {};
    return Scripting::SerializeInstanceToJson(m_instanceId);
}

bool Script::DeserializeInstance(const std::string& json) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!Scripting::IsValidInstance(m_instanceId)) return false;
    return Scripting::DeserializeJsonToInstance(m_instanceId, json);
}

void Script::RegisterPreserveKeys(const std::vector<std::string>& keys) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_options.preserveKeys = keys;
    if (Scripting::IsValidInstance(m_instanceId)) {
        Scripting::RegisterInstancePreserveKeys(m_instanceId, keys);
    }
}

std::string Script::ExtractPreserveState() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!Scripting::IsValidInstance(m_instanceId)) return {};
    return Scripting::ExtractInstancePreserveState(m_instanceId);
}

bool Script::ReinjectPreserveState(const std::string& json) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!Scripting::IsValidInstance(m_instanceId)) return false;
    return Scripting::ReinjectInstancePreserveState(m_instanceId, json);
}
