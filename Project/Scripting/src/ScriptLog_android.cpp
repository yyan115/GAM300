// ScriptLog_android.cpp
//
// Android logging backend for ScriptLog. Uses __android_log_print when compiled
// for Android. When not compiled for Android it falls back to printing to stderr.

#include "ScriptLog.h"

#if defined(__ANDROID__)
#include <android/log.h>
#endif

#include <memory>
#include <cstdio>
#include <string>

namespace Scripting {
    namespace Log {

        namespace {
            class AndroidBackend : public Backend {
            public:
                AndroidBackend(const char* tag = "scripting") : m_tag(tag ? tag : "scripting") {}
                void Log(Level lvl, const char* msg) override {
                    if (!msg) return;
#if defined(__ANDROID__)
                    int prio = ANDROID_LOG_INFO;
                    if (lvl == Level::Warn) prio = ANDROID_LOG_WARN;
                    else if (lvl == Level::Error) prio = ANDROID_LOG_ERROR;
                    __android_log_print(prio, m_tag, "%s", msg);
#else
                    // fallback to stderr on non-Android builds
                    const char* prefix = (lvl == Level::Info) ? "[scripting][info] " :
                        (lvl == Level::Warn) ? "[scripting][warn] " : "[scripting][error] ";
                    std::string out = std::string(prefix) + msg + "\n";
                    fputs(out.c_str(), stderr);
                    fflush(stderr);
#endif
                }
            private:
                const char* m_tag;
            };

            static std::shared_ptr<Backend> s_androidBackend;
        } // anonymous

        void EnsureAndroidBackend(const char* tag) {
#if defined(__ANDROID__)
            if (!s_androidBackend) {
                s_androidBackend = std::make_shared<AndroidBackend>(tag);
                SetBackend(s_androidBackend);
            }
#else
            (void)tag;
#endif
        }

    } // namespace Log
} // namespace Scripting
