#pragma once

#if !defined(_WIN32)
#include "xcontainer/xcontainer_basics.h"

// Linux/GCC parses the unused xcontainer v2 queue template strictly enough to
// require this old xcore cache-line helper name. The vendored library provides
// the implementation as xcontainer::getCacheLineSize(), so expose the legacy
// name here without editing Project/Libraries.
namespace xcore::target
{
    constexpr std::size_t getCacheLineSize() noexcept
    {
        return xcontainer::getCacheLineSize();
    }
}
#endif
