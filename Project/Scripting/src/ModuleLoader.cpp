#include "ModuleLoader.h"
#include "ScriptFileSystem.h" // for IScriptFileSystem
#include "Logging.hpp"

#ifdef ANDROID
#include "WindowManager.hpp" // for WindowManager::GetPlatform()
#include "Platform/AndroidPlatform.h"
#endif

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <sstream>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <fstream>

static inline int lua_table_len_compat(lua_State* L, int idx) {
#if LUA_VERSION_NUM >= 502
    return (int)lua_rawlen(L, idx);
#else
    return (int)lua_objlen(L, idx);
#endif
}

namespace Scripting {

    ModuleLoader::ModuleLoader() = default;
    ModuleLoader::~ModuleLoader() = default;

    void ModuleLoader::Initialize(std::shared_ptr<IScriptFileSystem> fs)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (fs) {
            m_ownedFs = std::move(fs);
            m_fs = m_ownedFs.get();
        }
        else {
            // Leave m_ownedFs empty and m_fs null — callers may set search paths but loader can't read files.
            m_ownedFs.reset();
            m_fs = nullptr;
        }
        if (m_searchPaths.empty()) {
            m_searchPaths.push_back("Resources/Scripts/?.lua");
            m_searchPaths.push_back("Resources/Scripts/extension/?.lua");
            m_searchPaths.push_back("Resources/Scripts/?/init.lua");
        }
    }

    void ModuleLoader::AddSearchPath(const std::string& pattern) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_searchPaths.push_back(pattern);
    }

#ifdef ANDROID
    // Forward declare the friend function
    int AndroidAssetSearcher_CFunc(lua_State* L);

    // Android asset searcher - handles module loading from APK assets
    // This is a free function (not a member) that's declared as friend in the header
    int AndroidAssetSearcher_CFunc(lua_State* L) {
        // upvalue 1 is the ModuleLoader* (lightuserdata)
        void* ud = lua_touserdata(L, lua_upvalueindex(1));
        ModuleLoader* loader = static_cast<ModuleLoader*>(ud);
        if (!loader) {
            lua_pushstring(L, "\n\tAndroidAssetSearcher: ModuleLoader missing in upvalue");
            return 1;
        }

        const char* moduleName = luaL_checkstring(L, 1);
        if (!moduleName) {
            lua_pushstring(L, "\n\tmodule name is nil");
            return 1;
        }

        // Convert module name to path (e.g., "extension.engine_bootstrap" -> "extension/engine_bootstrap")
        std::string modulePath = moduleName;
        std::replace(modulePath.begin(), modulePath.end(), '.', '/');

        // Build search paths based on configured patterns
        std::vector<std::string> searchPaths;
        {
            std::lock_guard<std::mutex> lk(loader->m_mutex);
            for (const auto& pattern : loader->m_searchPaths) {
                size_t pos = pattern.find('?');
                if (pos != std::string::npos) {
                    searchPaths.push_back(pattern.substr(0, pos) + modulePath + pattern.substr(pos + 1));
                }
            }
        }

        // Get platform interface for asset access
        IPlatform* platform = WindowManager::GetPlatform();
        if (!platform) {
            std::string err = std::string("\n\tno module '") + moduleName +
                "' in Android assets (platform unavailable)";
            lua_pushstring(L, err.c_str());
            return 1;
        }

        // Try each search path
        for (const auto& path : searchPaths) {
            ENGINE_PRINT(EngineLogging::LogLevel::Debug,
                "AndroidAssetSearcher trying: ", path.c_str());

            if (platform->FileExists(path)) {
                std::vector<uint8_t> scriptData = platform->ReadAsset(path);
                if (!scriptData.empty()) {
                    // Load the module
                    int loadStatus = luaL_loadbuffer(L,
                        reinterpret_cast<const char*>(scriptData.data()),
                        scriptData.size(),
                        path.c_str());

                    if (loadStatus == LUA_OK) {
                        ENGINE_PRINT(EngineLogging::LogLevel::Info,
                            "AndroidAssetSearcher loaded: ", path.c_str());
                        return 1; // Return the loaded chunk
                    }
                    else {
                        // Load error - error message is already on stack
                        return 1;
                    }
                }
            }
        }

        // Not found - return error message
        std::string err = std::string("\n\tno module '") + moduleName +
            "' in Android assets";
        lua_pushstring(L, err.c_str());
        return 1;
    }
#endif

    void ModuleLoader::InstallLuaSearcher(lua_State* L, int pos) {
        if (!L) return;
        ENGINE_PRINT(EngineLogging::LogLevel::Info, "ModuleLoader::InstallLuaSearcher called (pos=", pos, ")");

        // get package.searchers (Lua 5.2+ uses package.searchers, older uses package.loaders)
        lua_getglobal(L, "package");
#if LUA_VERSION_NUM >= 502
        lua_getfield(L, -1, "searchers");
#else
        lua_getfield(L, -1, "loaders");
#endif
        if (!lua_istable(L, -1)) {
            lua_pop(L, 2);
            ENGINE_PRINT(EngineLogging::LogLevel::Warn, "ModuleLoader::InstallLuaSearcher - package.searchers not a table");
            return;
        }

#ifdef ANDROID
        // On Android, install the AndroidAssetSearcher instead of the regular loader
        lua_pushlightuserdata(L, this);
        lua_pushcclosure(L, &AndroidAssetSearcher_CFunc, 1);
        ENGINE_PRINT(EngineLogging::LogLevel::Info, "ModuleLoader: Installing AndroidAssetSearcher");
#else
        // On other platforms, use the regular ModuleLoader
        lua_pushlightuserdata(L, this);
        lua_pushcclosure(L, &ModuleLoader::LuaLoader_CFunc, 1);
        ENGINE_PRINT(EngineLogging::LogLevel::Info, "ModuleLoader: Installing regular searcher");
#endif

        // Insert it into the searchers table at pos (1-based). If pos == -1 append.
        int insertPos = pos;
        if (insertPos == -1) {
            // append: get length + 1
            int len = lua_table_len_compat(L, -2); // package.searchers is below closure we just pushed
            lua_rawseti(L, -2, len + 1); // pops our function and sets at len+1
        }
        else {
            // shift elements to make room: simple method - move elements up
            int len = lua_table_len_compat(L, -2);
            for (int i = len; i >= insertPos; --i) {
                lua_rawgeti(L, -2, i);
                lua_rawseti(L, -2, i + 1);
            }
            // now set at insertPos
            lua_rawseti(L, -2, insertPos); // pops our function
        }

        lua_pop(L, 2); // pop searchers and package
    }

    std::string ModuleLoader::ResolveModuleName(const std::string& modulename) {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it = m_resolveCache.find(modulename);
            if (it != m_resolveCache.end()) return it->second;
        }

        // transform module name "foo.bar" -> path fragment "foo/bar"
        std::string qname = modulename;
        std::replace(qname.begin(), qname.end(), '.', '/');

        std::lock_guard<std::mutex> lk(m_mutex);
        ENGINE_PRINT(EngineLogging::LogLevel::Info, "ModuleLoader::ResolveModuleName called for '", modulename.c_str(),
            "' (this=", (void*)this, ", m_fs=", (void*)m_fs, ")");

        for (const auto& pat : m_searchPaths) {
            std::string candidate;
            candidate.reserve(pat.size() + qname.size());

            // replace single '?' with qname (simple policy)
            size_t p = pat.find('?');
            if (p != std::string::npos) {
                candidate = pat.substr(0, p) + qname + pat.substr(p + 1);
            }
            else {
                candidate = pat;
            }

            ENGINE_PRINT(EngineLogging::LogLevel::Info, "ModuleLoader trying candidate: ", candidate.c_str());

            // ask FS if it exists / readable
            if (m_fs) {
                std::string content;
                if (m_fs->ReadAllText(candidate, content)) {
                    ENGINE_PRINT(EngineLogging::LogLevel::Info, "ModuleLoader found file via FS at: ", candidate.c_str());
                    m_resolveCache.emplace(modulename, candidate);
                    return candidate;
                }
                else {
                    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "ModuleLoader: FS couldn't read: ", candidate.c_str());
                }
            }
            else {
                // fallback: check file exists on host FS
                std::ifstream ifs(candidate, std::ios::binary);
                if (ifs.good()) {
                    ENGINE_PRINT(EngineLogging::LogLevel::Info, "ModuleLoader found file on host FS at: ", candidate.c_str());
                    m_resolveCache.emplace(modulename, candidate);
                    return candidate;
                }
                else {
                    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "ModuleLoader: no file at: ", candidate.c_str());
                }
            }
        }

        // not found: cache empty result to avoid repeated attempts
        m_resolveCache.emplace(modulename, std::string());
        return std::string();
    }

    int ModuleLoader::LuaLoader_CFunc(lua_State* L) {
        // upvalue 1 is the ModuleLoader* (lightuserdata)
        void* ud = lua_touserdata(L, lua_upvalueindex(1));
        ModuleLoader* loader = static_cast<ModuleLoader*>(ud);
        if (!loader) {
            lua_pushstring(L, "ModuleLoader missing in loader upvalue");
            return 1; // return error string per searcher contract
        }
        return loader->LuaLoader_Impl(L);
    }

    int ModuleLoader::LuaLoader_Impl(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        if (!name) {
            lua_pushstring(L, "module name is nil");
            return 1;
        }
        std::string modulename(name);
        std::string path = ResolveModuleName(modulename);
        if (path.empty()) {
            // not found: return error message (Lua will concatenate messages from searchers)
            std::string err = std::string("\n\tno module '") + modulename + "' in ModuleLoader search paths";
            lua_pushstring(L, err.c_str());
            return 1;
        }

        // try to load file content using our FS (if available) or fallback to luaL_loadfile
        if (m_fs) {
            std::string content;
            if (!m_fs->ReadAllText(path, content)) {
                std::string err = std::string("\n\tfailed to read module file: ") + path;
                lua_pushstring(L, err.c_str());
                return 1;
            }
            // load buffer; on success leaves function on stack; on error leaves error message
            int loadStatus = luaL_loadbuffer(L, content.c_str(), content.size(), path.c_str());
            if (loadStatus != LUA_OK) {
                // error message on stack already
                return 1;
            }
            return 1; // function is on stack for require to call
        }
        else {
            int loadStatus = luaL_loadfile(L, path.c_str());
            if (loadStatus != LUA_OK) {
                return 1; // error message on stack
            }
            return 1;
        }
    }

    bool ModuleLoader::TryLoadPath(lua_State* L, const std::string& path, const std::string& modulename) {
        // helper not currently used but kept for completeness
        (void)L; (void)path; (void)modulename;
        return false;
    }

    void ModuleLoader::FlushModule(lua_State* L, const std::string& modulename) {
        if (!L) return;
        // clear internal cache entry
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_resolveCache.erase(modulename);
        }
        // remove from package.loaded
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "loaded");
        if (lua_istable(L, -1)) {
            lua_pushstring(L, modulename.c_str());
            lua_pushnil(L);
            lua_settable(L, -3);
        }
        lua_pop(L, 2);
    }

    void ModuleLoader::FlushAll(lua_State* L) {
        if (!L) return;
        std::vector<std::string> keys;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            for (const auto& kv : m_resolveCache) {
                if (!kv.second.empty()) keys.push_back(kv.first);
            }
            m_resolveCache.clear();
        }
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "loaded");
        if (lua_istable(L, -1)) {
            for (const auto& name : keys) {
                lua_pushstring(L, name.c_str());
                lua_pushnil(L);
                lua_settable(L, -3);
            }
        }
        lua_pop(L, 2);
    }

    bool ModuleLoader::ReloadModule(lua_State* L, const std::string& modulename) {
        if (!L) return false;
        FlushModule(L, modulename);
        // call require(name)
        lua_getglobal(L, "require");
        lua_pushstring(L, modulename.c_str());
        if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            if (err) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "ModuleLoader::ReloadModule require failed: ", err);
            }
            lua_pop(L, 1);
            return false;
        }
        // success: module table/function returned on stack; pop it
        lua_pop(L, 1);
        return true;
    }

} // namespace Scripting