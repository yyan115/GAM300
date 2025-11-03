#pragma once
// ModuleLoader.h
// Lightweight module/require loader for Lua modules backed by IScriptFileSystem.
//
// Responsibilities:
//  - Resolve module names to script asset paths using simple search-path patterns.
//  - Provide a Lua searcher (loader) to insert into package.searchers.
//  - Cache resolved paths and allow flushing package.loaded and cache entries for hot-reload.

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>

extern "C" { struct lua_State; }

struct IScriptFileSystem; // forward (your existing interface)

namespace Scripting {

    class ModuleLoader {
    public:
        ModuleLoader();
        ~ModuleLoader();

        // Initialize with a filesystem (not owned). Call before using.
        void Initialize(IScriptFileSystem* fs);

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

    private:
        // The C function used as a Lua loader/searcher, bound per lua_State.
        static int LuaLoader_CFunc(lua_State* L);

        // instance method invoked by C func (uses lua_State* from stack)
        int LuaLoader_Impl(lua_State* L);

        // Helper to try patterns and use fs->ReadAllText
        bool TryLoadPath(lua_State* L, const std::string& path, const std::string& modulename);

        IScriptFileSystem* m_fs = nullptr; // not owned

        std::vector<std::string> m_searchPaths;
        std::mutex m_mutex;

        // cache: moduleName -> resolved path (or empty if not found previously).
        std::unordered_map<std::string, std::string> m_resolveCache;
    };

} // namespace Scripting
