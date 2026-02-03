#pragma once

#include <string>
#include <unordered_map>

namespace Asset
{
    /**
     * @brief Data-only structure for cutscene timing.
     */
    struct CutsceneInfo
    {
        int frameStart = 0;   ///< Starting frame
        int frameEnd = 0;     ///< Ending frame

        // New timing system
        float fadeDuration = 0.5f;      ///< Fade in/out between boards (seconds)
        float boardDuration = 3.0f;     ///< How long a board stays before auto-advancing (seconds)
        float panelDuration = 6.0f;     ///< How long before fading to next panel on last board (seconds)
        float skipFadeDuration = 1.0f;  ///< Fade duration when skipping or ending (seconds)

        // Legacy timing (kept for compatibility)
        float preTime = 0.0f;  ///< Delay before playback starts
        float duration = 0.0f; ///< Active playback time
        float postTime = 0.0f; ///< Delay/Hold after playback ends
    };

    /**
     * @brief Container for multiple cutscene definitions.
     */
    struct Cutscene
    {
        // Map: Key = cutscene name, Value = CutsceneInfo
        std::unordered_map<std::string, CutsceneInfo> cutscenes;

        // Constructor handles parsing a standard path string instead of a custom File object
        Cutscene(const std::string& metadataPath);
    };

} // namespace Asset