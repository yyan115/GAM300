// WLVERSE [https://wlverse.web.app]
// flexbase64.h
//
#pragma once

/******************************************************************
LEGACY - TO BE DELETED
********************************************************************/
#include <string>
#include <vector>

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
ENGINE_API std::string Encode(const std::vector<unsigned char>& data);

// Decodes a base64 string to a vector of bytes.
ENGINE_API std::vector<unsigned char> Decode(const std::string& data);