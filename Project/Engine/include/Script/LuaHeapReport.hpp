#pragma once

// Desktop-only Lua memory diagnostic, active when GAM300_FRAME_STATS is on and
// the GAM300_LUA_HEAP_REPORT environment variable holds a period in seconds.
// Every period it forces a full collection, walks every table reachable from
// the globals and the registry, and prints the reachable-entry count of the
// largest object groups together with their growth since the previous report.
// That is enough to name the table a leaking script keeps appending to.

struct lua_State;

namespace LuaHeapReport {

void Tick(lua_State* L);

}  // namespace LuaHeapReport
