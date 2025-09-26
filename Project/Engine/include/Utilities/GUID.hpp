#pragma once
#include <string>
#include <cstdint>
#include "../Engine.h"  // For ENGINE_API macro

// GUIDs are written to .meta files as strings.
using GUID_string = std::string;

// Internally, we use a 128-bit structure for asset GUIDs for better performance.
struct GUID_128 {
	uint64_t high;
	uint64_t low;
	bool operator==(const GUID_128& other) const {
		return high == other.high && low == other.low;
	}
};

// Specialize std::hash for GUID_128_t for use in std::unordered_map.
namespace std {
    template <>
    struct hash<GUID_128> {
        size_t operator()(const GUID_128& guid) const noexcept {
            return hash<uint64_t>()(guid.high) ^ (hash<uint64_t>()(guid.low) << 1);
        }
    };
}

class GUIDUtilities {
public:
	ENGINE_API static GUID_string GenerateGUIDString();
	ENGINE_API static GUID_128  ConvertStringToGUID128(const GUID_string& guidStr);
	static GUID_string ConvertGUID128ToString(const GUID_128& guid_128_t);
};