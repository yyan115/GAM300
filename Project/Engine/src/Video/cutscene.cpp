#include "pch.h"
#include "Video/cutscene.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Asset
{
    // Constructs a Cutscene instance by parsing a file path
    Cutscene::Cutscene(const std::string& metadataPath)
    {
        std::ifstream file(metadataPath);
        if (!file.is_open())
        {
            // Log error: Could not open metadata file
            return;
        }

        std::string line;
        // Process each line in the metadata file.
        while (std::getline(file, line))
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
            float times[3] = { 0.0f, 0.0f, 0.0f };
            int idx = 0;

            while (std::getline(ssTime, token, ',') && idx < 3)
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
            info.preTime = times[0];
            info.duration = times[1];
            info.postTime = times[2];

            cutscenes[name] = info;
        }
    }
} // namespace Asset