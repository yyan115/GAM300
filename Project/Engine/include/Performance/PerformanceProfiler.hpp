#pragma once

#include "Engine.h"

#ifndef DISABLE_PROFILING
#if defined(_DEBUG) || defined(DEBUG)
#define DISABLE_PROFILING 0 
#else
#define DISABLE_PROFILING 0 // for now make it enable for all builds
#endif
#endif

// Performance zone timing data with history
struct ZoneTimingData {
    std::string zoneName;
    double avgTime = 0.0;      // Average time in milliseconds
    double minTime = 999999.0; // Minimum time in milliseconds
    double maxTime = 0.0;      // Maximum time in milliseconds
    uint32_t sampleCount = 0;  // Number of samples recorded
    double totalTime = 0.0;    // Total accumulated time
    
    // History for graphing (last N samples)
    std::vector<float> history;
    size_t maxHistory = 300;
    size_t historyIndex = 0;
    bool historyFilled = false;
    
    void AddSample(double timeMs);
    void Reset();
};

// Frame timing history for graphs
struct FrameTimingHistory {
    std::vector<float> frameTimes;     // Frame times in ms
    std::vector<float> fpsHistory;     // FPS values
    size_t maxFrames = 300;            // Configurable history size
    size_t currentIndex = 0;           // Circular buffer index
    bool bufferFilled = false;
    
    void AddFrame(double frameTimeMs, double fps);
    void Clear();
    void SetMaxFrames(size_t maxFrames);
};

// RAII-based profiling zone (optimized - no BeginZone call needed)
class ProfileZone {
public:
    explicit ProfileZone(const char* zoneName);
    ~ProfileZone();
    
    // Disable copy and move
    ProfileZone(const ProfileZone&) = delete;
    ProfileZone& operator=(const ProfileZone&) = delete;
    
private:
    const char* zoneName;
    std::chrono::high_resolution_clock::time_point startTime;
};

// Main performance profiler class (singleton)
class PerformanceProfiler {
public:
    ENGINE_API static PerformanceProfiler& GetInstance();
    
    // Frame management - call once per frame
    void ENGINE_API BeginFrame();
    void ENGINE_API EndFrame();
    
    // Zone recording (called by ProfileZone RAII class)
    void EndZone(const char* zoneName, double durationMs);
    
    // Data access for UI
    const FrameTimingHistory& GetFrameHistory() const { return frameHistory; }
    const std::unordered_map<std::string, ZoneTimingData>& GetZoneStatistics() const { return zoneStats; }
    
    // Configuration
    void ENGINE_API ClearHistory();
    void EnableProfiling(bool enable) { profilingEnabled = enable; }
    bool IsProfilingEnabled() const { return profilingEnabled; }
    
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
    
    // State
    bool profilingEnabled = false;
};

// Convenience macros for profiling
// Note: Profiling is enabled by default in all builds
// To disable in release builds, define DISABLE_PROFILING
#if DISABLE_PROFILING
    #define PROFILE_SCOPE(name)
    #define PROFILE_FUNCTION()
#else
    // Profiling enabled - works in both debug and release
    #define PROFILE_SCOPE(name) ProfileZone profileZone##__LINE__(name)
    #define PROFILE_FUNCTION() ProfileZone profileZone##__LINE__(__FUNCTION__)
#endif
