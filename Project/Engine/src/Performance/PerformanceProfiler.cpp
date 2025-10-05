#include "pch.h"
#include "Performance/PerformanceProfiler.hpp"
#include <algorithm>

// ===== ZoneTimingData Implementation =====

void ZoneTimingData::AddSample(double timeMs) {
    totalTime += timeMs;
    sampleCount++;
    
    minTime = std::min(minTime, timeMs);
    maxTime = std::max(maxTime, timeMs);
    avgTime = totalTime / sampleCount;
}

void ZoneTimingData::Reset() {
    avgTime = 0.0;
    minTime = 999999.0;
    maxTime = 0.0;
    sampleCount = 0;
    totalTime = 0.0;
}

// ===== FrameTimingHistory Implementation =====

void FrameTimingHistory::AddFrame(double frameTimeMs, double fps) {
    if (frameTimes.size() < maxFrames) {
        frameTimes.push_back(static_cast<float>(frameTimeMs));
        fpsHistory.push_back(static_cast<float>(fps));
    } else {
        frameTimes[currentIndex] = static_cast<float>(frameTimeMs);
        fpsHistory[currentIndex] = static_cast<float>(fps);
        bufferFilled = true;
    }
    
    currentIndex = (currentIndex + 1) % maxFrames;
}

void FrameTimingHistory::Clear() {
    frameTimes.clear();
    fpsHistory.clear();
    currentIndex = 0;
    bufferFilled = false;
}

void FrameTimingHistory::SetMaxFrames(size_t newMaxFrames) {
    maxFrames = newMaxFrames;
    if (frameTimes.size() > maxFrames) {
        frameTimes.resize(maxFrames);
        fpsHistory.resize(maxFrames);
        currentIndex = 0;
        bufferFilled = true;
    }
}

// ===== ProfileZone Implementation =====

ProfileZone::ProfileZone(const char* zoneName) 
    : zoneName(zoneName)
    , startTime(std::chrono::high_resolution_clock::now()) {
    PerformanceProfiler::GetInstance().BeginZone(zoneName);
}

ProfileZone::~ProfileZone() {
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    double durationMs = duration.count() / 1000.0;
    
    PerformanceProfiler::GetInstance().EndZone(zoneName, durationMs);
}

// ===== PerformanceProfiler Implementation =====

PerformanceProfiler& PerformanceProfiler::GetInstance() {
    static PerformanceProfiler instance;
    return instance;
}

void PerformanceProfiler::Initialize() {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        return;
    }
    
    frameHistory.Clear();
    zoneStats.clear();
    initialized = true;
}

void PerformanceProfiler::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    
    frameHistory.Clear();
    zoneStats.clear();
    initialized = false;
}

void PerformanceProfiler::BeginFrame() {
    if (!profilingEnabled) return;
    
    frameStartTime = std::chrono::high_resolution_clock::now();
}

void PerformanceProfiler::EndFrame() {
    if (!profilingEnabled) return;
    
    auto frameEndTime = std::chrono::high_resolution_clock::now();
    auto frameDuration = std::chrono::duration_cast<std::chrono::microseconds>(frameEndTime - frameStartTime);
    
    currentFrameTime = frameDuration.count() / 1000.0; // Convert to milliseconds
    currentFps = currentFrameTime > 0.0 ? 1000.0 / currentFrameTime : 0.0;
    
    std::lock_guard<std::mutex> lock(mutex);
    frameHistory.AddFrame(currentFrameTime, currentFps);
}


void PerformanceProfiler::BeginZone(const char* zoneName) {
    // Zone start is tracked by ProfileZone's constructor timestamp
    // This function can be used for additional setup if needed
}

void PerformanceProfiler::EndZone(const char* zoneName, double durationMs) {
    if (!profilingEnabled) return;
    
    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = zoneStats.find(zoneName);
    if (it == zoneStats.end()) {
        // Create new zone entry
        ZoneTimingData newZone;
        newZone.zoneName = zoneName;
        newZone.AddSample(durationMs);
        zoneStats[zoneName] = newZone;
    } else {
        // Update existing zone
        it->second.AddSample(durationMs);
    }
}

void PerformanceProfiler::SetHistorySize(size_t maxFrames) {
    std::lock_guard<std::mutex> lock(mutex);
    frameHistory.SetMaxFrames(maxFrames);
}

void PerformanceProfiler::ClearHistory() {
    std::lock_guard<std::mutex> lock(mutex);
    frameHistory.Clear();
    
    // Reset zone statistics
    for (auto& pair : zoneStats) {
        pair.second.Reset();
    }
}
