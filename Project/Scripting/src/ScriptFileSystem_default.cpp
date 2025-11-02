// ScriptFileSystem_default.cpp
// Non-Windows default file system implementation (POSIX-like).
// Compiles on non-Windows platforms and implements CreateDefaultFileSystem().

#include "ScriptFileSystem.h"
#include "ScriptingRuntime.h" // for IScriptFileSystem (if needed)
#include "Logging.hpp"

#ifndef _WIN32

#include <sys/stat.h>
#include <fstream>
#include <memory>

namespace Scripting {

    struct DefaultFileSystem : public IScriptFileSystem {
        bool ReadAllText(const std::string& path, std::string& out) override {
            std::ifstream ifs(path, std::ios::binary);
            if (!ifs) return false;
            std::string content((std::istreambuf_iterator<char>(ifs)),
                std::istreambuf_iterator<char>());
            out.swap(content);
            return true;
        }

        bool Exists(const std::string& path) override {
            std::ifstream ifs(path);
            return static_cast<bool>(ifs);
        }

        uint64_t LastWriteTimeUtc(const std::string& path) override {
            struct stat st;
            if (stat(path.c_str(), &st) != 0) return 0;
            return static_cast<uint64_t>(st.st_mtime);
        }
    };

    std::unique_ptr<IScriptFileSystem> CreateDefaultFileSystem() {
        return std::make_unique<DefaultFileSystem>();
    }

} // namespace Scripting

#endif // !_WIN32
