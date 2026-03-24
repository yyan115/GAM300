#include "Panels/PerformancePanel.hpp"
#include "imgui.h"
#include "TimeManager.hpp"
#include "EditorComponents.hpp"
#include <IconsFontAwesome6.h>
#include "Graphics/GraphicsManager.hpp"
#include "Graphics/Instancing/InstancingManager.hpp"

#ifdef _WIN32
#include <Windows.h>
#endif

PerformancePanel::PerformancePanel()
    : EditorPanel("Performance", false) {
}

PerformancePanel::~PerformancePanel() {
#ifdef _WIN32
    if (tracyProcessHandle) {
        CloseHandle(tracyProcessHandle);
        tracyProcessHandle = nullptr;
    }
#endif
}

bool PerformancePanel::IsTracyProfilerRunning() const {
#ifdef _WIN32
    if (!tracyProcessHandle) return false;
    DWORD exitCode = 0;
    if (GetExitCodeProcess(tracyProcessHandle, &exitCode)) {
        return exitCode == STILL_ACTIVE;
    }
    return false;
#else
    if (tracyProcessPid <= 0) return false;
    // Check if process is still alive (signal 0 = just check)
    return kill(tracyProcessPid, 0) == 0;
#endif
}

void PerformancePanel::LaunchTracyProfiler() {
#ifdef TRACY_PROFILER_PATH
#ifdef _WIN32
    // Clean up old handle if process has exited
    if (tracyProcessHandle) {
        if (!IsTracyProfilerRunning()) {
            CloseHandle(tracyProcessHandle);
            tracyProcessHandle = nullptr;
        } else {
            return; // Already running
        }
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    // TRACY_PROFILER_PATH is defined by CMake as an absolute path
    const char* exePath = TRACY_PROFILER_PATH;

    if (CreateProcessA(
            exePath,    // Application name
            nullptr,    // Command line
            nullptr,    // Process security attributes
            nullptr,    // Thread security attributes
            FALSE,      // Inherit handles
            0,          // Creation flags
            nullptr,    // Environment
            nullptr,    // Current directory
            &si,        // Startup info
            &pi))       // Process information
    {
        tracyProcessHandle = pi.hProcess;
        CloseHandle(pi.hThread); // Don't need the thread handle
    }
#else
    if (tracyProcessPid > 0 && IsTracyProfilerRunning()) {
        return; // Already running
    }
    // On Linux, users install tracy-profiler via package manager
    pid_t pid = fork();
    if (pid == 0) {
        execlp("tracy-profiler", "tracy-profiler", nullptr);
        _exit(1); // exec failed
    } else if (pid > 0) {
        tracyProcessPid = pid;
    }
#endif
#endif // TRACY_PROFILER_PATH
}

void PerformancePanel::OnImGuiRender() {

    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorComponents::PANEL_BG_UTILITY);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorComponents::PANEL_BG_UTILITY);

    if (ImGui::Begin(name.c_str(), &isOpen)) {
        // Header
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
        ImGui::Text("=== Performance Monitor ===");
        ImGui::PopStyleColor();

        // Current metrics from TimeManager
        double currentFps = TimeManager::GetFps();
        double frameTime = TimeManager::GetDeltaTime() * 1000.0;

        // FPS with color coding
        ImVec4 fpsColor = currentFps >= 60.0 ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
                         currentFps >= 30.0 ? ImVec4(1.0f, 0.65f, 0.0f, 1.0f) :
                         ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        ImGui::Text("Current FPS:");
        ImGui::SameLine();
        ImGui::TextColored(fpsColor, "%.1f", currentFps);

        // Frame time with color coding
        ImVec4 frameTimeColor = frameTime < 16.0 ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
                                frameTime < 35.0 ? ImVec4(1.0f, 0.65f, 0.0f, 1.0f) :
                                ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        ImGui::Text("Frame Time:");
        ImGui::SameLine();
        ImGui::TextColored(frameTimeColor, "%.3f ms", frameTime);

        ImGui::Separator();

        // ---- FPS History graph ----
        m_fpsHistory[m_fpsHistoryOffset] = (float)currentFps;
        m_fpsHistoryOffset = (m_fpsHistoryOffset + 1) % FPS_HISTORY_SIZE;

        char overlay[32];
        snprintf(overlay, sizeof(overlay), "%.1f FPS", currentFps);
        ImGui::PlotLines("##fps", m_fpsHistory, FPS_HISTORY_SIZE, m_fpsHistoryOffset,
                         overlay, 0.0f, 200.0f, ImVec2(0, 60));

        ImGui::Separator();

        // ---- Rendering Stats ----
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
        ImGui::Text("=== Rendering Stats ===");
        ImGui::PopStyleColor();

        auto& gfx = GraphicsManager::GetInstance();
        const auto& instStats = InstancingManager::GetInstance().GetStats();
        const auto& sortStats = gfx.GetSortingStats();

        // Depth prepass toggle
        bool prepassEnabled = gfx.IsDepthPrepassEnabled();
        if (ImGui::Checkbox("Depth Prepass", &prepassEnabled))
            gfx.SetDepthPrepassEnabled(prepassEnabled);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Toggle the depth prepass on/off to compare performance.\n"
                              "When ON, all opaque geometry is rendered to the depth\n"
                              "buffer first so the main pass skips hidden pixels.");

        // Instancing
        ImGui::Spacing();
        ImGui::Text("Draw Calls:      %d", instStats.drawCalls + sortStats.drawCalls);
        ImGui::Text("Instanced:       %d  (%d batches)", instStats.instancedObjects, instStats.batchCount);
        ImGui::Text("Non-Instanced:   %d", instStats.nonInstancedObjects);
        ImGui::Text("Culled:          %d", instStats.culledObjects);
        if (instStats.instancedObjects > 0)
        {
            ImGui::Text("Batch Efficiency:%.0f%%", instStats.GetBatchEfficiency());
        }

        // State switches
        ImGui::Spacing();
        ImGui::Text("Shader Switches: %d", sortStats.shaderSwitches);
        ImGui::Text("Material Switches:%d", sortStats.materialSwitches);

        ImGui::Separator();

        // Tracy Profiler section
#ifdef TRACY_PROFILER_PATH
        bool isRunning = IsTracyProfilerRunning();

        if (isRunning) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s Tracy Profiler Running", ICON_FA_CIRCLE_CHECK);
            ImGui::SameLine();
            if (ImGui::SmallButton("Open Again")) {
                LaunchTracyProfiler();
            }
        } else {
            if (ImGui::Button(ICON_FA_CHART_LINE " Open Tracy Profiler")) {
                LaunchTracyProfiler();
            }
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Launch the Tracy profiler GUI to view detailed performance data.\n"
                              "Tracy automatically connects to the running engine.");
        }
#else
        ImGui::TextDisabled("Tracy profiler GUI not found.");
        ImGui::TextDisabled("Run CMake configure to download it, or place Tracy-profiler.exe in Libraries/tracy/");
#endif

#ifdef TRACY_ENABLE
        ImGui::Spacing();
        ImGui::TextDisabled("Tracy instrumentation is active. Profiling zones and log messages");
        ImGui::TextDisabled("will appear in the Tracy GUI when connected.");
#else
        ImGui::Spacing();
        ImGui::TextDisabled("Tracy instrumentation is disabled in this build configuration.");
#endif
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
}
