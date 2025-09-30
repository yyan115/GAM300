#pragma once
/*********************************************************************************
* @File         Base64.hpp
* @Author       Soh Wei Jie, weijie.soh@digipen.edu
* @Co-Author    -
* @Date         26/9/2025
* @Brief        Implements portable, allocation-friendly Base64 encoding and decoding:
*                  - Base64_Encode: encodes a byte vector to a compact Base64 string.
*                  - Base64_Decode: decodes a Base64 string into a byte vector with
*                    whitespace-tolerant parsing, validation and padding support.
*                  - ValidateBase64: character-level validation after whitespace removal.
*                  - Internal helpers: whitespace detection and a static encoding table.
*               Design goals:
*                  - Safe: performs input validation and logs errors rather than crashing.
*                  - Portable: no platform-specific APIs, suitable for mobile and embedded.
*                  - Efficient: reserves output capacity and processes input in 3-byte chunks.
*                  - Clear error diagnostics via ENGINE_LOG_ERROR on malformed input.
*
* Usage notes:
*    - Exposes ENGINE_API functions Base64_Encode and Base64_Decode.
*    - Decoding ignores CR/LF/space/tab; however it enforces that the sanitized input
*      length is a multiple of 4 and that only valid Base64 characters (A-Z,a-z,0-9,+,/)
*      and '=' padding are used.
*    - Caller is responsible for including the project's logging macros (ENGINE_LOG_ERROR).
*
* Copyright (C) 2025 DigiPen Institute of Technology. Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen
* Institute of Technology is prohibited.
*********************************************************************************/
#include <string>
#include <vector>

#include "Logging.hpp"

#ifdef _WIN32
#ifdef ENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
#else
// Linux/GCC
#ifdef ENGINE_EXPORTS
#define ENGINE_API __attribute__((visibility("default")))
#else
#define ENGINE_API
#endif
#endif

// Encodes a vector of bytes to a base64 string.
ENGINE_API std::string Base64_Encode(const std::vector<unsigned char>& data);

// Decodes a base64 string to a vector of bytes.
ENGINE_API std::vector<unsigned char> Base64_Decode(const std::string& data);