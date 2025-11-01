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
