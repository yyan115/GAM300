#include "pch.h"
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>
#include "Utilities/GUID.hpp"
#include <cassert>

GUID_string GUIDUtilities::GenerateGUIDString() {
    // Thread-local random number generator for the random component
    thread_local std::mt19937_64                                rng(std::random_device{}());
    thread_local std::uniform_int_distribution<std::uint64_t>   dist;

    // Thread-local counter, only 24 bits used to track GUIDs per thread
    thread_local std::uint32_t counter = 0;

    // Thread ID hash: 8-bit value computed once per thread from counter's address
    thread_local std::uint8_t threadSalt = []
        {
            std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(&counter);
            std::uint8_t hash = 0;
            for (size_t i = 0; i < sizeof(addr); ++i)
            {
                hash ^= static_cast<std::uint8_t>((addr >> (i * 8)) & 0xFF);
            }
            return hash;
        }();

    // Machine salt: 16-bit random value, unique per process instance
    static const std::uint16_t machineSalt = static_cast<std::uint16_t>(std::random_device{}() & 0xFFFF);

    // Epoch set to January 1, 2025 UTC (Unix timestamp: 1735689600)
    static const auto epoch = std::chrono::system_clock::from_time_t(1735689600);

    // Calculate seconds since epoch for the timestamp
    const auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - epoch).count();

    // Timestamp: 48 bits, masked to prevent overflow into other components
    const std::uint64_t timeComponent = static_cast<std::uint64_t>(timestamp & 0xFFFFFFFFFFFF);

    // Random component: 31 bits, adds entropy within the same second
    const std::uint32_t randomComponent = static_cast<std::uint32_t>(dist(rng) & 0x7FFFFFFF);

    // Counter: 24 bits, masked to stay within allocation, increments per GUID
    const std::uint32_t counterComponent = static_cast<std::uint32_t>(counter++ & 0xFFFFFF);

    // Assemble the 128-bit GUID using bitwise operations
    // Lower 64 bits: fixed bit + counter (24 bits) + timestamp (39 bits)
    const std::uint64_t lower =
        1ULL                                                    // Bit 0: fixed to 1
        | (static_cast<std::uint64_t>(counterComponent) << 1)   // Bits 1-24: counter
        | (timeComponent << 25);                                // Bits 25-63: timestamp (lower 39 bits)

    // Upper 64 bits: timestamp (9 bits) + thread ID (8 bits) + machine salt (16 bits) + random (31 bits)
    const std::uint64_t upper =
        (static_cast<std::uint64_t>(timeComponent >> 39) << 55) // Bits 0-8: timestamp (upper 9 bits)
        | (static_cast<std::uint64_t>(threadSalt) << 47)        // Bits 9-16: thread ID
        | (static_cast<std::uint64_t>(machineSalt) << 31)       // Bits 17-32: machine salt
        | (static_cast<std::uint64_t>(randomComponent) << 0);   // Bits 33-63: random

	return ConvertGUID128ToString({ upper, lower });
}

GUID_128 GUIDUtilities::ConvertStringToGUID128(const GUID_string& guidStr) {
    assert(guidStr.size() == 33 && "GUID string must be exactly 33 characters!");

    size_t dashPos = guidStr.find('-');
    assert(dashPos != std::string::npos && "GUID string must contain a dash!");

    uint64_t high = std::stoull(guidStr.substr(0, dashPos), nullptr, 16);
    uint64_t low = std::stoull(guidStr.substr(dashPos + 1), nullptr, 16);

	return { high, low };
}

GUID_string GUIDUtilities::ConvertGUID128ToString(const GUID_128& guid_128_t) {
    // Format the GUID as a hexadecimal string
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << guid_128_t.high << "-"
        << std::setw(16) << guid_128_t.low;

    return oss.str(); // "123456789abcdef0-0fedcba987654321"
}
