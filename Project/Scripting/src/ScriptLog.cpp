// ScriptLog.cpp
//
// Core implementation of ScriptLog: SetBackend and Logf.
// This file is platform-agnostic and links to platform-specific backends
// implemented in separate translation units.

#include "ScriptLog.h"
#include <cstdarg>
#include <mutex>
#include <memory>
#include <vector>
#include <cstdio>

namespace Scripting {
    namespace Log {

        namespace {
            std::shared_ptr<Backend> s_backend;
            std::mutex s_backendMutex;
        } // anon

        void SetBackend(std::shared_ptr<Backend> backend) {
            std::lock_guard<std::mutex> lk(s_backendMutex);
            s_backend = backend;
        }

        void Logf(Level lvl, const char* fmt, ...) {
            if (!fmt) return;

            va_list ap;
            va_start(ap, fmt);

            // Try formatting into small stack buffer; otherwise allocate.
            char stackbuf[1024];
            va_list ap2;
            va_copy(ap2, ap);
            int needed = vsnprintf(stackbuf, sizeof(stackbuf), fmt, ap2);
            va_end(ap2);

            std::string out;
            if (needed >= 0 && needed < static_cast<int>(sizeof(stackbuf))) {
                out.assign(stackbuf, needed);
            }
            else {
                int size = (needed > 0) ? (needed + 1) : 2048;
                std::vector<char> heapbuf(size);
                vsnprintf(heapbuf.data(), heapbuf.size(), fmt, ap);
                // vsnprintf writes up to size-1 chars plus null terminator
                out.assign(heapbuf.data(), std::min<int>(needed, static_cast<int>(heapbuf.size() - 1)));
            }

            va_end(ap);

            std::shared_ptr<Backend> backendCopy;
            {
                std::lock_guard<std::mutex> lk(s_backendMutex);
                backendCopy = s_backend;
            }

            if (backendCopy) {
                backendCopy->Log(lvl, out.c_str());
            }
            else {
                // fallback to stderr
                const char* prefix = (lvl == Level::Info) ? "[scripting][info] " :
                    (lvl == Level::Warn) ? "[scripting][warn] " : "[scripting][error] ";
                fprintf(stderr, "%s%s\n", prefix, out.c_str());
                fflush(stderr);
            }
        }

    } // namespace Log
} // namespace Scripting
