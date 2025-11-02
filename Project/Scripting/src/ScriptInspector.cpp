// ScriptInspector.cpp
#include "ScriptInspector.h"
#include "ScriptSerializer.h" // used to serialize nested tables / values for "defaultValueSerialized"
#include "Logging.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <cassert>
#include <chrono>
#include <sstream>

using namespace Scripting;

static inline void SI_LOG(EngineLogging::LogLevel lvl, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    ENGINE_PRINT(lvl, fmt, ap);
    va_end(ap);
}

// ctor/dtor
ScriptInspector::ScriptInspector() = default;
ScriptInspector::~ScriptInspector() = default;

// public Inspector
std::vector<FieldInfo> ScriptInspector::InspectInstance(lua_State* L, int instanceRef, const std::string& scriptPath, double cacheTtlSeconds)
{
    if (!L || instanceRef == LUA_NOREF) return {};

    // quick caching by scriptPath
    {
        std::lock_guard<std::mutex> lk(m_cacheMutex);
        auto it = m_cache.find(scriptPath);
        if (it != m_cache.end()) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - it->second.lastInspect).count();
            if (elapsed <= it->second.ttlSeconds) {
                return it->second.fields; // return cached
            }
        }
    }

    // Need to inspect current instance
    lua_rawgeti(L, LUA_REGISTRYINDEX, instanceRef);
    int tableIndex = lua_gettop(L);
    int absIndex = lua_absindex(L, tableIndex);

    std::vector<FieldInfo> fields = InspectTableOnce(L, absIndex);

    lua_pop(L, 1); // pop instance

    // store cache
    {
        std::lock_guard<std::mutex> lk(m_cacheMutex);
        CacheEntry entry;
        entry.fields = fields;
        entry.ttlSeconds = cacheTtlSeconds;
        entry.lastInspect = std::chrono::steady_clock::now();
        m_cache[scriptPath] = std::move(entry);
    }

    return fields;
}

// Inspect a table once (no caching). Rules:
// - If table.__editor exists and is a table, it can contain per-field metadata:
//      __editor = { fieldName = { displayName="...", tooltip="...", editorHint="..." }, ... }
// - Else we expose public fields by convention: keys that are strings and do not start with '_' are public.
std::vector<FieldInfo> ScriptInspector::InspectTableOnce(lua_State* L, int absTableIndex) const
{
    std::vector<FieldInfo> out;

    // Read __editor metadata if present
    std::unordered_map<std::string, FieldMeta> metaMap;
    lua_getfield(L, absTableIndex, "__editor");
    if (lua_istable(L, -1)) {
        // iterate __editor members
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            // key at -2 (should be string), value at -1 expected table with fields
            if (lua_type(L, -2) == LUA_TSTRING && lua_istable(L, -1)) {
                size_t klen = 0;
                const char* key = lua_tolstring(L, -2, &klen);
                FieldMeta fm;
                lua_getfield(L, -1, "displayName");
                if (lua_isstring(L, -1)) fm.displayName = lua_tostring(L, -1);
                lua_pop(L, 1);
                lua_getfield(L, -1, "tooltip");
                if (lua_isstring(L, -1)) fm.tooltip = lua_tostring(L, -1);
                lua_pop(L, 1);
                lua_getfield(L, -1, "editorHint");
                if (lua_isstring(L, -1)) fm.editorHint = lua_tostring(L, -1);
                lua_pop(L, 1);
                // other string entries - copy into other map
                lua_pushnil(L);
                while (lua_next(L, -2) != 0) {
                    if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TSTRING) {
                        size_t nmLen = 0; const char* nm = lua_tolstring(L, -2, &nmLen);
                        std::string s(nm);
                        if (s != "displayName" && s != "tooltip" && s != "editorHint") {
                            fm.other[s] = lua_tostring(L, -1);
                        }
                    }
                    lua_pop(L, 1);
                }
                metaMap.emplace(std::string(key, klen), std::move(fm));
            }
            lua_pop(L, 1); // pop value, keep key for next iteration
        }
    }
    lua_pop(L, 1); // pop __editor entry

    // Iterate all keys in the instance table
    lua_pushnil(L);
    while (lua_next(L, absTableIndex) != 0) {
        // key at -2, value at -1
        if (lua_type(L, -2) == LUA_TSTRING) {
            size_t klen = 0;
            const char* key = lua_tolstring(L, -2, &klen);
            std::string name(key, klen);
            // convention: skip fields starting with '_' (private)
            if (!name.empty() && name[0] == '_') {
                lua_pop(L, 1);
                continue;
            }
            FieldInfo fi;
            fi.name = name;
            fi.type = DetectFieldType(L, lua_gettop(L));
            auto metaIt = metaMap.find(fi.name);
            if (metaIt != metaMap.end()) fi.meta = metaIt->second;
            fi.defaultValueSerialized = SerializeLuaValueForEditor(L, lua_gettop(L));
            out.emplace_back(std::move(fi));
        }
        lua_pop(L, 1); // pop value, keep key
    }

    return out;
}

FieldType ScriptInspector::DetectFieldType(lua_State* L, int absIndex) const
{
    int t = lua_type(L, absIndex);
    switch (t) {
    case LUA_TNIL: return FieldType::Nil;
    case LUA_TNUMBER: return FieldType::Number;
    case LUA_TBOOLEAN: return FieldType::Boolean;
    case LUA_TSTRING: return FieldType::String;
    case LUA_TTABLE: return FieldType::Table;
    case LUA_TFUNCTION: return FieldType::Function;
    case LUA_TUSERDATA: {
        // check metatable.__reflect_type if present (engine reflection)
        if (lua_getmetatable(L, absIndex)) {
            lua_getfield(L, -1, "__reflect_type");
            if (lua_isstring(L, -1)) {
                lua_pop(L, 2);
                return FieldType::ReflectedUserdata;
            }
            lua_pop(L, 2);
        }
        return FieldType::Other;
    }
    default:
        return FieldType::Other;
    }
}

std::string ScriptInspector::SerializeLuaValueForEditor(lua_State* L, int idx) const
{
    // Prefer using ScriptSerializer for tables and userdata to get a compact JSON representation.
    int absIdx = lua_absindex(L, idx);
    int t = lua_type(L, absIdx);
    try {
        if (t == LUA_TTABLE || t == LUA_TUSERDATA) {
            // create a temporary registry ref to let ScriptSerializer serialize the value
            lua_pushvalue(L, absIdx);
            int tmpRef = luaL_ref(L, LUA_REGISTRYINDEX);
            ScriptSerializer ss;
            std::string json = ss.SerializeInstanceToJson(L, tmpRef);
            luaL_unref(L, LUA_REGISTRYINDEX, tmpRef);
            return json;
        }
        else if (t == LUA_TSTRING) {
            size_t len = 0; const char* s = lua_tolstring(L, absIdx, &len);
            return std::string(s, len);
        }
        else if (t == LUA_TNUMBER) {
            std::ostringstream oss;
            oss << lua_tonumber(L, absIdx);
            return oss.str();
        }
        else if (t == LUA_TBOOLEAN) {
            return lua_toboolean(L, absIdx) ? "true" : "false";
        }
    }
    catch (...) {
        SI_LOG(EngineLogging::LogLevel::Warn, "ScriptInspector: serialization exception (field) - fallback to empty");
    }
    return {};
}

// Public conversion helper: push string as lua value of targetType
bool ScriptInspector::PushStringAsLuaValue(lua_State* L, const std::string& valueString, FieldType targetType)
{
    if (!L) return false;
    switch (targetType) {
    case FieldType::String:
        lua_pushlstring(L, valueString.c_str(), valueString.size());
        return true;
    case FieldType::Number: {
        char* endptr = nullptr;
        double v = strtod(valueString.c_str(), &endptr);
        if (endptr == valueString.c_str()) return false;
        lua_pushnumber(L, static_cast<lua_Number>(v));
        return true;
    }
    case FieldType::Boolean: {
        std::string low = valueString;
        for (auto& c : low) c = (char)tolower(c);
        lua_pushboolean(L, (low == "true" || low == "1" || low == "yes") ? 1 : 0);
        return true;
    }
    case FieldType::Nil:
        lua_pushnil(L);
        return true;
    default:
        // For complex types we expect a JSON representation: use ScriptSerializer via temporary table approach.
        if (targetType == FieldType::Table || targetType == FieldType::ReflectedUserdata) {
            // Create temp table ref and feed Parse via ScriptSerializer: here we reuse DeserializeJsonToInstance by creating a new temp table
            lua_newtable(L);
            int tmpRef = luaL_ref(L, LUA_REGISTRYINDEX);
            ScriptSerializer ss;
            bool ok = ss.DeserializeJsonToInstance(L, tmpRef, valueString);
            if (!ok) {
                luaL_unref(L, LUA_REGISTRYINDEX, tmpRef);
                return false;
            }
            // push the resulting table onto stack for caller (we must get it from registry)
            lua_rawgeti(L, LUA_REGISTRYINDEX, tmpRef);
            luaL_unref(L, LUA_REGISTRYINDEX, tmpRef);
            return true;
        }
        return false;
    }
}

// Public conversion helper: read lua value to string (editor friendly)
std::string ScriptInspector::LuaValueToString(lua_State* L, int idx, FieldType expectedType)
{
    int t = lua_type(L, idx);
    if (t == LUA_TSTRING) {
        size_t len = 0; const char* s = lua_tolstring(L, idx, &len);
        return std::string(s, len);
    }
    if (t == LUA_TNUMBER) {
        std::ostringstream oss; oss << lua_tonumber(L, idx); return oss.str();
    }
    if (t == LUA_TBOOLEAN) return lua_toboolean(L, idx) ? "true" : "false";
    if (t == LUA_TTABLE || t == LUA_TUSERDATA) {
        // re-use ScriptSerializer
        lua_pushvalue(L, idx);
        int tmpRef = luaL_ref(L, LUA_REGISTRYINDEX);
        ScriptSerializer ss;
        std::string json = ss.SerializeInstanceToJson(L, tmpRef);
        luaL_unref(L, LUA_REGISTRYINDEX, tmpRef);
        return json;
    }
    return {};
}

// Set a field on a live instance using valueString representation (editor edited)
bool ScriptInspector::SetFieldFromString(lua_State* L, int instanceRef, const FieldInfo& field, const std::string& valueString) const
{
    if (!L || instanceRef == LUA_NOREF) return false;
    // get table absolute index
    lua_rawgeti(L, LUA_REGISTRYINDEX, instanceRef);
    int tableIdx = lua_gettop(L);
    int absTable = lua_absindex(L, tableIdx);

    // push the value according to field type
    if (!PushStringAsLuaValue(L, valueString, field.type)) {
        lua_pop(L, 1);
        return false;
    }
    // set table[field.name] = value (pops value)
    lua_setfield(L, absTable, field.name.c_str());

    lua_pop(L, 1); // pop instance table
    return true;
}

// Simple editor stub generation (human-readable)
std::string ScriptInspector::GenerateEditorStub(const std::string& scriptPath, const std::vector<FieldInfo>& fields) const
{
    std::ostringstream oss;
    oss << "-- Editor stub for " << scriptPath << "\n";
    oss << "-- Generated fields (for autocomplete and hints)\n\n";
    oss << "local M = {}\n\n";
    for (const auto& f : fields) {
        oss << "-- " << (f.meta.displayName.empty() ? f.name : f.meta.displayName) << "\n";
        if (!f.meta.tooltip.empty()) oss << "-- " << f.meta.tooltip << "\n";
        oss << "M." << f.name << " = ";
        switch (f.type) {
        case FieldType::Number: oss << "0"; break;
        case FieldType::Boolean: oss << "false"; break;
        case FieldType::String: oss << "\"\""; break;
        case FieldType::Table: oss << "{}"; break;
        default: oss << "nil"; break;
        }
        oss << " -- " << (f.defaultValueSerialized.empty() ? "" : f.defaultValueSerialized) << "\n\n";
    }
    oss << "return M\n";
    return oss.str();
}
