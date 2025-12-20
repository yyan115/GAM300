#ifndef XCONTAINER_FIXEDSIZE_MEMORY_POOL_H
#define XCONTAINER_FIXEDSIZE_MEMORY_POOL_H
#pragma once

namespace xcontainer
{
    template< typename T, auto T_MAX_ENTRIES_V >
    struct fixedsize_memory_pool
    {
        inline static constexpr auto end_list_v     = ~static_cast<std::uint64_t>(0);
        inline static constexpr auto max_entries_v  = T_MAX_ENTRIES_V;

        using pool_entry    = xcontainer::lock::object<T, xcontainer::lock::semaphore>;
        using pool_list     = std::unique_ptr<pool_entry[]>;
        using index         = std::uint64_t;

        template< typename T_READ_CALLBACK>
        requires details::is_first_arg_const<T_READ_CALLBACK>
        void ReadOnly(index Index, T_READ_CALLBACK&& Callback)
        {
            const auto& Entry = m_Pool[Index];
            xcontainer::lock::scope Lk(Entry);
            Callback(Entry);
        }

        template< typename T>
        void Write(index Index, T&& Callback)
        {
            auto& Entry = m_Pool[Index];
            xcontainer::lock::scope Lk(Entry);
            Callback(Entry);
        }

        template< typename T>
        void FreeEntry(index Index, T&& Callback = [](auto&) {})
        {
            xcontainer::lock::scope Lk(m_Pool[Index].get());
            auto& Entry = m_Pool[Index].get();

            // Just in case the user wants to do something before we officially kill it
            Callback(Entry);

            // Clear entry...
            Entry.clear();

            std::uint64_t Local = m_EmptyList.load(std::memory_order_relaxed);
            do
            {
                Entry.m_GUID.m_Instance = Local;
                if (m_EmptyList.compare_exchange_weak(Local, Index, std::memory_order_release, std::memory_order_relaxed)) break;
            } while (true);
        }

        template< typename T>
        index Alloc(T&& Callback)
        {
            std::uint64_t Local = m_EmptyList.load(std::memory_order_relaxed);
            do
            {
                xcontainer::lock::scope Lk(m_Pool[Local].get());

                auto& Entry = m_Pool[Local].get();

                assert(Entry.m_GUID.m_Instance != end_list_v);
                if (m_EmptyList.compare_exchange_weak(Local, Entry.m_GUID.m_Instance, std::memory_order_release, std::memory_order_relaxed))
                {
                    Callback(Entry);
                    break;
                }
            } while (true);

            return Local;
        }

        pool_list                   m_Pool = std::make_unique<pool_entry[]>(max_entries_v);
        std::atomic<std::uint64_t>  m_EmptyList = 0;
    };
} // namespace xcontainer

#endif