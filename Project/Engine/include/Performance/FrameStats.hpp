#pragma once

// Lightweight in-engine frame profiler.
//
// Opt in with the GAM300_FRAME_STATS CMake option. The PROFILE_* macros in
// Logging.hpp then time each zone with steady_clock and every few seconds a
// summary of the calling thread's frame (avg / p95 / max frame time plus the
// most expensive zones) is written through the normal engine log. Unlike
// Tracy this needs no network connection or GUI, so it works on a phone over
// logcat, and it only reports zones from the thread that calls EndFrame (the
// main thread); job-system zones are timed but never reported.

#include <chrono>
#include <cstdint>

#include "Engine.h"

namespace FrameStats {

using Clock = std::chrono::steady_clock;

// Adds an elapsed duration to the named zone on the calling thread. The name
// must outlive the process (string literals and __FUNCTION__ qualify).
ENGINE_API void RecordZone(const char* name, std::int64_t nanoseconds);

// Adds to a per-frame counter (draw calls, for example). Counters appear in the
// report with zero time and the count in the calls-per-frame column.
ENGINE_API void RecordCount(const char* name, std::uint64_t count);

// Marks the end of a frame on the calling thread and emits a report when the
// reporting interval has elapsed.
ENGINE_API void EndFrame();

// RAII zone timer used by the PROFILE_* macros.
class Scope {
public:
	explicit Scope(const char* name) noexcept
		: m_name(name), m_start(Clock::now()) {}

	~Scope() {
		const auto elapsed = Clock::now() - m_start;
		RecordZone(
			m_name,
			std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
	}

	Scope(const Scope&) = delete;
	Scope& operator=(const Scope&) = delete;

private:
	const char* m_name;
	Clock::time_point m_start;
};

}  // namespace FrameStats
