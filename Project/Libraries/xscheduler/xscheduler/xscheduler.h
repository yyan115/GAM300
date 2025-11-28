#ifndef XCORE_SCHEDULER_H
#define XCORE_SCHEDULER_H
#pragma once

//
// standard library
//
#include <atomic>
#include <array>
#include <span>
#include <thread>
#include <functional>
#include <coroutine>
#include <cstdint>
#include <concepts>
#include <cassert>
#include <variant>
#include <format>
#include <condition_variable>

//
// Platform specific
// Simple keyword to show which functions are in the quantum world
//
#define xquatum
#ifdef _WIN32
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <processthreadsapi.h>
    #undef DELETE
#elif __linux__
    #include <pthread.h>
#elif __APPLE__
    #include <pthread.h>
#endif

//
// Dependencies
//
#include "xcontainer/xcontainer_lockless_queue.h"
#include "xcontainer/xcontainer_lockless_pool.h"
#include "xcontainer/xcontainer_function.h"

//
// Predifinitions
//
namespace xscheduler
{
    class system;

    struct universal_string
    {
        const char*     m_pStr; // = T_STR_V.value1.data();
        const wchar_t*  m_pWStr;// = T_STR_V.value2.data();
    };

    namespace details
    {
        template<std::size_t N>
        struct str_literal
        {
            std::array<char,    N> value1{};
            std::array<wchar_t, N> value2{};

            consteval str_literal(const char(&str)[N])
            {
                for (std::size_t i = 0; i < N; ++i)
                    value1[i] = str[i];

                for (std::size_t i = 0; i <N; ++i)
                    value2[i] = static_cast<wchar_t>(str[i]);
            }
        };
    }

    template< details::str_literal T_NAME_V>
    static constexpr universal_string str_v = universal_string{ T_NAME_V.value1.data(), T_NAME_V.value2.data() };
}

//
// Core header files
//
#include "xscheduler_jobs.h"
#include "xscheduler_triggers.h"
#include "xscheduler_system.h"
#include "xscheduler_task_group.h"

//
// Implementations
//
#include "Implementation/xscheduler_system_inline.h"
#include "Implementation/xscheduler_jobs_inline.h"
#include "Implementation/xscheduler_triggers_inline.h"
#include "Implementation/xscheduler_task_group_inline.h"

#endif
