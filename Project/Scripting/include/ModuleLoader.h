#pragma once
// ModuleLoader.h
// Lightweight module/require loader for Lua modules backed by IScriptFileSystem.

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>

extern "C" { struct lua_State; }

namespace Scripting {

    // Forward-declare IScriptFileSystem in the Scripting namespace (important).
    struct IScriptFileSystem;

    class ModuleLoader {
    public:
        ModuleLoader();
        ~ModuleLoader();

        // Initialize with a filesystem (not owned). Call before using.
        void Initialize(std::shared_ptr<IScriptFileSystem> fs);

        // Add search path pattern. Use '?' as the module-name placeholder.
        // e.g. "Resources/Scripts/?.lua", "Resources/Scripts/?/init.lua"
        void AddSearchPath(const std::string& pattern);

        // Insert the loader into Lua's package.searchers table at the given position (1-based).
        // If pos is -1, append at the end.
        void InstallLuaSearcher(lua_State* L, int pos = -1);

        // Resolve module name to a filesystem path (returns empty if not found).
        std::string ResolveModuleName(const std::string& modulename);

        // Flush a specific module from package.loaded and internal cache (main-thread).
        void FlushModule(lua_State* L, const std::string& modulename);

        // Flush all modules (package.loaded entries for modules resolved by this loader).
        void FlushAll(lua_State* L);

        // Reload a module: flush, then call require(name). Returns true on success.
        bool ReloadModule(lua_State* L, const std::string& modulename);

        void ClearSearchPaths();
    private:
#ifdef ANDROID
        friend int AndroidAssetSearcher_CFunc(lua_State* L);
#endif
        // The C function used as a Lua loader/searcher, bound per lua_State.
        static int LuaLoader_CFunc(lua_State* L);

        // instance method invoked by C func (uses lua_State* from stack)
        int LuaLoader_Impl(lua_State* L);

        // Helper to try patterns and use fs->ReadAllText
        bool TryLoadPath(lua_State* L, const std::string& path, const std::string& modulename);

        std::shared_ptr<IScriptFileSystem> m_ownedFs; // Owned/shared filesystem to guarantee lifetime.
        IScriptFileSystem* m_fs = nullptr; // still the pointer used for calls (may point into m_ownedFs.get())

        std::vector<std::string> m_searchPaths;
        std::mutex m_mutex;

        // cache: moduleName -> resolved path (or empty if not found previously).
        std::unordered_map<std::string, std::string> m_resolveCache;
    };

} // namespace Scripting
