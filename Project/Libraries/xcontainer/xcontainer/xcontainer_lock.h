#ifndef XCONTAINER_LOCK_H
#define XCONTAINER_LOCK_H
#pragma once

#include "dependencies/xerr/source/xerr.h"
#include <thread>

namespace xcontainer::lock
{
    //------------------------------------------------------------------------------
    // Description:
    //      A spin lock that is thread aware so recursion will work
    //      Example: xcore::lock::spin_reentrant
    //------------------------------------------------------------------------------
    struct spin_reentrant
    {
        template< typename T_CALLBACK = void(*)() >
        void lock(T_CALLBACK&& Callback = []()constexpr {}) noexcept
        {
            const auto Id = _getID();

            if (_TryLock(Id) == false)
            {
                //XCORE_PERF_ZONE_SCOPED_NC("spin_reentrant spin Lock is Spinning and waiting", tracy::Color::ColorType::Red);
                while (_TryLock(Id) == false) Callback();
            }
        }

        bool trylock(void) noexcept
        {
            const auto Id = _getID();
            return _TryLock(Id);
        }

        void unlock(void) noexcept
        {
            assert(m_EnteringCount > 0);
            --m_EnteringCount;
            if (m_EnteringCount == 0)
            {
                m_FullThreadID.store(std::thread::id(), std::memory_order_relaxed);
                m_Lock.store(false, std::memory_order_release);
            }
        }

        bool isLockedDebug(void) const noexcept { return !!m_Lock.load(std::memory_order_relaxed); }

        static const std::thread::id _getID(void) noexcept
        {
            return std::this_thread::get_id();
        }

        bool _TryLock(const std::thread::id Id) noexcept
        {
            const auto curID = m_FullThreadID.load(std::memory_order_relaxed);

            // Are we dealing with the same thread?
            if (curID == Id)
            {
                assert(Id != std::thread::id{});
                assert(m_EnteringCount > 0);
                m_EnteringCount++;
                return true;
            }

            // try to lock it officially
            if (curID == std::thread::id() && !m_Lock.exchange(true, std::memory_order_acquire))
            {
                assert(Id != std::thread::id{});
                assert(m_FullThreadID == std::thread::id{});
                assert(m_EnteringCount == 0);
                m_FullThreadID.store(Id, std::memory_order_relaxed);
                m_EnteringCount++;
                return true;
            }

            return false;
        }

        std::atomic<std::thread::id>    m_FullThreadID{};
        std::int16_t                    m_EnteringCount{ 0 };
        std::atomic<bool>               m_Lock{ 0 };
    };

    //------------------------------------------------------------------------------
    // Description:
    //      A spin lock that is NOT thread aware so recursion won't work
    //      Example: xcore::lock::spin
    //------------------------------------------------------------------------------
    struct spin
    {
        template< typename T_CALLBACK = void(*)() >
        void lock(T_CALLBACK&& Callback = []()constexpr {}) noexcept
        {
            if (m_Lock.exchange(true, std::memory_order_acquire))
            {
                // XCORE_PERF_ZONE_SCOPED_NC("Lock is Spinning and waiting", tracy::Color::ColorType::Red);
                const auto Id = _getID();
                while (_TryLock(Id) == false) Callback();
            }
        }

        bool trylock(void) noexcept
        {
            const auto Id = _getID();
            return _TryLock(Id);
        }

        bool isLockedDebug(void) const noexcept { return !!m_Lock.load(std::memory_order_relaxed); }

        void unlock(void) noexcept
        {
            //XCORE_CMD_DEBUG(m_Debug_FullThreadID.store(std::thread::id{}, std::memory_order_relaxed));
            m_Lock.store(false, std::memory_order_release);
        }

#if _DEBUG
        static const std::thread::id   _getID(void) noexcept { return std::this_thread::get_id(); }
#else
        static constexpr const int     _getID(void) noexcept { return 0; }
#endif

#if _DEBUG
        bool _TryLock(const std::thread::id Id) noexcept
#else
        xforceinline bool _TryLock(int) noexcept
#endif
        {
            if (!m_Lock.load(std::memory_order_relaxed)
                && !m_Lock.exchange(true, std::memory_order_acquire))
            {
              //  XCORE_CMD_DEBUG(m_Debug_FullThreadID.store(Id, std::memory_order_relaxed));
                return true;
            }
#if _DEBUG
            else
            {
                //const std::thread::id Id = std::this_thread::get_id();
                //x_assert( Id == std::this_thread::get_id() );

                // if this assert hits you may want to change to the reentrance version
                // since you did try to relock again with the same thread.
                assert(m_Debug_FullThreadID.load(std::memory_order_relaxed) != Id);
            }
#endif
            return false;
        }

#if _DEBUG
        std::atomic<std::thread::id>    m_Debug_FullThreadID{};
#endif
        std::atomic<bool>               m_Lock{ 0 };
    };

    //------------------------------------------------------------------------------------------
    // semaphore lock non-reentrant (Gives priority to writers)
    //------------------------------------------------------------------------------------------
    struct semaphore
    {
        struct thelock
        {
            std::uint16_t           m_nLocks : 10             // max(1024) number of locks currently in action. Readers can lock many times, writers only ones
                , m_nWaitingWritters : 5    // max(32)   number of waiting to lock writers
                , m_isLockWriter : 1;       // What type of lock is in action? Reading or writing?
        };

        mutable std::atomic<thelock> m_Lock{ thelock{0u,0u,0u} };

        inline bool isLockedDebug(void) const noexcept { return m_Lock.load(std::memory_order_relaxed).m_nLocks; }

        // Writer lock
        template< typename T_CALLBACK = void(*)() >
        inline
            void lock(T_CALLBACK&& Callback = []()constexpr {}) noexcept
        {
            auto p = m_Lock.load(std::memory_order_relaxed);
            do
            {
                assert(p.m_nWaitingWritters < ((1u << 5) - 2u));
                if (p.m_nLocks)
                {
                    if (m_Lock.compare_exchange_weak(p, thelock{ p.m_nLocks, p.m_nWaitingWritters + 1u, p.m_isLockWriter }))
                    {
                  //      XCORE_PERF_ZONE_SCOPED_NC("Waiting to access for writing", tracy::Color::ColorType::Red);
                        do
                        {
                            if (p.m_nLocks)
                            {
                                // Spin...
                                Callback();
                                p = m_Lock.load(std::memory_order_relaxed);
                            }
                            else if (m_Lock.compare_exchange_weak(p, thelock{ p.m_nLocks + 1u, p.m_nWaitingWritters, 1u })) goto LOCKED;

                        } while (true);
                    }
                }
                else if (m_Lock.compare_exchange_weak(p, thelock{ 1u, p.m_nWaitingWritters + 1u, 1u })) goto LOCKED;

            } while (true);

        LOCKED:;
        }

        // Writer unlock
        inline void unlock(void) noexcept
        {
            auto p = m_Lock.load(std::memory_order_relaxed);
            do
            {
                assert(p.m_isLockWriter);
                assert(p.m_nWaitingWritters > 0);
                assert(p.m_nLocks == 1u);
            } while (false == m_Lock.compare_exchange_weak(p, thelock{ 0u, p.m_nWaitingWritters - 1u, 0u }));
        }

        // Reader lock
        template< typename T_CALLBACK = void(*)() >
        inline void lock(T_CALLBACK&& Callback = []()constexpr {}) const noexcept
        {
            auto p = m_Lock.load(std::memory_order_relaxed);
            do
            {
                if (p.m_nWaitingWritters)
                {
                //    XCORE_PERF_ZONE_SCOPED_NC("Waiting to access for reading", tracy::Color::ColorType::Red)
                        do
                        {
                            Callback();
                            // Spin...
                            p = m_Lock.load(std::memory_order_relaxed);
                        } while (p.m_nWaitingWritters);
                }
                else if (m_Lock.compare_exchange_weak(p, thelock{ p.m_nLocks + 1u, 0u, 0u })) break;

            } while (true);
        }

        /*
        // Transition From Reader Lock to Writer Lock. Note you need to unlock like a writer now!
        inline void TransitionFromReadToWriteLock( void ) const noexcept
        {
            auto p = m_Lock.load( std::memory_order_relaxed );

            // We must have a lock already!
            assert( p.m_isLockWriter == false );
            assert( p.m_nLocks >= 1 );

            // Let the lock know that we are trying to transition
            while( false == m_Lock.compare_exchange_weak( p, thelock{ p.m_nLocks, p.m_nWaitingWritters+1u, p.m_nLocks == 1 ? true : false } ) );

            p = m_Lock.load( std::memory_order_relaxed );
            if( p.m_isLockWriter ) return;

            while( p.m_nLocks > 1 )
            {
                // Spin and wait!
                p = m_Lock.load( std::memory_order_relaxed );
            }

            m_Lock.store( thelock{ 1u, p.m_nWaitingWritters-1u, true }, std::memory_order_release );
        }
        */

        // Reader unlock
        inline void unlock(void) const noexcept
        {
            auto p = m_Lock.load(std::memory_order_relaxed);
            do
            {
                assert(p.m_isLockWriter == false);
                assert(p.m_nLocks > 0u);
            } while (false == m_Lock.compare_exchange_weak(p, thelock{ p.m_nLocks - 1u, p.m_nWaitingWritters, 0u }));
        }

        template< typename T_CALLBACK = void(*)() >
        inline xerr ExclusiveWriteLock(T_CALLBACK&& Callback = []()constexpr {}) noexcept
        {
            auto p = m_Lock.load(std::memory_order_relaxed);
            do
            {
                if (p.m_nLocks || p.m_nWaitingWritters) return xerr::create_f<xerr::default_states, "Fail to exclusively lock for writing. There are other locks pending in xcore::semaphore">();
                Callback();
            } while (false == m_Lock.compare_exchange_weak(p, thelock{ 1u, 1u, 1u }));

            return {};
        }

        inline xerr ExclusiveReadLock(void) const noexcept
        {
            auto p = m_Lock.load(std::memory_order_relaxed);
            do
            {
                if (p.m_nWaitingWritters) return xerr::create_f<xerr::default_states, "Fail to exclusively lock for reading. There is a writer pending using the xcore::semaphore">();
            } while (false == m_Lock.compare_exchange_weak(p, thelock{ p.m_nLocks + 1u, 0u, 0u }));

            return {};
        }
    };

    //------------------------------------------------------------------------------------------
    // semaphore with lock reentrant for writers, readers were already supported
    //------------------------------------------------------------------------------------------
    struct semaphore_reentrant
    {
        inline bool isLockedDebug(void) const noexcept { return m_Semaphore.isLockedDebug(); }

        template< typename T_CALLBACK = void(*)() >
        void lock(T_CALLBACK&& Callback = []()constexpr {}) noexcept
        {
            const auto ID = _getID();
            if (m_FullThreadID.load(std::memory_order_relaxed) != ID)
            {
                m_Semaphore.lock(std::forward<T_CALLBACK&&>(Callback));
                m_FullThreadID.store(ID, std::memory_order_relaxed);
            }

            m_EnteringCount++;
        }

        void unlock(void) noexcept
        {
            assert(m_EnteringCount > 0);
            assert(_getID() == m_FullThreadID.load(std::memory_order_relaxed));

            m_EnteringCount--;
            if (!m_EnteringCount)
            {
                m_FullThreadID.store(std::thread::id{}, std::memory_order_relaxed);
                m_Semaphore.unlock();
            }
        }

        void unlock(void) const noexcept
        {
            m_Semaphore.unlock();
        }

        template< typename T_CALLBACK = void(*)() >
        void lock(T_CALLBACK&& Callback = []()constexpr {}) const noexcept
        {
            m_Semaphore.lock(std::forward<T_CALLBACK&&>(Callback));
        }

        static const std::thread::id _getID(void) noexcept
        {
            return std::this_thread::get_id();
        }

        std::atomic<std::thread::id>    m_FullThreadID{};
        std::int16_t                    m_EnteringCount{ 0 };
        semaphore                       m_Semaphore{};
    };

    //------------------------------------------------------------------------------------------
    // scope the lock
    //------------------------------------------------------------------------------------------
    template< typename T >
    class scope
    {
    public:

        // Callback used when it is waiting to lock, one call per try
        template< typename T_CALLBACK = void(*)() >
        constexpr scope(T& Lock, T_CALLBACK&& Callback = []()constexpr {}) noexcept : m_Lock(Lock)
        {
            m_Lock.lock(Callback);  // lock the mutex in the constructor
        }

        inline ~scope(void)
        {
            m_Lock.unlock(); // unlock the mutex in the constructor
        }

    protected:
        T& m_Lock;
    };

    //------------------------------------------------------------------------------------------
    // locked object
    //------------------------------------------------------------------------------------------
    template< typename T_CLASS, typename T_LOCK >
    class object : public T_LOCK
    {
    public:

        template<typename ...T_ARG>
        constexpr                object(T_ARG&&...Args)       noexcept : m_Class{ std::forward<T_ARG>(Args)... } {}
        inline          T_CLASS& get(void)                    noexcept { assert(T_LOCK::isLockedDebug()); return m_Class; }
        constexpr const T_CLASS& get(void)             const  noexcept { assert(T_LOCK::isLockedDebug()); return m_Class; }

    protected:

        T_CLASS             m_Class;
    };
}

#endif