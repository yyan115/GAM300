/////////////////////////////////////////////////////////////////////////////
// WLVERSE [https://wlverse.web.app]
// cutscene.cpp
//
// Implements the Cutscene constructor defined in cutscene.h. This function
// reads metadata from the provided file and parses cutscene frame ranges and
// timing information. It supports lines formatted as:
// Cutscene_01_WakingUp, frames:00000-00023, time:(1.0f,0.6f,1.0f)
// The parser extracts the cutscene name, frame start/end, and timing values
// (preTime, duration, postTime), and stores them in an unordered_map for quick access.
//
// AUTHORS
// [100%] Soh Wei Jie (weijie.soh\@digipen.edu)
//   - Main Author
//
// Copyright (c) 2025 DigiPen, All rights reserved.
/////////////////////////////////////////////////////////////////////////////

//#include "pch.h"
//#include "cutscene.h"
//
//namespace FlexEngine
//{
//    namespace Asset
//    {
//        // Constructs a Cutscene instance by parsing the provided metadata file.
//        Cutscene::Cutscene(File& _metadata)
//            : metadata(_metadata)
//        {
//            // Read the entire metadata file into a stringstream for line-by-line processing.
//            std::stringstream ss(metadata.Read());
//            std::string line;
//
//            // Process each line in the metadata file.
//            while (std::getline(ss, line))
//            {
//                // Skip empty lines.
//                if (line.empty())
//                    continue;
//
//                // Example line:
//                // Cutscene_01_WakingUp, frames:00000-00023, time:(1.0f,0.6f,1.0f)
//
//                // Extract the cutscene name (text before the first comma).
//                size_t firstComma = line.find(",");
//                if (firstComma == std::string::npos)
//                    continue;
//                std::string name = line.substr(0, firstComma);
//
//                // Extract the frame range string.
//                size_t framesPos = line.find("frames:");
//                if (framesPos == std::string::npos)
//                    continue;
//                size_t commaAfterFrames = line.find(",", framesPos);
//                std::string framesStr = (commaAfterFrames != std::string::npos) ?
//                    line.substr(framesPos, commaAfterFrames - framesPos) :
//                    line.substr(framesPos);
//                // Remove the "frames:" prefix.
//                const std::string framesPrefix = "frames:";
//                std::string frameRange = framesStr.substr(framesPrefix.length());
//                // Split the frame range (e.g., "00000-00023") into start and end strings.
//                size_t dashPos = frameRange.find("-");
//                if (dashPos == std::string::npos)
//                    continue;
//                std::string frameStartStr = frameRange.substr(0, dashPos);
//                std::string frameEndStr = frameRange.substr(dashPos + 1);
//                int frameStart = std::stoi(frameStartStr);
//                int frameEnd = std::stoi(frameEndStr);
//
//                // Extract the time information.
//                size_t timePos = line.find("time:");
//                if (timePos == std::string::npos)
//                    continue;
//                size_t openParenPos = line.find("(", timePos);
//                size_t closeParenPos = line.find(")", timePos);
//                if (openParenPos == std::string::npos || closeParenPos == std::string::npos)
//                    continue;
//                std::string timeStr = line.substr(openParenPos + 1, closeParenPos - openParenPos - 1);
//                // timeStr should now be like "1.0f,0.6f,1.0f"
//                std::istringstream timeStream(timeStr);
//                std::string token;
//                float preTime = 0, duration = 0, postTime = 0;
//                int tokenIndex = 0;
//                while (std::getline(timeStream, token, ','))
//                {
//                    // Remove any whitespace characters.
//                    token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
//                    // Remove trailing 'f' if present.
//                    if (!token.empty() && token.back() == 'f')
//                        token.pop_back();
//                    float value = std::stof(token);
//                    if (tokenIndex == 0)
//                        preTime = value;
//                    else if (tokenIndex == 1)
//                        duration = value;
//                    else if (tokenIndex == 2)
//                        postTime = value;
//                    tokenIndex++;
//                }
//
//                // Store the parsed data into a CutsceneInfo instance.
//                CutsceneInfo info;
//                info.frameStart = frameStart;
//                info.frameEnd = frameEnd;
//                info.preTime = preTime;
//                info.duration = duration;
//                info.postTime = postTime;
//
//                // Insert the parsed data into the unordered_map using the cutscene name as the key.
//                cutscenes[name] = info;
//            }
//        }
//
//
//
//        // Constructs a Cutscene instance by parsing the provided metadata file.
//        VideoCutscene::VideoCutscene(File& _metadata)
//            : metadata(_metadata)
//        {
//            //Format: videoPath, identifier, time:start-end, timescale:xxx
//            // Read the entire metadata file into a stringstream for line-by-line processing.
//            std::stringstream ss(metadata.Read());
//            std::string line;
//
//            // Process each line in the metadata file.
//            while (std::getline(ss, line))
//            {
//                std::istringstream ss_line(line);
//                std::string token;
//                std::vector<std::string> tokens;
//
//                // Split the line by commas.
//                while (std::getline(ss_line, token, ','))
//                {
//                    // Trim whitespace.
//                    token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
//                    tokens.push_back(token);
//                }
//
//                // We expect at least 4 tokens: videoPath, identifier, time:[start]-[end], timescale:...
//                if (tokens.size() < 4)
//                    continue;
//
//                VideoCutsceneInfo info;
//                info.videoPath = tokens[0]; // First token is the video path.
//                std::string key = tokens[1];  // Second token is used as the key for the map.
//
//                // Parse time range from token 3.
//                // Expected format: "time:0.5-1.2"
//                const std::string timePrefix = "time:";
//                if (tokens[2].find(timePrefix) != 0)
//                    continue;
//                std::string timeRange = tokens[2].substr(timePrefix.length());
//                // timeRange should now be in the format "0.5-1.2"
//                size_t dashPos = timeRange.find("-");
//                if (dashPos == std::string::npos)
//                    continue;
//                std::string startStr = timeRange.substr(0, dashPos);
//                std::string endStr = timeRange.substr(dashPos + 1);
//
//                // Convert to floats.
//                float startTime = std::stof(startStr);
//                float endTime = std::stof(endStr);
//                // Here we store the times into the struct.
//                // (If these represent time values, consider renaming to startingTime/endingTime.)
//                info.startTime = startTime;
//                info.endingTime = endTime;
//
//                // Parse timescale from token 4.
//                // Expected format: "timescale:0.4" or "timescale:0.4f"
//                const std::string timeScalePrefix = "timescale:";
//                if (tokens[3].find(timeScalePrefix) != 0)
//                    continue;
//                std::string timeScaleStr = tokens[3].substr(timeScalePrefix.length());
//                // Remove trailing 'f' if it exists.
//                if (!timeScaleStr.empty() && timeScaleStr.back() == 'f')
//                    timeScaleStr.pop_back();
//
//                info.timeScale = std::stof(timeScaleStr);
//
//                // Store the parsed cutscene info into the map using the key from token 2.
//                cutscenes[key] = info;
//            }
//        }
//    } // namespace Asset
//} // namespace FlexEngine
