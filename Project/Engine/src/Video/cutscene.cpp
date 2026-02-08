#include "pch.h"
#include "Video/cutscene.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

#ifdef ANDROID
#include "WindowManager.hpp"
#include "Platform/IPlatform.h"
#endif

namespace Asset
{
    // Constructs a Cutscene instance by parsing a file path
    Cutscene::Cutscene(const std::string& metadataPath)
    {
        std::string fileContent;

#ifdef ANDROID
        IPlatform* platform = WindowManager::GetPlatform();
        if (!platform) {
            ENGINE_PRINT("[Cutscene] ERROR: Platform is null, cannot read '", metadataPath, "'\n");
            return;
        }
        // Normalize path for Android - strip leading "../"
        std::string assetPath = metadataPath;
        std::replace(assetPath.begin(), assetPath.end(), '\\', '/');
        while (assetPath.size() >= 3 && assetPath.substr(0, 3) == "../") {
            assetPath = assetPath.substr(3);
        }
        ENGINE_PRINT("[Cutscene] Reading asset: '", assetPath, "' (original: '", metadataPath, "')\n");
        std::vector<uint8_t> buffer = platform->ReadAsset(assetPath);
        if (buffer.empty()) {
            ENGINE_PRINT("[Cutscene] ERROR: Failed to read asset '", assetPath, "' (buffer empty)\n");
            return;
        }
        fileContent.assign(buffer.begin(), buffer.end());
        ENGINE_PRINT("[Cutscene] Read ", fileContent.size(), " bytes from '", assetPath, "'\n");
#else
        std::ifstream file(metadataPath);
        if (!file.is_open())
        {
            return;
        }
        std::stringstream ss;
        ss << file.rdbuf();
        fileContent = ss.str();
#endif

        std::istringstream contentStream(fileContent);
        std::string line;
        // Process each line in the metadata file.
        while (std::getline(contentStream, line))
        {
            // 1. Skip empty lines or comments
            if (line.empty() || line[0] == '#')
                continue;

            // Example line:
            // Cutscene_01_WakingUp, frames:00000-00023, time:(1.0f, 0.6f, 1.0f)

            // 2. Extract the Name (Key)
            size_t firstComma = line.find(",");
            if (firstComma == std::string::npos) continue;
            std::string name = line.substr(0, firstComma);

            // 3. Extract Frames (frameStart and frameEnd)
            size_t framesPos = line.find("frames:");
            if (framesPos == std::string::npos) continue;

            size_t commaAfterFrames = line.find(",", framesPos);
            std::string framesStr = line.substr(framesPos + 7, (commaAfterFrames - (framesPos + 7)));

            size_t dashPos = framesStr.find("-");
            if (dashPos == std::string::npos) continue;

            int frameStart = std::stoi(framesStr.substr(0, dashPos));
            int frameEnd = std::stoi(framesStr.substr(dashPos + 1));

            // 4. Extract Times (pre, duration, post)
            size_t openParen = line.find("(");
            size_t closeParen = line.find(")");
            if (openParen == std::string::npos || closeParen == std::string::npos) continue;

            std::string timeValues = line.substr(openParen + 1, closeParen - openParen - 1);
            std::stringstream ssTime(timeValues);
            std::string token;
            float times[5] = { 0.5f, 3.0f, 6.0f, 0.5f, 1.0f };  // Default values
            int idx = 0;

            while (std::getline(ssTime, token, ',') && idx < 5)
            {
                // Clean up whitespace and 'f' suffix
                token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
                if (!token.empty() && token.back() == 'f') token.pop_back();

                times[idx++] = std::stof(token);
            }

            // 5. Store in Map
            CutsceneInfo info;
            info.frameStart = frameStart;
            info.frameEnd = frameEnd;

            // Support both new (5-value) and legacy (3-value) timing formats
            if (idx >= 5)
            {
                // New format: (fadeDuration, boardDuration, panelDuration, fadeOut, skipFade)
                info.fadeDuration = times[0];
                info.boardDuration = times[1];
                info.panelDuration = times[2];
                info.postTime = times[3];  // Use as fade out for legacy compatibility
                info.skipFadeDuration = times[4];
                // Set legacy values for backwards compatibility
                info.preTime = times[0];
                info.duration = times[1];
            }
            else
            {
                // Legacy format: (preTime, duration, postTime)
                info.preTime = times[0];
                info.duration = times[1];
                info.postTime = times[2];
                // Set new values from legacy
                info.fadeDuration = times[0];
                info.boardDuration = times[1];
                info.panelDuration = times[1] * 2.0f;
                info.skipFadeDuration = 1.0f;
            }

            cutscenes[name] = info;
        }
    }
} // namespace Asset