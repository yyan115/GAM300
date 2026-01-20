/////////////////////////////////////////////////////////////////////////////
// WLVERSE [https://wlverse.web.app]
// cutscene.h
//
// Provides data structures for storing cutscene timing and frame range information.
// 
// This header defines the CutsceneInfo structure, which encapsulates the frame range
// and timing details for a cutscene, as well as the Cutscene structure that parses
// and stores these data from metadata files.
//
// AUTHORS
// [100%] Soh Wei Jie (weijie.soh\@digipen.edu)
//   - Main Author
//
// Copyright (c) 2025 DigiPen, All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "flx_api.h"
#include "Utilities/file.h"
#include "assetkey.h"

namespace FlexEngine
{
    namespace Asset
    {
        /**
         * @brief Structure to store frame range and timing information for a cutscene.
         *
         * Contains the starting and ending frames as well as the timing parameters for
         * pre-transition, main duration, and post-transition phases of a cutscene.
         */
        struct CutsceneInfo
        {
            int frameStart;  ///< The starting frame of the cutscene.
            int frameEnd;    ///< The ending frame of the cutscene.
            float preTime;   ///< Time before the main cutscene starts (pre-transition duration).
            float duration;  ///< Duration of the main cutscene sequence.
            float postTime;  ///< Time after the main cutscene ends (post-transition duration).
        };

        /**
         * @brief Encapsulates cutscene metadata and provides quick access to cutscene timing info.
         *
         * The Cutscene structure holds a reference to the metadata file and an unordered_map
         * that maps cutscene names to their respective CutsceneInfo. This allows for efficient
         * retrieval of cutscene timing and frame range details.
         */
        struct __FLX_API Cutscene
        {
            File& metadata;  ///< Reference to the file containing cutscene metadata.

            // Unordered map for quick access:
            // Key = cutscene name, Value = CutsceneInfo (frame range and timing data)
            std::unordered_map<std::string, CutsceneInfo> cutscenes;

            /**
             * @brief Constructs a Cutscene instance by parsing the provided metadata file.
             *
             * @param _metadata Reference to the File containing cutscene metadata.
             */
            Cutscene(File& _metadata);
        };


        //struct VideoCutsceneInfo
        //{
        //    std::string videoPath;
        //    float startTime = 0.0f;
        //    float endingTime = 0.0f;
        //    float timeScale = 0.0f;
        //};

        //struct __FLX_API VideoCutscene
        //{
        //    File& metadata;  ///< Reference to the file containing cutscene metadata.
        //    // Key = cutscene name, Value = CutsceneInfo (frame range and timing data)
        //    std::unordered_map<std::string, VideoCutsceneInfo> cutscenes;

        //    VideoCutscene(File& _metadata);
        //};

    } // namespace Asset
} // namespace FlexEngine
