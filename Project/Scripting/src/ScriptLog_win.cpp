// ScriptLog_win.cpp
//
// Windows logging backend for ScriptLog. Writes to OutputDebugStringA when a
// debugger is present and always writes to stderr. In debug builds it can
// optionally attach a console to allow console output when running from GUI processes.

#include "ScriptLog.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <mutex>
#include <memory>
#include <cstdio>
#include <string>

namespace Scripting {
    namespace Log {

        namespace {
            class WinBackend : public Backend {
            public:
                WinBackend(bool attachConsole) : m_attachConsole(attachConsole) {
#ifdef _WIN32
                    if (m_attachConsole) {
#if defined(_DEBUG)
                        // Try attach to parent console or allocate a new one.
                        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
                            if (AllocConsole()) {
                                // redirect stdout/stderr to the console
                                FILE* fp;
                                freopen_s(&fp,"CONOUT$", "w", stdout);
                                freopen_s(&fp,"CONOUT$", "w", stderr);
                            }
                        }
                        else 
                        {
                            // successful attach - optionally redirect
                            FILE* fp;
                            freopen_s(&fp, "CONOUT$", "w", stdout);
                            freopen_s(&fp, "CONOUT$", "w", stderr);
                        }
#endif
                    }
#endif
                }

                void Log(Level lvl, const char* msg) override {
                    if (!msg) return;
                    // prefix
                    const char* prefix = (lvl == Level::Info) ? "[scripting][info] " :
                        (lvl == Level::Warn) ? "[scripting][warn] " : "[scripting][error] ";

                    std::string out = std::string(prefix) + msg + "\n";

#ifdef _WIN32
                    if (IsDebuggerPresent()) {
                        // Output to debugger
                        OutputDebugStringA(out.c_str());
                    }
#endif
                    // Always write to stderr as fallback
                    fputs(out.c_str(), stderr);
                    fflush(stderr);
                }

            private:
                bool m_attachConsole = false;
            };

            static std::shared_ptr<Backend> s_winBackend;
        } // anonymous

        void EnsureWindowsBackend(bool attachConsole) {
#ifdef _WIN32
            if (!s_winBackend) {
                s_winBackend = std::make_shared<WinBackend>(attachConsole);
                SetBackend(s_winBackend);
            }
#else
            (void)attachConsole;
#endif
        }

    } // namespace Log
} // namespace Scripting
