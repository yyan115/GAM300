// ComponentProxy.cpp
#include "pch.h"
#include "Script/ComponentProxy.h"
#include "ECS/ComponentRegistry.hpp"
#include "Reflection/ReflectionBase.hpp"
#include "ECS/ECSManager.hpp"
#include "Logging.hpp"               // <--- added for ENGINE_PRINT
#include <string>
#include <cassert>
#include <cstring>

// Lua C API
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

// userdata layout
struct ComponentProxy {
    ECSManager* ecs = nullptr;
    Entity ent = 0;
    // no std::string members here so destructor is trivial for now
};

// metatable name
static const char* COMP_MT = "Engine.ComponentProxy";

// forward declarations
static int comp_index(lua_State* L);
static int comp_newindex(lua_State* L);
static int comp_tostring(lua_State* L);
static int comp_gc(lua_State* L);

// helpers that convert reflected values <-> Lua
static void PushValueFromDescriptor(lua_State* L, TypeDescriptor* td, void* ptr);
static bool ReadLuaToDescriptor(lua_State* L, int idx, TypeDescriptor* td, void* ptr);

// helper to set/get the comp name field stored in the userdata's uservalue table
static void SetProxyCompName(lua_State* L, int userdataIndex, const std::string& name) {
    int abs = lua_absindex(L, userdataIndex);
    // get uservalue (may be nil)
    lua_getuservalue(L, abs); // pushes uservalue
    if (!lua_istable(L, -1)) {
        // replace non-table uservalue with a fresh table
        lua_pop(L, 1);
        lua_newtable(L);
    }
    // stack: ... , uservalue_table
    lua_pushlstring(L, name.c_str(), name.size());
    lua_setfield(L, -2, "__compname"); // uservalue_table["__compname"] = name
    // set back as uservalue (lua_setuservalue pops value)
    lua_setuservalue(L, abs);
}

static std::string GetProxyCompName(lua_State* L, int userdataIndex) {
    int abs = lua_absindex(L, userdataIndex);
    lua_getuservalue(L, abs); // pushes uservalue (or nil)
    std::string out;
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "__compname"); // pushes value or nil
        if (lua_isstring(L, -1)) {
            size_t len = 0;
            const char* s = lua_tolstring(L, -1, &len);
            if (s && len > 0) out.assign(s, len);
        }
        lua_pop(L, 1); // pop __compname
    }
    lua_pop(L, 1); // pop uservalue (table or nil)
    return out;
}

// helper to cache/get the TypeDescriptor* in the uservalue table
static void CacheTypeDescriptorInUservalue(lua_State* L, int userdataIndex, TypeDescriptor* td) {
    if (!td) return;
    int abs = lua_absindex(L, userdataIndex);
    lua_getuservalue(L, abs); // pushes uservalue (must be table)
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }
    lua_pushlightuserdata(L, td);
    lua_setfield(L, -2, "__typedesc");
    lua_setuservalue(L, abs); // pops uservalue
}

static TypeDescriptor* GetCachedTypeDescriptor(lua_State* L, int userdataIndex) {
    int abs = lua_absindex(L, userdataIndex);
    lua_getuservalue(L, abs); // pushes uservalue
    TypeDescriptor* td = nullptr;
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "__typedesc");
        if (lua_islightuserdata(L, -1)) {
            td = reinterpret_cast<TypeDescriptor*>(lua_touserdata(L, -1));
        }
        lua_pop(L, 1); // pop __typedesc
    }
    lua_pop(L, 1); // pop uservalue
    return td;
}

// ----- Push helpers (reflection->lua) -----
static void PushPrimitiveByName(lua_State* L, const std::string& name, void* ptr) {
    if (name == "int") {
        lua_pushinteger(L, static_cast<lua_Integer>(*(int*)ptr));
    }
    else if (name == "unsigned" || name == "unsigned int") {
        lua_pushinteger(L, static_cast<lua_Integer>(*(unsigned*)ptr));
    }
    else if (name == "double" || name == "Double") {
        lua_pushnumber(L, static_cast<lua_Number>(*(double*)ptr));
    }
    else if (name == "float") {
        lua_pushnumber(L, static_cast<lua_Number>(*(float*)ptr));
    }
    else if (name == "bool") {
        lua_pushboolean(L, (*(bool*)ptr) ? 1 : 0);
    }
    else if (name == "std::string" || name == "string") {
        const std::string& s = *(std::string*)ptr;
        lua_pushlstring(L, s.c_str(), static_cast<size_t>(s.size()));
    }
    else {
        lua_pushnil(L);
    }
}

static void PushVector3D(lua_State* L, void* ptr) {
    if (!ptr) { lua_pushnil(L); return; }
    Vector3D* v = reinterpret_cast<Vector3D*>(ptr);
    lua_newtable(L);
    lua_pushnumber(L, v->x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, v->y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, v->z); lua_setfield(L, -2, "z");
}

static void PushQuaternion(lua_State* L, void* ptr) {
    if (!ptr) { lua_pushnil(L); return; }
    Quaternion* q = reinterpret_cast<Quaternion*>(ptr);
    lua_newtable(L);
    lua_pushnumber(L, q->w); lua_setfield(L, -2, "w");
    lua_pushnumber(L, q->x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, q->y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, q->z); lua_setfield(L, -2, "z");
}

static void PushValueFromDescriptor(lua_State* L, TypeDescriptor* td, void* ptr) {
    if (!td || !ptr) { lua_pushnil(L); return; }
    const char* nameC = td->GetName();
    std::string name = nameC ? nameC : "";

    // fast paths
    if (name == "Vector3D") { PushVector3D(L, ptr); return; }
    if (name == "Quaternion") { PushQuaternion(L, ptr); return; }

    // primitives
    if (name == "int" || name == "unsigned" || name == "unsigned int" ||
        name == "float" || name == "double" || name == "bool" || name == "std::string" || name == "string") {
        PushPrimitiveByName(L, name, ptr);
        return;
    }

    // struct -> table
    TypeDescriptor_Struct* ts = dynamic_cast<TypeDescriptor_Struct*>(td);
    if (ts) {
        lua_newtable(L);
        auto members = ts->GetMembers();
        for (const auto& m : members) {
            void* memberPtr = m.get_ptr(ptr);
            PushValueFromDescriptor(L, m.type, memberPtr);
            lua_setfield(L, -2, m.name);
        }
        return;
    }

    // fallback
    lua_pushnil(L);
}

// ----- Lua -> C++ basic reads via reflection -----
static bool ReadLuaToDescriptor(lua_State* L, int idx, TypeDescriptor* td, void* ptr) {
    if (!td || !ptr) return false;
    const char* nameC = td->GetName();
    std::string name = nameC ? nameC : "";

    if (name == "int") {
        if (!lua_isnumber(L, idx)) return false;
        *(int*)ptr = static_cast<int>(lua_tointeger(L, idx));
        return true;
    }
    if (name == "unsigned" || name == "unsigned int") {
        if (!lua_isnumber(L, idx)) return false;
        *(unsigned*)ptr = static_cast<unsigned>(lua_tointeger(L, idx));
        return true;
    }
    if (name == "float") {
        if (!lua_isnumber(L, idx)) return false;
        *(float*)ptr = static_cast<float>(lua_tonumber(L, idx));
        return true;
    }
    if (name == "double") {
        if (!lua_isnumber(L, idx)) return false;
        *(double*)ptr = static_cast<double>(lua_tonumber(L, idx));
        return true;
    }
    if (name == "bool") {
        if (!lua_isboolean(L, idx) && !lua_isnumber(L, idx)) return false;
        *(bool*)ptr = (lua_toboolean(L, idx) != 0);
        return true;
    }
    if (name == "std::string" || name == "string") {
        size_t len = 0;
        const char* s = lua_tolstring(L, idx, &len);
        if (!s) { *(std::string*)ptr = ""; return true; }
        *(std::string*)ptr = std::string(s, len);
        return true;
    }
    if (name == "Vector3D") {
        if (!lua_istable(L, idx)) return false;
        Vector3D v{};
        lua_getfield(L, idx, "x"); if (lua_isnumber(L, -1)) v.x = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, idx, "y"); if (lua_isnumber(L, -1)) v.y = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, idx, "z"); if (lua_isnumber(L, -1)) v.z = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        *(Vector3D*)ptr = v;
        return true;
    }
    if (name == "Quaternion") {
        if (!lua_istable(L, idx)) return false;
        Quaternion q{};
        lua_getfield(L, idx, "w"); if (lua_isnumber(L, -1)) q.w = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, idx, "x"); if (lua_isnumber(L, -1)) q.x = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, idx, "y"); if (lua_isnumber(L, -1)) q.y = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, idx, "z"); if (lua_isnumber(L, -1)) q.z = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        *(Quaternion*)ptr = q;
        return true;
    }

    // unsupported complex types for write for now
    return false;
}

// ---------- metamethods ----------
static int comp_index(lua_State* L) {
    // args: userdata, key
    if (!lua_isuserdata(L, 1)) { lua_pushnil(L); return 1; }
    ComponentProxy* p = (ComponentProxy*)lua_touserdata(L, 1);
    if (!p || !p->ecs) { lua_pushnil(L); return 1; }

    const char* key = nullptr;
    if (lua_type(L, 2) == LUA_TSTRING) key = lua_tostring(L, 2);
    if (!key) { lua_pushnil(L); return 1; }

    // If asking special keys return quickly
    if (strcmp(key, "__ent") == 0) { lua_pushinteger(L, static_cast<lua_Integer>(p->ent)); return 1; }
    if (strcmp(key, "__component") == 0) { std::string nm = GetProxyCompName(L, 1); lua_pushstring(L, nm.c_str()); return 1; }

    // lookup component info via registry (but first check cached typedesc)
    std::string compName = GetProxyCompName(L, 1);
    if (compName.empty()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ComponentProxy] Proxy has no component name stored for userdata");
        lua_pushnil(L);
        return 1;
    }

    ComponentRegistry::ComponentInfo ci;
    // try cached typedesc first (avoids registry lookup cost)
    TypeDescriptor* cachedTd = GetCachedTypeDescriptor(L, 1);

    if (!ComponentRegistry::Instance().Get(compName, ci)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ComponentProxy] ComponentRegistry.Get failed for comp='", compName, "' (entity=", p->ent, ")");
        lua_pushnil(L);
        return 1;
    }

    void* compPtr = nullptr;
    if (ci.getter) compPtr = ci.getter(p->ecs, p->ent);
    if (!compPtr) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ComponentProxy] Getter returned null for comp='", compName, "' entity=", p->ent);
        lua_pushnil(L);
        return 1;
    }

    // Use TypeDescriptor from cache if present, otherwise from registry entry
    TypeDescriptor* td = cachedTd ? cachedTd : ci.typeDesc;
    if (!td) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ComponentProxy] Missing TypeDescriptor for comp='", compName, "'; cannot reflect members");
        lua_pushnil(L);
        return 1;
    }

    // reflect into struct to find member
    TypeDescriptor_Struct* ts = dynamic_cast<TypeDescriptor_Struct*>(td);
    if (!ts) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ComponentProxy] TypeDescriptor for comp='", compName, "' is not a struct");
        lua_pushnil(L);
        return 1;
    }

    auto members = ts->GetMembers();
    for (const auto& m : members) {
        if (std::strcmp(m.name, key) == 0) {
            void* memberPtr = m.get_ptr(compPtr);
            PushValueFromDescriptor(L, m.type, memberPtr);
            return 1;
        }
    }

    // member not found
    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[ComponentProxy] Member '", key, "' not found on component '", compName, "'");
    lua_pushnil(L);
    return 1;
}

static int comp_newindex(lua_State* L) {
    // args: userdata, key, value
    if (!lua_isuserdata(L, 1)) return 0;
    ComponentProxy* p = (ComponentProxy*)lua_touserdata(L, 1);
    if (!p || !p->ecs) return 0;

    const char* key = nullptr;
    if (lua_type(L, 2) == LUA_TSTRING) key = lua_tostring(L, 2);
    if (!key) return 0;

    std::string compName = GetProxyCompName(L, 1);
    if (compName.empty()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ComponentProxy] Proxy has no component name stored for userdata when writing");
        return 0;
    }

    ComponentRegistry::ComponentInfo ci;
    if (!ComponentRegistry::Instance().Get(compName, ci)) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ComponentProxy] ComponentRegistry.Get failed for comp='", compName, "' on write (entity=", p->ent, ")");
        return 0;
    }

    void* compPtr = nullptr;
    if (ci.getter) compPtr = ci.getter(p->ecs, p->ent);
    if (!compPtr) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ComponentProxy] Getter returned null for comp='", compName, "' on write entity=", p->ent);
        return 0;
    }

    // Use cached typedesc if present
    TypeDescriptor* td = GetCachedTypeDescriptor(L, 1);
    if (!td) td = ci.typeDesc;
    if (!td) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ComponentProxy] Missing TypeDescriptor for comp='", compName, "'; cannot write members");
        return 0;
    }

    // Reflective path
    TypeDescriptor_Struct* ts = dynamic_cast<TypeDescriptor_Struct*>(td);
    if (!ts) {
        ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[ComponentProxy] TypeDescriptor for comp='", compName, "' is not a struct (write)");
        return 0;
    }

    auto members = ts->GetMembers();
    for (const auto& m : members) {
        if (std::strcmp(m.name, key) == 0) {
            void* memberPtr = m.get_ptr(compPtr);
            // attempt to write Lua value into member memory
            // value is at index 3
            if (!ReadLuaToDescriptor(L, 3, m.type, memberPtr)) {
                ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[ComponentProxy] Unsupported write for member '", key, "' on component '", compName, "'");
            }
            return 0;
        }
    }

    ENGINE_PRINT(EngineLogging::LogLevel::Debug, "[ComponentProxy] Attempted write to unknown member '", key, "' on component '", compName, "'");
    return 0;
}

static int comp_tostring(lua_State* L) {
    if (!lua_isuserdata(L, 1)) { lua_pushliteral(L, "ComponentProxy(nil)"); return 1; }
    ComponentProxy* p = (ComponentProxy*)lua_touserdata(L, 1);
    std::string compName = GetProxyCompName(L, 1);
    std::string s = "ComponentProxy(" + compName + ":" + std::to_string(p->ent) + ")";
    lua_pushlstring(L, s.c_str(), static_cast<size_t>(s.size()));
    return 1;
}

static int comp_gc(lua_State* L) {
    // call the destructor for placement-new object
    if (!lua_isuserdata(L, 1)) return 0;
    ComponentProxy* p = (ComponentProxy*)lua_touserdata(L, 1);
    if (p) {
        p->~ComponentProxy();
    }
    return 0;
}

// ---------- public push function ----------
int PushComponentProxy(lua_State* L, ECSManager* ecs, Entity ent, const std::string& compName) {
    assert(L);
    // allocate userdata with 1 uservalue slot so we can store component metadata
    void* ud = lua_newuserdatauv(L, sizeof(ComponentProxy), 1);
    ComponentProxy* p = new (ud) ComponentProxy();
    p->ecs = ecs;
    p->ent = ent;

    // create or reuse metatable
    if (luaL_newmetatable(L, COMP_MT)) {
        // metatable created for first time -> set metamethods
        lua_pushcfunction(L, comp_index); lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, comp_newindex); lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, comp_tostring); lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, comp_gc); lua_setfield(L, -2, "__gc");
        // protect metatable
        lua_pushliteral(L, "locked"); lua_setfield(L, -2, "__metatable");
    }
    // set the metatable on userdata (metatable already on stack at -1)
    lua_setmetatable(L, -2);

    // store component name in userdata uservalue table
    SetProxyCompName(L, -1, compName);

    // cache TypeDescriptor* (if available) in uservalue for fast access
    ComponentRegistry::ComponentInfo ci;
    if (ComponentRegistry::Instance().Get(compName, ci) && ci.typeDesc) {
        CacheTypeDescriptorInUservalue(L, -1, ci.typeDesc);
    }

    // leave userdata on stack
    return 1;
}

// ---------- registration helper ----------
void RegisterComponentProxyMeta(lua_State* L) {
    // Ensure metatable exists (we rely on PushComponentProxy to create it lazily).
    if (!L) return;
    if (luaL_newmetatable(L, COMP_MT)) {
        lua_pushcfunction(L, comp_index); lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, comp_newindex); lua_setfield(L, -2, "__newindex");
        lua_pushcfunction(L, comp_tostring); lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, comp_gc); lua_setfield(L, -2, "__gc");
        lua_pushliteral(L, "locked"); lua_setfield(L, -2, "__metatable");
    }
    lua_pop(L, 1);
}
