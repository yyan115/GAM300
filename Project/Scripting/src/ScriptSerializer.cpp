// ScriptSerializer.cpp
#include "ScriptSerializer.h"
#include "Logging.hpp"
#include "Reflection/ReflectionBase.hpp"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <cassert>
#include <vector>
#include <cstring>
#include <mutex>

using namespace Scripting;

#define SS_LOG(level, ...) ENGINE_PRINT(level, __VA_ARGS__)

// Helpers to bridge header placeholder signatures to real RapidJSON types
static inline rapidjson::Value* RP_Value(void* p) { return reinterpret_cast<rapidjson::Value*>(p); }
static inline rapidjson::Document::AllocatorType* RP_Alloc(void* p) { return reinterpret_cast<rapidjson::Document::AllocatorType*>(p); }

// -----------------------------------------------------------------------------
// Construction / destruction
// -----------------------------------------------------------------------------
ScriptSerializer::ScriptSerializer() = default;
ScriptSerializer::~ScriptSerializer() = default;

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------
std::string ScriptSerializer::SerializeInstanceToJson(lua_State* L, int instanceRef) const
{
    if (!L) return "{}";

    rapidjson::Document doc; // local to this TU (allowed)
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    // push instance table
    lua_rawgeti(L, LUA_REGISTRYINDEX, instanceRef);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        SS_LOG(EngineLogging::LogLevel::Warn, "ScriptSerializer::SerializeInstanceToJson - instanceRef is not a table");
        return "{}";
    }

    std::unordered_set<const void*> visited;
    rapidjson::Value root;
    // call the internal converter with real RapidJSON objects
    if (!LuaValueToJson(L, lua_gettop(L), /*valuePlaceholder*/ &root, /*allocatorPlaceholder*/ &alloc, visited)) {
        lua_pop(L, 1);
        SS_LOG(EngineLogging::LogLevel::Warn, "ScriptSerializer::SerializeInstanceToJson - conversion failed");
        return "{}";
    }
    lua_pop(L, 1); // pop instance table

    doc.Swap(root);

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    doc.Accept(writer);
    return std::string(sb.GetString(), sb.GetSize());
}

bool ScriptSerializer::DeserializeJsonToInstance(lua_State* L, int instanceRef, const std::string& json) const
{
    if (!L) return false;

    rapidjson::Document doc;
    if (doc.Parse(json.c_str()).HasParseError()) {
        SS_LOG(EngineLogging::LogLevel::Warn, "ScriptSerializer::DeserializeJsonToInstance - JSON parse error");
        return false;
    }

    if (!doc.IsObject() && !doc.IsArray()) {
        SS_LOG(EngineLogging::LogLevel::Warn, "ScriptSerializer::DeserializeJsonToInstance - JSON root not object/array");
        return false;
    }

    // push instance table
    lua_rawgeti(L, LUA_REGISTRYINDEX, instanceRef);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        SS_LOG(EngineLogging::LogLevel::Warn, "ScriptSerializer::DeserializeJsonToInstance - instanceRef not a table");
        return false;
    }
    int tableIndex = lua_gettop(L);
    int absTable = lua_absindex(L, tableIndex);

    // shallow clear: remove existing keys
    lua_pushnil(L);
    while (lua_next(L, absTable) != 0) {
        lua_pop(L, 1);
        lua_pushvalue(L, -1);
        lua_pushnil(L);
        lua_settable(L, absTable);
    }
    // pop the last key left by lua_next
    if (lua_gettop(L) >= 1) lua_pop(L, 1);

    // populate directly from rapidjson doc members
    if (doc.IsObject()) {
        for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
            const std::string key = it->name.GetString();
            // Convert member value into Lua (passes pointer to rapidjson::Value)
            if (!JsonToLuaValue(L, /*valuePlaceholder*/ const_cast<rapidjson::Value*>(&(it->value)))) {
                lua_pushnil(L);
            }
            lua_setfield(L, absTable, key.c_str()); // pops pushed value
        }
    }
    else if (doc.IsArray()) {
        for (rapidjson::SizeType i = 0; i < doc.Size(); ++i) {
            if (!JsonToLuaValue(L, /*valuePlaceholder*/ const_cast<rapidjson::Value*>(&(doc[i])))) {
                lua_pushnil(L);
            }
            lua_rawseti(L, absTable, static_cast<lua_Integer>(i + 1)); // set array element (1-based)
        }
    }

    lua_pop(L, 1); // pop instance table
    return true;
}

// -----------------------------------------------------------------------------
// Lua -> JSON conversion (implementation uses real RapidJSON types)
// -----------------------------------------------------------------------------
bool ScriptSerializer::LuaValueToJson(lua_State* L, int idx,
    void* valuePlaceholder,
    void* allocatorPlaceholder,
    std::unordered_set<const void*>& visited) const
{
    if (!L) return false;
    rapidjson::Value* out = RP_Value(valuePlaceholder);
    auto* alloc = RP_Alloc(allocatorPlaceholder);
    int absIdx = lua_absindex(L, idx);
    int type = lua_type(L, absIdx);

    switch (type) {
    case LUA_TNIL:
        out->SetNull();
        return true;
    case LUA_TBOOLEAN:
        out->SetBool(lua_toboolean(L, absIdx) != 0);
        return true;
    case LUA_TNUMBER:
        out->SetDouble(lua_tonumber(L, absIdx));
        return true;
    case LUA_TSTRING: {
        size_t len = 0;
        const char* s = lua_tolstring(L, absIdx, &len);
        out->SetString(s, static_cast<rapidjson::SizeType>(len), *alloc);
        return true;
    }
    case LUA_TTABLE: {
        // Exception-safe visited handling:
        const void* addr = lua_topointer(L, absIdx);
        if (addr && visited.find(addr) != visited.end()) {
            out->SetNull();
            return true;
        }

        struct AutoUnvisit {
            std::unordered_set<const void*>& set;
            const void* addr;
            AutoUnvisit(std::unordered_set<const void*>& s, const void* a) : set(s), addr(a) {}
            ~AutoUnvisit() { if (addr) set.erase(addr); }
        };

        if (addr) visited.insert(addr);
        AutoUnvisit guard(visited, addr);

        bool isArray = true;
        lua_Integer maxIndex = 0;

        lua_pushnil(L);
        while (lua_next(L, absIdx) != 0) {
            int ktype = lua_type(L, -2);
            if (ktype == LUA_TNUMBER && lua_isinteger(L, -2)) {
                lua_Integer k = lua_tointeger(L, -2);
                if (k > maxIndex) maxIndex = k;
            }
            else {
                isArray = false;
            }
            lua_pop(L, 1);
        }

        if (isArray && maxIndex > 0) {
            out->SetArray();
            for (lua_Integer i = 1; i <= maxIndex; ++i) {
                lua_rawgeti(L, absIdx, i);
                rapidjson::Value elem;
                bool ok = LuaValueToJson(L, lua_gettop(L), &elem, alloc, visited);
                lua_pop(L, 1);
                if (!ok) elem.SetNull();
                out->PushBack(elem, *alloc);
            }
        }
        else {
            out->SetObject();
            lua_pushnil(L);
            while (lua_next(L, absIdx) != 0) {
                if (lua_type(L, -2) == LUA_TSTRING) {
                    size_t klen = 0;
                    const char* k = lua_tolstring(L, -2, &klen);
                    rapidjson::Value key(k, static_cast<rapidjson::SizeType>(klen), *alloc);
                    rapidjson::Value val;
                    bool ok = LuaValueToJson(L, lua_gettop(L), &val, alloc, visited);
                    if (!ok) val.SetNull();
                    out->AddMember(key, val, *alloc);
                }
                lua_pop(L, 1);
            }
        }

        // AutoUnvisit destructor will remove addr from visited (even if recursion throws)
        return true;
    }
    case LUA_TUSERDATA: {
        rapidjson::Value tmp;
        if (SerializeReflectedUserdata(L, absIdx, &tmp, alloc)) {
            out->CopyFrom(tmp, *alloc);
            return true;
        }
        SS_LOG(EngineLogging::LogLevel::Warn, "ScriptSerializer: userdata at stack index %d not serializable", absIdx);
        out->SetNull();
        return true;
    }
    default:
        out->SetNull();
        return true;
    }
}

// -----------------------------------------------------------------------------
// JSON -> Lua conversion (implementation uses real RapidJSON types)
// -----------------------------------------------------------------------------
bool ScriptSerializer::JsonToLuaValue(lua_State* L, const void* valuePlaceholder) const
{
    if (!L) return false;
    const rapidjson::Value* v = reinterpret_cast<const rapidjson::Value*>(valuePlaceholder);

    if (v->IsNull()) { lua_pushnil(L); return true; }
    if (v->IsBool()) { lua_pushboolean(L, v->GetBool()); return true; }
    if (v->IsNumber()) { lua_pushnumber(L, v->GetDouble()); return true; }
    if (v->IsString()) { lua_pushlstring(L, v->GetString(), v->GetStringLength()); return true; }
    if (v->IsArray()) {
        lua_newtable(L);
        for (rapidjson::SizeType i = 0; i < v->Size(); ++i) {
            if (!JsonToLuaValue(L, &((*v)[i]))) lua_pushnil(L);
            lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
        }
        return true;
    }
    if (v->IsObject()) {
        if (v->HasMember("type") && v->HasMember("data") && (*v)["type"].IsString()) {
            // pass pointer (not dereferenced value) to match DeserializeReflectedUserdata signature
            if (DeserializeReflectedUserdata(L, v)) return true;
        }

        lua_newtable(L);
        for (auto it = v->MemberBegin(); it != v->MemberEnd(); ++it) {
            const char* key = it->name.GetString();
            bool pushed = JsonToLuaValue(L, &it->value);
            if (!pushed) {
                // JsonToLuaValue failed and did not push anything — push nil to keep contract.
                lua_pushnil(L);
            }
            lua_setfield(L, -2, key); // pops one value
        }
        return true;
    }

    lua_pushnil(L);
    return true;
}

// -----------------------------------------------------------------------------
// Reflected userdata helpers (use rapidjson::Value in this TU only)
// -----------------------------------------------------------------------------
bool ScriptSerializer::SerializeReflectedUserdata(lua_State* L, int idx,
    void* valuePlaceholder,
    void* allocatorPlaceholder) const
{
    rapidjson::Value* out = RP_Value(valuePlaceholder);
    auto* alloc = RP_Alloc(allocatorPlaceholder);

    if (!lua_getmetatable(L, idx)) return false;
    lua_pushstring(L, "__reflect_type");
    lua_rawget(L, -2); // push metatable.__reflect_type
    if (!lua_isstring(L, -1)) { lua_pop(L, 2); return false; }

    size_t tlen = 0;
    const char* typeName = lua_tolstring(L, -1, &tlen);

    void* userdata_ptr = nullptr;
    if (lua_isuserdata(L, idx)) {
        void* block = lua_touserdata(L, idx);
        if (block) std::memcpy(&userdata_ptr, block, sizeof(void*));
    }
    lua_pop(L, 2); // pop __reflect_type and metatable

    if (!userdata_ptr) {
        SS_LOG(EngineLogging::LogLevel::Warn, "ScriptSerializer: reflected userdata without pointer for type ", typeName);
        return false;
    }

    TypeDescriptor* desc = nullptr;
    {
        std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
        auto& map = TypeDescriptor::type_descriptor_lookup();
        auto it = map.find(std::string(typeName));
        if (it != map.end()) desc = it->second;
    }
    if (!desc) {
        SS_LOG(EngineLogging::LogLevel::Warn, "ScriptSerializer: no TypeDescriptor registered for ", typeName);
        return false;
    }

    rapidjson::Document subdoc; subdoc.SetObject();
    try {
        desc->SerializeJson(userdata_ptr, subdoc);
    }
    catch (const std::exception& e) {
        SS_LOG(EngineLogging::LogLevel::Warn, "ScriptSerializer: TypeDescriptor::SerializeJson failed for ", typeName, " :",e.what());
        return false;
    }

    out->CopyFrom(subdoc, *alloc);
    return true;
}

bool ScriptSerializer::DeserializeReflectedUserdata(lua_State* L, const void* valuePlaceholder) const
{
    const rapidjson::Value* v = reinterpret_cast<const rapidjson::Value*>(valuePlaceholder);
    if (!v->HasMember("type") || !(*v)["type"].IsString()) return false;
    const char* typeName = (*v)["type"].GetString();

    TypeDescriptor* desc = nullptr;
    {
        std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
        auto& map = TypeDescriptor::type_descriptor_lookup();
        auto it = map.find(std::string(typeName));
        if (it != map.end()) desc = it->second;
    }
    if (!desc) {
        SS_LOG(EngineLogging::LogLevel::Warn, "ScriptSerializer: no TypeDescriptor for ", typeName);
        return false;
    }

    size_t sz = desc->GetSize();
    if (sz == 0) {
        SS_LOG(EngineLogging::LogLevel::Warn, "ScriptSerializer: TypeDescriptor ", typeName," reports size 0");
        return false;
    }

    // allocate the concrete object storage
    uint8_t* buffer = new uint8_t[sz];
    std::memset(buffer, 0, sz);

    try {
        desc->Deserialize(buffer, *v);
    }
    catch (const std::exception& e) {
        SS_LOG(EngineLogging::LogLevel::Warn, "ScriptSerializer: Deserialize failed for ", typeName," : ", e.what());
        delete[] buffer;
        return false;
    }

    // Safe userdata header
    struct LuaRefHeader {
        void* ptr;            // pointer to heap buffer (allocated above)
        uint32_t magic;       // magic tag for debug/validation
        uint32_t version;     // future-proofing
    };
    static const uint32_t USER_MAGIC = 0xA5F0C0DEu;
    static const uint32_t USER_VERSION = 1u;

    void* ud = lua_newuserdata(L, sizeof(LuaRefHeader));
    if (!ud) { delete[] buffer; return false; }

    LuaRefHeader header;
    header.ptr = buffer;
    header.magic = USER_MAGIC;
    header.version = USER_VERSION;
    std::memcpy(ud, &header, sizeof(header));

    // create/get metatable for this typeName
    int created = luaL_newmetatable(L, typeName); // pushes metatable
    // if created==1, this metatable didn't exist previously — we should initialize fields.
    if (created) {
        // __reflect_type
        lua_pushstring(L, "__reflect_type");
        lua_pushstring(L, typeName);
        lua_settable(L, -3);

        // __gc
        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, [](lua_State* L) -> int {
            void* ud = lua_touserdata(L, 1);
            if (!ud) return 0;
            LuaRefHeader hdr;
            std::memcpy(&hdr, ud, sizeof(hdr));
            if (hdr.magic == USER_MAGIC && hdr.ptr) {
                delete[] reinterpret_cast<uint8_t*>(hdr.ptr);
            }
            else {
                // if magic mismatches, try best effort: attempt to interpret first pointer-sized bytes
                void* maybe_ptr = nullptr;
                std::memcpy(&maybe_ptr, ud, sizeof(void*));
                if (maybe_ptr) delete[] reinterpret_cast<uint8_t*>(maybe_ptr);
            }
            return 0;
            });
        lua_settable(L, -3);
    }
    else {
        // Metatable already existed. Ensure __reflect_type present or set if missing:
        lua_pushstring(L, "__reflect_type");
        lua_rawget(L, -2); // get metatable.__reflect_type
        if (!lua_isstring(L, -1)) {
            lua_pop(L, 1);
            lua_pushstring(L, "__reflect_type");
            lua_pushstring(L, typeName);
            lua_settable(L, -3);
        }
        else {
            lua_pop(L, 1);
        }
        // Do NOT overwrite __gc if it already exists.
    }

    // set metatable on userdata (metatable is on stack top)
    lua_setmetatable(L, -2); // sets metatable on userdata (pops metatable)

    // Leave userdata on the stack for caller to consume/push into tables.
    return true;
}
