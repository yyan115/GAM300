/******************************************************************
LEGACY - TO BE DELETED
********************************************************************/
#if 0
#include "pch.h"

#include "Reflection/Base64.hpp"

#include <wincrypt.h> // CryptBinaryToStringA, CryptStringToBinaryA

#pragma region Internal Function

        bool Internal_ValidateBase64(const std::string& data)
        {
            // guard
            if (data.empty()) return false;

            static const std::string base64_chars =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz"
                "0123456789+/"
                ;

            // check if the data is a valid base64 string
            for (char c : data)
            {
                if (base64_chars.find(c) == std::string::npos && c != '=') return false;
            }

            return true;
        }

        std::string Internal_GetErrorMessage()
        {
            unsigned long error_code = GetLastError();
            char* message_buffer = nullptr;
            unsigned long size = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (char*)&message_buffer, 0, nullptr
            );
            std::string message(message_buffer, size);
            LocalFree(message_buffer);
            return message;
        }

#pragma endregion

        ENGINE_API std::string Encode(const std::vector<unsigned char>& data)
        {
            // guard
            if (data.empty()) return "";

            unsigned long data_size = static_cast<unsigned long>(data.size());
            unsigned long size = 0;

            unsigned long flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;

            // get size
            if (!CryptBinaryToStringA(data.data(), data_size, flags, nullptr, &size))
            {
                Log::Error("Base64 encoding: Failed to get the size of the encoded data. " + Internal_GetErrorMessage());
                return "";
            }

            // allocate
            std::string result(size, '\0');

            // encode
            if (!CryptBinaryToStringA(data.data(), data_size, flags, &result[0], &size))
            {
                Log::Error("Base64 encoding: Failed to encode the data. " + Internal_GetErrorMessage());
                return "";
            }

            // CryptBinaryToStringA includes the null terminator in the string.
            // This removes it.
            result.pop_back();

            return result;
        }

        ENGINE_API std::vector<unsigned char> Decode(const std::string& data)
        {
            // guard
            if (data.empty()) return {};

            // validate
            if (!Internal_ValidateBase64(data))
            {
                Log::Error("Base64 decoding: The input data is not a valid base64 string.");
                return {};
            }

            unsigned long data_size = static_cast<unsigned long>(data.size() + 1);
            unsigned long size = 0;

            unsigned long flags = CRYPT_STRING_BASE64;// | CRYPT_STRING_NOCRLF;

            // get size
            if (!CryptStringToBinaryA(data.c_str(), data_size, flags, nullptr, &size, nullptr, nullptr))
            {
                Log::Error("Base64 decoding: Failed to get the size of the decoded data. " + Internal_GetErrorMessage());
                return {};
            }

            // allocate
            std::vector<unsigned char> result(size);

            // decode
            if (!CryptStringToBinaryA(data.c_str(), data_size, flags, result.data(), &size, nullptr, nullptr))
            {
                Log::Error("Base64 decoding: Failed to decode the data. " + Internal_GetErrorMessage());
                return {};
            }

            return result;
        }

    }
}
#endif