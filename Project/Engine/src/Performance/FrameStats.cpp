#include "pch.h"
#include "Performance/FrameStats.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#ifdef ANDROID
#include <android/log.h>
#endif

namespace {

constexpr std::size_t kMaxZonesPerThread = 160;
constexpr std::size_t kMaxReportedZones = 32;
constexpr std::size_t kMaxReportedSpikes = 8;
constexpr std::size_t kFrameSampleCapacity = 2048;
constexpr double kDefaultReportIntervalSeconds = 5.0;

struct Zone {
	const char* name = nullptr;
	std::int64_t frameNs = 0;       // accumulated since the last EndFrame
	std::int64_t windowNs = 0;      // accumulated over the reporting window
	std::int64_t maxFrameNs = 0;    // worst single-frame total in the window
	std::uint64_t frameCalls = 0;
	std::uint64_t windowCalls = 0;
};

// One table per thread. RecordZone only ever touches the calling thread's
// table, so the mutex is uncontended except for the brief fold in EndFrame.
struct ThreadTable {
	std::mutex mutex;
	Zone zones[kMaxZonesPerThread];
	std::size_t zoneCount = 0;
	std::size_t droppedZones = 0;
	int threadIndex = 0;  // registration order; 0 is whichever thread registered first
};

struct Registry {
	std::mutex mutex;
	std::vector<ThreadTable*> tables;
	int nextThreadIndex = 0;
};

Registry& GetRegistry() {
	static Registry registry;
	return registry;
}

// Owns the calling thread's table for the lifetime of the thread and keeps the
// registry consistent when the thread exits.
struct ThreadTableHandle {
	ThreadTable* table;

	ThreadTableHandle() : table(new ThreadTable()) {
		Registry& registry = GetRegistry();
		std::lock_guard<std::mutex> lock(registry.mutex);
		table->threadIndex = registry.nextThreadIndex++;
		registry.tables.push_back(table);
	}

	~ThreadTableHandle() {
		Registry& registry = GetRegistry();
		std::lock_guard<std::mutex> lock(registry.mutex);
		registry.tables.erase(
			std::remove(registry.tables.begin(), registry.tables.end(), table),
			registry.tables.end());
		delete table;
	}
};

ThreadTable& LocalTable() {
	thread_local ThreadTableHandle handle;
	return *handle.table;
}

// Frame-level bookkeeping belongs to the thread that calls EndFrame.
struct FrameState {
	bool started = false;
	FrameStats::Clock::time_point lastFrameEnd{};
	FrameStats::Clock::time_point windowStart{};
	double reportIntervalSeconds = kDefaultReportIntervalSeconds;
	int reportingThreadIndex = -1;

	std::uint64_t windowFrames = 0;
	std::int64_t windowFrameNs = 0;
	std::int64_t windowMaxFrameNs = 0;
	std::int64_t frameSamples[kFrameSampleCapacity];
	std::size_t frameSampleCount = 0;
};

FrameState g_frame;

double ResolveReportInterval() {
#ifndef ANDROID
	if (const char* value = std::getenv("GAM300_FRAME_STATS_INTERVAL");
		value != nullptr && value[0] != '\0') {
		const double parsed = std::strtod(value, nullptr);
		if (parsed > 0.0) {
			return parsed;
		}
	}
#endif
	return kDefaultReportIntervalSeconds;
}

Zone* FindOrAddZone(ThreadTable& table, const char* name) {
	// Zone names are literals, so pointer identity is the common fast path;
	// the string compare only runs when a literal was not merged across
	// translation units.
	for (std::size_t i = 0; i < table.zoneCount; ++i) {
		if (table.zones[i].name == name) {
			return &table.zones[i];
		}
	}
	for (std::size_t i = 0; i < table.zoneCount; ++i) {
		if (std::strcmp(table.zones[i].name, name) == 0) {
			return &table.zones[i];
		}
	}
	if (table.zoneCount == kMaxZonesPerThread) {
		++table.droppedZones;
		return nullptr;
	}
	Zone& zone = table.zones[table.zoneCount++];
	zone = Zone{};
	zone.name = name;
	return &zone;
}

// __PRETTY_FUNCTION__ style names carry the return type and the parameter
// list; keep only the qualified function name so the report stays readable.
std::string DisplayName(const char* name) {
	std::string text(name);
	const std::size_t paren = text.find('(');
	if (paren != std::string::npos) {
		text.erase(paren);
		const std::size_t space = text.rfind(' ');
		if (space != std::string::npos) {
			text.erase(0, space + 1);
		}
	}
	constexpr std::size_t kMaxWidth = 40;
	if (text.size() > kMaxWidth) {
		text.erase(0, text.size() - kMaxWidth);
	}
	return text;
}

double ToMilliseconds(std::int64_t nanoseconds) {
	return static_cast<double>(nanoseconds) / 1.0e6;
}

void AppendLine(std::string& report, const char* format, ...) {
	char line[256];
	va_list args;
	va_start(args, format);
	std::vsnprintf(line, sizeof(line), format, args);
	va_end(args);
	report += line;
	report += '\n';
}

// The engine logger drops Info in Release and Android builds, and the report
// is the whole point of opting into this profiler, so it bypasses the log
// level: stdout on desktop, logcat on Android.
void Emit(const std::string& report) {
#ifdef ANDROID
	__android_log_print(ANDROID_LOG_INFO, "GAM300", "%s", report.c_str());
#else
	std::fputs(report.c_str(), stdout);
	std::fflush(stdout);
#endif
}

struct ReportRow {
	const char* name;
	int threadIndex;
	std::int64_t windowNs;
	std::int64_t maxFrameNs;
	std::uint64_t windowCalls;
};

// Folds every thread's per-frame accumulators into the window totals and,
// when a report is due, collects the rows and resets the window. Caller holds
// the registry mutex.
void FoldFrame(Registry& registry, bool collectRows, std::vector<ReportRow>& rows, std::size_t& droppedZones) {
	for (ThreadTable* table : registry.tables) {
		std::lock_guard<std::mutex> lock(table->mutex);
		for (std::size_t i = 0; i < table->zoneCount; ++i) {
			Zone& zone = table->zones[i];
			zone.windowNs += zone.frameNs;
			zone.maxFrameNs = std::max(zone.maxFrameNs, zone.frameNs);
			zone.windowCalls += zone.frameCalls;
			zone.frameNs = 0;
			zone.frameCalls = 0;
			if (collectRows) {
				if (zone.windowCalls > 0) {
					rows.push_back({ zone.name, table->threadIndex, zone.windowNs, zone.maxFrameNs, zone.windowCalls });
				}
				zone.windowNs = 0;
				zone.maxFrameNs = 0;
				zone.windowCalls = 0;
			}
		}
		if (collectRows) {
			droppedZones += table->droppedZones;
			table->droppedZones = 0;
		}
	}
}

void Report(std::vector<ReportRow>& rows, std::size_t droppedZones, double windowSeconds) {
	if (g_frame.windowFrames == 0) {
		return;
	}

	std::int64_t p95Ns = g_frame.windowMaxFrameNs;
	if (g_frame.frameSampleCount > 0) {
		const std::size_t rank =
			static_cast<std::size_t>(0.95 * static_cast<double>(g_frame.frameSampleCount - 1));
		std::nth_element(
			g_frame.frameSamples,
			g_frame.frameSamples + rank,
			g_frame.frameSamples + g_frame.frameSampleCount);
		p95Ns = g_frame.frameSamples[rank];
	}

	const double frames = static_cast<double>(g_frame.windowFrames);

	std::string report;
	report.reserve(4096);
	AppendLine(report,
		"[FrameStats] %llu frames in %.2f s: avg %.2f ms, p95 %.2f ms, max %.2f ms (%.1f fps)",
		static_cast<unsigned long long>(g_frame.windowFrames),
		windowSeconds,
		ToMilliseconds(g_frame.windowFrameNs) / frames,
		ToMilliseconds(p95Ns),
		ToMilliseconds(g_frame.windowMaxFrameNs),
		frames / windowSeconds);
	AppendLine(report,
		"[FrameStats]   %-40s %-7s %10s %10s %10s",
		"zone", "thread", "avg ms", "max ms", "calls/f");

	// Three groups: the zones that cost the most on average, then the worst
	// single-frame spikes not already listed (a 9 ms hitch once per minute
	// averages to nothing but is what the player notices), then counters.
	std::sort(rows.begin(), rows.end(), [](const ReportRow& a, const ReportRow& b) {
		return a.windowNs > b.windowNs;
	});
	std::vector<bool> listed(rows.size(), false);

	auto appendRow = [&](std::size_t index) {
		const ReportRow& row = rows[index];
		listed[index] = true;
		char thread[16];
		if (row.threadIndex == g_frame.reportingThreadIndex) {
			std::snprintf(thread, sizeof(thread), "main");
		}
		else {
			std::snprintf(thread, sizeof(thread), "t%d", row.threadIndex);
		}
		AppendLine(report,
			"[FrameStats]   %-40s %-7s %10.3f %10.3f %10.2f",
			DisplayName(row.name).c_str(),
			thread,
			ToMilliseconds(row.windowNs) / frames,
			ToMilliseconds(row.maxFrameNs),
			static_cast<double>(row.windowCalls) / frames);
	};

	const std::size_t reported = std::min(rows.size(), kMaxReportedZones);
	for (std::size_t i = 0; i < reported; ++i) {
		if (rows[i].windowNs > 0) {
			appendRow(i);
		}
	}

	std::vector<std::size_t> spikes;
	for (std::size_t i = 0; i < rows.size(); ++i) {
		if (!listed[i] && rows[i].maxFrameNs > 0) {
			spikes.push_back(i);
		}
	}
	std::sort(spikes.begin(), spikes.end(), [&rows](std::size_t a, std::size_t b) {
		return rows[a].maxFrameNs > rows[b].maxFrameNs;
	});
	if (!spikes.empty()) {
		AppendLine(report, "[FrameStats]   -- worst single-frame spikes not listed above --");
		const std::size_t spikeCount = std::min(spikes.size(), kMaxReportedSpikes);
		for (std::size_t i = 0; i < spikeCount; ++i) {
			appendRow(spikes[i]);
		}
	}

	bool counterHeader = false;
	for (std::size_t i = 0; i < rows.size(); ++i) {
		if (!listed[i] && rows[i].windowNs == 0 && rows[i].windowCalls > 0) {
			if (!counterHeader) {
				AppendLine(report, "[FrameStats]   -- counters (per frame) --");
				counterHeader = true;
			}
			appendRow(i);
		}
	}
	if (droppedZones > 0) {
		AppendLine(report,
			"[FrameStats]   (%zu zone records dropped: more than %zu distinct zones on one thread)",
			droppedZones, kMaxZonesPerThread);
	}

	Emit(report);
}

void ResetWindow(FrameStats::Clock::time_point now) {
	g_frame.windowStart = now;
	g_frame.windowFrames = 0;
	g_frame.windowFrameNs = 0;
	g_frame.windowMaxFrameNs = 0;
	g_frame.frameSampleCount = 0;
}

}  // namespace

namespace FrameStats {

void RecordZone(const char* name, std::int64_t nanoseconds) {
	ThreadTable& table = LocalTable();
	std::lock_guard<std::mutex> lock(table.mutex);
	if (Zone* zone = FindOrAddZone(table, name)) {
		zone->frameNs += nanoseconds;
		++zone->frameCalls;
	}
}

void RecordCount(const char* name, std::uint64_t count) {
	ThreadTable& table = LocalTable();
	std::lock_guard<std::mutex> lock(table.mutex);
	if (Zone* zone = FindOrAddZone(table, name)) {
		zone->frameCalls += count;
	}
}

void EndFrame() {
	const Clock::time_point now = Clock::now();
	Registry& registry = GetRegistry();
	std::lock_guard<std::mutex> registryLock(registry.mutex);

	if (!g_frame.started) {
		g_frame.started = true;
		g_frame.reportIntervalSeconds = ResolveReportInterval();
		g_frame.reportingThreadIndex = LocalTable().threadIndex;
		g_frame.lastFrameEnd = now;
		ResetWindow(now);
		// Discard whatever accumulated before the first frame boundary.
		std::vector<ReportRow> discarded;
		std::size_t dropped = 0;
		FoldFrame(registry, true, discarded, dropped);
		return;
	}

	const std::int64_t frameNs =
		std::chrono::duration_cast<std::chrono::nanoseconds>(now - g_frame.lastFrameEnd).count();
	g_frame.lastFrameEnd = now;

	++g_frame.windowFrames;
	g_frame.windowFrameNs += frameNs;
	g_frame.windowMaxFrameNs = std::max(g_frame.windowMaxFrameNs, frameNs);
	if (g_frame.frameSampleCount < kFrameSampleCapacity) {
		g_frame.frameSamples[g_frame.frameSampleCount++] = frameNs;
	}

	const double windowSeconds =
		std::chrono::duration<double>(now - g_frame.windowStart).count();
	const bool reportDue = windowSeconds >= g_frame.reportIntervalSeconds;

	std::vector<ReportRow> rows;
	std::size_t droppedZones = 0;
	FoldFrame(registry, reportDue, rows, droppedZones);

	if (reportDue) {
		Report(rows, droppedZones, windowSeconds);
		ResetWindow(now);
	}
}

}  // namespace FrameStats
