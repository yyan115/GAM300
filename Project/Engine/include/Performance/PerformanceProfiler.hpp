#pragma once

#include "Engine.h"
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <memory>

// Performance zone timing data
struct ENGINE_API ZoneTimingData {
    std::string zoneName;
    double avgTime = 0.0;      // Average time in milliseconds
    double minTime = 999999.0; // Minimum time in milliseconds
    double maxTime = 0.0;      // Maximum time in milliseconds
    uint32_t sampleCount = 0;  // Number of samples recorded
    double totalTime = 0.0;    // Total accumulated time
    
    void AddSample(double timeMs);
    void Reset();
};

// Frame timing history for graphs
struct ENGINE_API FrameTimingHistory {
    std::vector<float> frameTimes;     // Frame times in ms
    std::vector<float> fpsHistory;     // FPS values
    size_t maxFrames = 300;            // Configurable history size
    size_t currentIndex = 0;           // Circular buffer index
    bool bufferFilled = false;
    
    void AddFrame(double frameTimeMs, double fps);
    void Clear();
    void SetMaxFrames(size_t maxFrames);
};

// RAII-based profiling zone
class ENGINE_API ProfileZone {
public:
    ProfileZone(const char* zoneName);
    ~ProfileZone();
    
    // Disable copy and move
    ProfileZone(const ProfileZone&) = delete;
    ProfileZone& operator=(const ProfileZone&) = delete;
    
private:
    const char* zoneName;
    std::chrono::high_resolution_clock::time_point startTime;
};

// Main performance profiler class (singleton)
class ENGINE_API PerformanceProfiler {
public:
    static PerformanceProfiler& GetInstance();
    
    // Lifecycle
    void Initialize();
    void Shutdown();
    
    // Frame management - call once per frame
    void BeginFrame();
    void EndFrame();
    
    // Zone recording (called by ProfileZone RAII class)
    void BeginZone(const char* zoneName);
    void EndZone(const char* zoneName, double durationMs);
    
    // Data access for UI
    const FrameTimingHistory& GetFrameHistory() const { return frameHistory; }
    const std::unordered_map<std::string, ZoneTimingData>& GetZoneStatistics() const { return zoneStats; }
    
    // Configuration
    void SetHistorySize(size_t maxFrames);
    void ClearHistory();
    void EnableProfiling(bool enable) { profilingEnabled = enable; }
    bool IsProfilingEnabled() const { return profilingEnabled; }
    
    // Current frame info (pascalCase for variables)
    double GetCurrentFrameTime() const { return currentFrameTime; }
    double GetCurrentFps() const { return currentFps; }
    
private:
    PerformanceProfiler() = default;
    ~PerformanceProfiler() = default;
    
    // Disable copy and move
    PerformanceProfiler(const PerformanceProfiler&) = delete;
    PerformanceProfiler& operator=(const PerformanceProfiler&) = delete;
    
    // Thread safety
    mutable std::mutex mutex;
    
    // Timing data
    FrameTimingHistory frameHistory;
    std::unordered_map<std::string, ZoneTimingData> zoneStats;
    
    // Current frame tracking
    std::chrono::high_resolution_clock::time_point frameStartTime;
    double currentFrameTime = 0.0;
    double currentFps = 0.0;
    
    // State
    bool profilingEnabled = true;
    bool initialized = false;
};

// Convenience macros for profiling
// Note: Profiling is enabled by default in all builds
// To disable in release builds, define DISABLE_PROFILING
#if defined(DISABLE_PROFILING)
    #define PROFILE_SCOPE(name)
    #define PROFILE_FUNCTION()
#else
    // Profiling enabled - works in both debug and release
    #define PROFILE_SCOPE(name) ProfileZone profileZone##__LINE__(name)
    #define PROFILE_FUNCTION() ProfileZone profileZone##__LINE__(__FUNCTION__)
#endif
