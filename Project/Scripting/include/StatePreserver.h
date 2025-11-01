#pragma once
#include <string>
#include <atomic>
#include <memory>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace Scripting {

	bool Initialize(const std::string& scriptPath); // call at app startup
	void Shutdown();                                 // call at app exit
	lua_State* GetLuaState();                        // optional (read-only use recommended)
	void Tick(float dt);                             // call every frame from main thread
	// If you want explicit control:
	void RequestReload();                            // request reload on next Tick

} // namespace Scripting


// Public API for the scripting subsystem that engine code calls.
// - Replaces your current single header with a richer API.
// - Should include: forward declarations for ScriptComponentHandle, scripting init/shutdown, tick, request reload, convenience helpers for running a script file, and API to create/destroy per-script environments.
// - Contains: doc comments for each function, thread-safety notes, lifetime semantics.
// - Use cases: called by engine startup/shutdown, main loop, editor to control scripting lifecycle.
