#pragma once
#include "EditorPanel.hpp"
#include "imgui.h"

#ifdef _WIN32
#include <Windows.h>
#endif

/**
 * @brief Panel that displays FPS/frame time and provides a button to launch the Tracy profiler GUI
 */
class PerformancePanel : public EditorPanel {
public:
    PerformancePanel();
    virtual ~PerformancePanel();

protected:
    void OnImGuiRender() override;

private:
    void LaunchTracyProfiler();
    bool IsTracyProfilerRunning() const;

    static constexpr int FPS_HISTORY_SIZE = 120;
    float m_fpsHistory[FPS_HISTORY_SIZE] = {};
    int   m_fpsHistoryOffset = 0;
    int   m_shadowQuality = 1;  // 0=Low(128), 1=Medium(256), 2=High(512)

#ifdef _WIN32
    HANDLE tracyProcessHandle = nullptr;
#else
    pid_t tracyProcessPid = 0;
#endif
};
