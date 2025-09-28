#include "pch.h"

#include "Reflection/Base64.hpp"

#pragma region Internal Function

static const char b64_table[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

inline bool is_whitespace(char c) {
    return c == '\r' || c == '\n' || c == ' ' || c == '\t';
}

// validate base64 characters (after whitespace removal)
bool ValidateBase64(const std::string& s) {
    if (s.empty()) return false;

    // valid set: A-Z a-z 0-9 + / and = for padding
    for (char c : s) {
        if (is_whitespace(c)) continue;
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc == '=') continue;
        bool ok = false;
        // check table
        for (int i = 0; i < 64; ++i) {
            if (b64_table[i] == c) { ok = true; break; }
        }
        if (!ok) return false;
    }
    return true;
}
#pragma endregion

// Public API
ENGINE_API std::string Base64_Encode(const std::vector<unsigned char>& data)
{
    if (data.empty()) return "";

    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);

    size_t i = 0;
    const size_t n = data.size();
    while (i + 2 < n) {
        uint32_t triple = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) | uint32_t(data[i + 2]);
        out.push_back(b64_table[(triple >> 18) & 0x3F]);
        out.push_back(b64_table[(triple >> 12) & 0x3F]);
        out.push_back(b64_table[(triple >> 6) & 0x3F]);
        out.push_back(b64_table[triple & 0x3F]);
        i += 3;
    }

    size_t rem = n - i;
    if (rem) {
        uint32_t triple = uint32_t(data[i]) << 16;
        if (rem == 2) triple |= uint32_t(data[i + 1]) << 8;

        out.push_back(b64_table[(triple >> 18) & 0x3F]);
        out.push_back(b64_table[(triple >> 12) & 0x3F]);
        out.push_back(rem == 2 ? b64_table[(triple >> 6) & 0x3F] : '=');
        out.push_back('=');
    }

    return out;
}

ENGINE_API std::vector<unsigned char> Base64_Decode(const std::string& input)
{
    if (input.empty()) return {};

    // Remove whitespace characters first.
    std::string s;
    s.reserve(input.size());
    for (char c : input) {
        if (!is_whitespace(c)) s.push_back(c);
    }

    // Basic validation
    if (s.size() % 4 != 0) {
        ENGINE_LOG_ERROR("Base64 decoding: invalid input length (not a multiple of 4).");
        return {};
    }
    if (!ValidateBase64(s)) {
        ENGINE_LOG_ERROR("Base64 decoding: invalid characters in input.");
        return {};
    }

    // Build reverse lookup table
    int rev[256];
    for (int i = 0; i < 256; ++i) rev[i] = -1;
    for (int i = 0; i < 64; ++i) rev[static_cast<unsigned char>(b64_table[i])] = i;

    // Count padding
    size_t padding = 0;
    if (!s.empty()) {
        if (s[s.size() - 1] == '=') ++padding;
        if (s.size() > 1 && s[s.size() - 2] == '=') ++padding;
    }

    std::vector<unsigned char> out;
    out.reserve((s.size() / 4) * 3 - padding);

    for (size_t i = 0; i < s.size(); i += 4) 
    {
        int v0 = (s[i] == '=') ? 0 : rev[static_cast<unsigned char>(s[i])];
        int v1 = (s[i + 1] == '=') ? 0 : rev[static_cast<unsigned char>(s[i + 1])];
        int v2 = (s[i + 2] == '=') ? 0 : rev[static_cast<unsigned char>(s[i + 2])];
        int v3 = (s[i + 3] == '=') ? 0 : rev[static_cast<unsigned char>(s[i + 3])];

        // If s[i+2] is not '=' and v2 < 0 -> invalid. Same for v3.
        if (v0 < 0 || v1 < 0) {
            ENGINE_LOG_ERROR("Base64 decoding: invalid character encountered.");
            return {};
        }
        if (s[i + 2] != '=' && v2 < 0) {
            ENGINE_LOG_ERROR("Base64 decoding: invalid character encountered at position (i+2).");
            return {};
        }
        if (s[i + 3] != '=' && v3 < 0) {
            ENGINE_LOG_ERROR("Base64 decoding: invalid character encountered at position (i+3).");
            return {};
        }

        uint32_t triple = (uint32_t(v0) << 18) | (uint32_t(v1) << 12) | (uint32_t(v2) << 6) | uint32_t(v3);

        out.push_back(static_cast<unsigned char>((triple >> 16) & 0xFF));
        if (s[i + 2] != '=') out.push_back(static_cast<unsigned char>((triple >> 8) & 0xFF));
        if (s[i + 3] != '=') out.push_back(static_cast<unsigned char>(triple & 0xFF));
    }

    return out;
}