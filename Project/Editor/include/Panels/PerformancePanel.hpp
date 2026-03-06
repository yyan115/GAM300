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

#ifdef _WIN32
    HANDLE tracyProcessHandle = nullptr;
#else
    pid_t tracyProcessPid = 0;
#endif
};
