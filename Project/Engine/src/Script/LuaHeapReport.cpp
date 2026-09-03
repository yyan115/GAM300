#include "pch.h"
#include "Script/LuaHeapReport.hpp"

#if defined(GAM300_FRAME_STATS) && !defined(ANDROID)

#include <chrono>
#include <cstdio>
#include <cstdlib>

#include <lua.hpp>

namespace {

// Breadth-first walk over every reachable table. Each table is attributed to
// the first path that reaches it and the counts are aggregated by the first
// three path segments, so per-entity tables collapse into their owner.
constexpr const char* kReportChunk = R"lua(
local registry = debug.getregistry()
local prev = registry.__gam300_heap_prev or {}
local seen = {}
local groups = {}      -- aggregated path -> reachable entry count
local objectCount = 0
-- Reachable values by type; strings also accumulate their byte length so a
-- growing string cache shows up even when the owning table does not grow.
local typeCounts = { string = 0, stringBytes = 0, ["function"] = 0, userdata = 0, thread = 0, table = 0, number = 0 }
local prevTypes = registry.__gam300_heap_prev_types or {}

local function groupKey(path)
  local parts = {}
  for part in string.gmatch(path, "[^%.]+") do
    parts[#parts + 1] = part
    if #parts == 3 then break end
  end
  return table.concat(parts, ".")
end

local function keyName(k)
  local kt = type(k)
  if kt == "string" then return k end
  if kt == "number" then return "[n]" end
  return "[" .. kt .. "]"
end

local queue = { { _G, "_G" }, { registry, "registry" } }
local head = 1
seen[registry] = true
seen[_G] = true
while queue[head] do
  local t, path = queue[head][1], queue[head][2]
  head = head + 1
  objectCount = objectCount + 1
  local n = 0
  for k, v in next, t do
    n = n + 1
    local vt = type(v)
    if typeCounts[vt] then typeCounts[vt] = typeCounts[vt] + 1 end
    if vt == "string" then typeCounts.stringBytes = typeCounts.stringBytes + #v end
    if vt == "table" then
      if not seen[v] then
        seen[v] = true
        queue[#queue + 1] = { v, path .. "." .. keyName(k) }
      end
    elseif vt == "function" then
      local i = 1
      while true do
        local upName, upValue = debug.getupvalue(v, i)
        if not upName then break end
        if type(upValue) == "table" and not seen[upValue] then
          seen[upValue] = true
          queue[#queue + 1] = { upValue, path .. "." .. keyName(k) .. "^" .. upName }
        end
        i = i + 1
      end
    end
    if type(k) == "table" and not seen[k] then
      seen[k] = true
      queue[#queue + 1] = { k, path .. ".[key]" }
    end
  end
  local mt = getmetatable(t)
  if type(mt) == "table" and not seen[mt] then
    seen[mt] = true
    queue[#queue + 1] = { mt, path .. ".<mt>" }
  end
  local g = groupKey(path)
  groups[g] = (groups[g] or 0) + n
end

local rows = {}
for g, n in pairs(groups) do
  rows[#rows + 1] = { g, n, n - (prev[g] or 0) }
end
table.sort(rows, function(a, b)
  if a[3] ~= b[3] then return a[3] > b[3] end
  return a[2] > b[2]
end)

local out = { string.format("[LuaHeap] %d reachable tables, %d groups; top growth since last report (entries, delta):", objectCount, #rows) }
local typeNames = { "table", "function", "userdata", "thread", "string", "stringBytes", "number" }
local typeLine = {}
for _, name in ipairs(typeNames) do
  local now = typeCounts[name] or 0
  typeLine[#typeLine + 1] = string.format("%s=%d(%+d)", name, now, now - (prevTypes[name] or 0))
end
out[#out + 1] = "[LuaHeap]   reachable values: " .. table.concat(typeLine, " ")
registry.__gam300_heap_prev_types = typeCounts
for i = 1, math.min(12, #rows) do
  out[#out + 1] = string.format("[LuaHeap]   %-60s %9d %+9d", rows[i][1], rows[i][2], rows[i][3])
end
registry.__gam300_heap_prev = groups
return table.concat(out, "\n")
)lua";

double ResolvePeriodSeconds() {
	const char* value = std::getenv("GAM300_LUA_HEAP_REPORT");
	if (value == nullptr || value[0] == '\0') {
		return 0.0;
	}
	const double parsed = std::strtod(value, nullptr);
	return parsed > 0.0 ? parsed : 0.0;
}

}  // namespace

namespace LuaHeapReport {

void Tick(lua_State* L) {
	static const double periodSeconds = ResolvePeriodSeconds();
	if (periodSeconds <= 0.0 || L == nullptr) {
		return;
	}

	using Clock = std::chrono::steady_clock;
	static Clock::time_point lastReport = Clock::now();
	const Clock::time_point now = Clock::now();
	if (std::chrono::duration<double>(now - lastReport).count() < periodSeconds) {
		return;
	}
	lastReport = now;

	const int kbBefore = lua_gc(L, LUA_GCCOUNT, 0);
	// Two full cycles: objects with __gc are resurrected for their finalizer in
	// the first cycle and only released in the second.
	lua_gc(L, LUA_GCCOLLECT, 0);
	const int kbAfterFirst = lua_gc(L, LUA_GCCOUNT, 0);
	lua_gc(L, LUA_GCCOLLECT, 0);
	const int kbAfter = lua_gc(L, LUA_GCCOUNT, 0);

	const int top = lua_gettop(L);
	if (luaL_dostring(L, kReportChunk) != LUA_OK) {
		std::printf("[LuaHeap] report failed: %s\n", lua_tostring(L, -1));
		lua_settop(L, top);
		return;
	}
	std::printf("[LuaHeap] live %d KB after two full collects (%d KB before, %d KB after the first)\n%s\n",
		kbAfter, kbBefore, kbAfterFirst, lua_tostring(L, -1));
	std::fflush(stdout);
	lua_settop(L, top);
}

}  // namespace LuaHeapReport

#else

namespace LuaHeapReport {

void Tick(lua_State*) {}

}  // namespace LuaHeapReport

#endif
