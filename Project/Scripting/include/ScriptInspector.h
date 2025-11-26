#pragma once
// ScriptInspector.h
// Editor API: reflect script public fields to inspector UI.
//
// - Helpers to enumerate script variables, metadata annotations, and converters
//   to/from native types for serialization.
// - Contains notes on how to generate stubs for editor autocomplete (API lua file)
//   and how to detect public fields (convention vs annotation).
// - Use cases: used by editor to show exposed script variables and to edit them at runtime.

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <memory>
#include <mutex>

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

extern "C" { struct lua_State; }

namespace Scripting {

    enum class FieldType {
        Nil,
        Number,
        Boolean,
        String,
        Table,
        Function,
        ReflectedUserdata,
        Other
    };

    // Minimal metadata bag for editor decoration (tooltips, range, display name)
    struct FieldMeta {
        std::string displayName;    // friendly name shown in UI
        std::string tooltip;        // help text
        std::string editorHint;     // e.g. "slider:0,1" or "color" (interpreted by editor)
        std::unordered_map<std::string, std::string> other; // freeform extra metadata
    };

    struct FieldInfo {
        std::string name;
        FieldType type;
        FieldMeta meta;
        // defaultValueSerialized: representation usable by the editor (JSON-like or plain string)
        std::string defaultValueSerialized;
    };

    // Primary inspector API
    class SCRIPTING_API ScriptInspector {
    public:
        ScriptInspector();
        ~ScriptInspector();

        // Inspect a running instance table (registry ref `instanceRef`) and return exposed fields.
        // The function caches results per scriptPath for performance. If cached entry is stale
        // (timeout in seconds) the inspector will re-inspect the Lua instance.
        std::vector<FieldInfo> InspectInstance(lua_State* L, int instanceRef, const std::string& scriptPath, double cacheTtlSeconds = 1.0);

        // Write back an edited value (string representation) into a live instance table.
        // Returns true on success.
        bool SetFieldFromString(lua_State* L, int instanceRef, const FieldInfo& field, const std::string& valueString) const;

        // Helpers for editor stub generation. Returns a Lua file content containing
        // field declarations/comments for the editor autocomplete.
        std::string GenerateEditorStub(const std::string& scriptPath, const std::vector<FieldInfo>& fields) const;

        // Conversion helpers: string -> lua (pushes value onto stack), and lua -> string (reads from stack idx).
        // These are public so editor UI code can reuse conversions consistently.
        static bool PushStringAsLuaValue(lua_State* L, const std::string& valueString, FieldType targetType);
        static std::string LuaValueToString(lua_State* L, int idx, FieldType expectedType);

    private:
        // Implementation detail: cached inspection result per script path
        struct CacheEntry {
            std::vector<FieldInfo> fields;
            double ttlSeconds = 1.0;
            // last-check time (steady clock)
            std::chrono::steady_clock::time_point lastInspect;
        };

        mutable std::mutex m_cacheMutex;
        std::unordered_map<std::string, CacheEntry> m_cache;

        // internal inspector helpers
        std::vector<FieldInfo> InspectTableOnce(lua_State* L, int absTableIndex) const;
        FieldType DetectFieldType(lua_State* L, int absIndex) const;
        std::string SerializeLuaValueForEditor(lua_State* L, int idx) const;
    };

} // namespace Scripting
