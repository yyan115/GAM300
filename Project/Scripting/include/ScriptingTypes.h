#pragma once
#include <cstdint>
#include <string>
#include <functional>


namespace Scripting 
{
	// Small, copyable handle used to reference a script resource or compiled blob.
	// 0 is reserved as an invalid handle.
	using ScriptHandle = uint64_t;
	static constexpr ScriptHandle InvalidScriptHandle = 0;

	// Per-instance ID for runtime-created script environments/instances.
	using ScriptInstanceID = uint32_t;
	static constexpr ScriptInstanceID InvalidScriptInstance = 0u;

	// How reloads should be applied when a script file changes or an explicit reload is requested.
	enum class ReloadPolicy : uint8_t 
	{
		Immediate = 0, // reload as soon as possible on next Tick
		Deferred, // schedule reload at a safe synchronization point chosen by runtime
		Never // never automatically reload (manual reload only)
	};

	// Options used when loading/running a script file.
	struct ScriptLoadOptions 
	{
		bool runMain = true; // run top-level chunk immediately
		bool sandboxed = false; // load into a more isolated environment if supported
		ReloadPolicy reloadPolicy = ReloadPolicy::Immediate;
		// Optional tag that callers can use to group or identify loads
		std::string tag;
	};

	// Lightweight reference to a Lua function stored in the registry. Value 0 means 'none'.
	struct ScriptFunctionRef 
	{
		int ref = 0; // luaL_ref registry reference; 0 == none
		bool valid() const { return ref != 0; }
		void reset() { ref = 0; }
	};

	// Equality helpers
	inline bool operator==(const ScriptFunctionRef& a, const ScriptFunctionRef& b) { return a.ref == b.ref; }
	inline bool operator!=(const ScriptFunctionRef& a, const ScriptFunctionRef& b) { return !(a == b); }
} // namespace Scripting