#pragma once
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <string>
#include <ECS/ComponentRegistry.hpp>

void RegisterComponentProxyMeta(lua_State* L);

int PushComponentProxy(lua_State* L, class ECSManager* ecs, Entity ent, const std::string& compName);
