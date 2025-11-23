#ifndef XCONTAINER_QUEUE_H
#define XCONTAINER_QUEUE_H
#pragma once

#ifndef XCONTAINER_BASICS_H
    #include "xcontainer_basics.h"
#endif

namespace xcontainer::queue
{
    namespace v1
    {
        //------------------------------------------------------------------------------
        // https://github.com/craflin/LockFreeQueue
        //------------------------------------------------------------------------------
        template< typename T, std::size_t T_CAPACITY >
        class mpmc_bounded
        {
        public:

            static constexpr auto mask_v        = T_CAPACITY - 1;
            static constexpr auto capacity_v    = T_CAPACITY;
            static_assert(((mask_v)&capacity_v) == 0, "Queue size must be power of 2");

        public:

            mpmc_bounded(const mpmc_bounded&) = delete;
            mpmc_bounded& operator =      (const mpmc_bounded&) = delete;

            constexpr mpmc_bounded(void) noexcept
            {
                for (std::size_t i = 0; i < capacity_v; ++i)
                    m_Buffer[i].m_Sequence.store(i, std::memory_order_relaxed);

                m_EnqueuePos.store(0, std::memory_order_relaxed);
                m_DequeuePos.store(0, std::memory_order_relaxed);
            }

            std::size_t size(void) const noexcept
            {
                const size_t head = m_DequeuePos.load(std::memory_order_acquire);
                return m_EnqueuePos.load(std::memory_order_relaxed) - head;
            }

            static constexpr std::size_t capacity(void) noexcept
            {
                return capacity_v;
            }

            template< typename... T_ARGS >
            bool push(T_ARGS&&... Args) noexcept
            {
                cell_t* pCell;
                std::size_t pos = m_EnqueuePos.load(std::memory_order_relaxed);

                while (true)
                {
                    pCell = &m_Buffer[pos & mask_v];

                    const std::size_t     seq = pCell->m_Sequence.load(std::memory_order_acquire);
                    const std::intptr_t   dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

                    if (dif == 0)
                    {
                        if (m_EnqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                            break;
                    }
                    else
                    {
                        if (dif < 0) return false;
                        pos = m_EnqueuePos.load(std::memory_order_relaxed);
                    }
                }

                new(&pCell->m_Data) T{ std::forward<T_ARGS>(Args)... };
                pCell->m_Sequence.store(pos + 1, std::memory_order_release);
                return true;
            }

            bool steal(T& Data) noexcept { return pop(Data); }
            bool pop(T& Data) noexcept
            {
                cell_t* pCell;
                std::size_t     pos = m_DequeuePos.load(std::memory_order_relaxed);

                while (true)
                {
                    pCell = &m_Buffer[pos & mask_v];

                    const std::size_t     seq = pCell->m_Sequence.load(std::memory_order_acquire);
                    const std::intptr_t   dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

                    if (dif == 0)
                    {
                        if (m_DequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                            break;
                    }
                    else
                    {
                        if (dif < 0) return false;
                        pos = m_DequeuePos.load(std::memory_order_relaxed);
                    }
                }

                Data = std::move(pCell->m_Data);
                pCell->m_Sequence.store(pos + mask_v + 1, std::memory_order_release);

                return true;
            }

        protected:

            struct cell_t
            {
                alignas(xcontainer::getCacheLineSize())  std::atomic<size_t>    m_Sequence;
                T                                                               m_Data;
            };

        protected:

            alignas(xcontainer::getCacheLineSize()) std::atomic<size_t> m_EnqueuePos{ 0 };
            alignas(xcontainer::getCacheLineSize()) std::atomic<size_t> m_DequeuePos{ 0 };
            std::array< cell_t, capacity_v >                            m_Buffer;
        };
    }

    namespace v2
    {
        //------------------------------------------------------------------------------
        // https://github.com/rigtorp/MPMCQueue
        //------------------------------------------------------------------------------
        template< typename T, std::size_t T_CAPACITY >
        class mpmc_bounded
        {
        public:

            using self = mpmc_bounded<T, T_CAPACITY>;
            static constexpr auto mask_v        = T_CAPACITY - 1;
            static constexpr auto capacity_v    = T_CAPACITY;
            static constexpr auto pow_v         = xcontainer::details::Log2Int(capacity_v);

            static_assert(((mask_v)&T_CAPACITY) == 0, "Queue size must be power of 2");
            static_assert((1 << pow_v) == T_CAPACITY);

        public:

            mpmc_bounded(const mpmc_bounded&) = delete;
            mpmc_bounded& operator=(const mpmc_bounded&) = delete;

            mpmc_bounded(void)
            {
                static_assert(std::is_nothrow_copy_assignable<T>::value || std::is_nothrow_move_assignable<T>::value, "T must be nothrow copy or move assignable");
                static_assert(std::is_nothrow_destructible<T>::value, "T must be nothrow destructible");
                static_assert(sizeof(self) % xcontainer::getCacheLineSize() == 0, "mpmc_bounded_v2<T> size must be a multiple of cache line size to prevent false sharing between adjacent queues");
                static_assert(sizeof(slot) % xcontainer::getCacheLineSize() == 0, "Slot size must be a multiple of cache line size to prevent false sharing between adjacent slots");

                // head and tail must be a cache line apart to prevent false sharing
                assert(reinterpret_cast<std::size_t>(&m_Tail) - reinterpret_cast<std::size_t>(&m_Head) >= static_cast<std::size_t>(getCacheLineSize()));

                for (std::size_t i = 0; i < capacity_v; ++i)
                    m_Slots[i].m_Turn.store(0, std::memory_order_relaxed);
            }

            template< typename... T_ARGS >
            bool push(T_ARGS&&... Args) noexcept
            {
                static_assert(std::is_nothrow_constructible<T, T_ARGS&&...>::value, "T must be nothrow constructible with T_ARGS&&...");
                auto Head = m_Head.load(std::memory_order_acquire);
                while (true)
                {
                    auto& Slot = m_Slots[idx(Head)];
                    if (turn(Head) * 2 == Slot.m_Turn.load(std::memory_order_acquire))
                    {
                        if (m_Head.compare_exchange_strong(Head, Head + 1))
                        {
                            Slot.construct(std::forward<T_ARGS>(Args)...);
                            Slot.m_Turn.store(turn(Head) * 2 + 1, std::memory_order_release);
                            return true;
                        }
                    }
                    else
                    {
                        auto const prevHead = Head;
                        Head = m_Head.load(std::memory_order_acquire);
                        if (Head == prevHead)
                            return false;
                    }
                }
                assert(false); //-V779
            }

            bool steal(T& Data) noexcept { return pop(Data); }
            bool pop(T& Value) noexcept
            {
                auto Tail = m_Tail.load(std::memory_order_acquire);
                while (true)
                {
                    auto& Slot = m_Slots[idx(Tail)];
                    if (turn(Tail) * 2 + 1 == Slot.m_Turn.load(std::memory_order_acquire))
                    {
                        if (m_Tail.compare_exchange_strong(Tail, Tail + 1))
                        {
                            Value = reinterpret_cast<T&&>(Slot.m_Storage);
                            Slot.destroy();
                            Slot.m_Turn.store(turn(Tail) * 2 + 2, std::memory_order_release);
                            return true;
                        }
                    }
                    else
                    {
                        auto const PrevTail = Tail;
                        Tail = m_Tail.load(std::memory_order_acquire);
                        if (Tail == PrevTail)
                            return false;
                    }
                }
            }

            static constexpr std::size_t capacity(void) noexcept
            {
                return capacity_v;
            }

            std::size_t size(void) const noexcept
            {
                const std::size_t head = m_Head.load(std::memory_order_acquire);
                const std::size_t tail = m_Tail.load(std::memory_order_relaxed);
                return (head - tail);
            }

        protected:

            static constexpr size_t idx(size_t i) noexcept { return i & mask_v; }
            static constexpr size_t turn(size_t i) noexcept { return i >> pow_v; }

            struct slot
            {
                ~slot() noexcept
                {
                    if (m_Turn & 1) destroy();
                }

                template< typename... T_ARGS >
                void construct(T_ARGS&&... Args) noexcept
                {
                    static_assert(std::is_nothrow_constructible<T, T_ARGS&&...>::value, "T must be nothrow constructible with Args&&...");
                    new (&m_Storage) T(std::forward<T_ARGS>(Args)...);
                }

                void destroy() noexcept
                {
                    static_assert(std::is_nothrow_destructible<T>::value, "T must be nothrow destructible");
                    reinterpret_cast<T*>(&m_Storage)->~T();
                }

                // Align to avoid false sharing between adjacent slots
                alignas(xcore::target::getCacheLineSize()) std::atomic<std::size_t>     m_Turn;
                alignas(T) std::array<std::byte, sizeof(T)>                             m_Storage;
            };

        protected:

            // Align to avoid false sharing
            alignas(xcore::target::getCacheLineSize()) std::atomic<size_t>  m_Head{ 0 };
            alignas(xcore::target::getCacheLineSize()) std::atomic<size_t>  m_Tail{ 0 };
            std::array< slot, T_CAPACITY >                                  m_Slots;
        };
    }

    using namespace v1;
}

#endif