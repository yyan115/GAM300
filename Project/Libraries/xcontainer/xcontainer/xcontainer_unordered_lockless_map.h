#ifndef XCONTAINER_UNORDERED_LOCKLESS_MAP_H
#define XCONTAINER_UNORDERED_LOCKLESS_MAP_H
#pragma once

namespace xcontainer
{
    namespace details
    {
        template<typename T>
        struct scope_guard
        {
            scope_guard(T&& Callback) : m_ScopeCall(std::move(Callback)) {}
            ~scope_guard()
            {
                m_ScopeCall();
            }
            T m_ScopeCall;
        };
    }

    template< typename T_KEY, typename T_VALUE >
    struct unordered_lockless_map
    {
        constexpr inline static std::size_t    hash_key_ration_to_data_v = 8;
        constexpr inline static std::int64_t   grow_threshold_v          = 48;

        struct atomic_key
        {
            union
            {
                std::uint32_t       m_Value;
                struct
                {
                    std::uint16_t   m_Used              :1        // If this node currently it is being used
                                ,   m_BeenUsedBefore    :1        // Tells if this entry has been used before
                                ,   m_WriteLock         :1        // There is a write been done
                                ,   m_WritePendingCount :12;      // Tells how many entries writing to write are waiting in the queue... (order out of the queue is random)
                    std::uint16_t   m_ReadLockCount;              // How many readers are currently reading this node 
                };
            };
            std::uint32_t   m_LowHash;      // The hash key part 1
        };
        static_assert(sizeof(atomic_key) == sizeof(std::uint64_t));

        struct key_entry
        {
            consteval               key_entry   (void)                              noexcept {}
            constexpr               key_entry   (const key_entry& V)                noexcept : m_Value{V.m_Value},  m_Value2{ V.m_Value2 }{}
            constexpr key_entry&    operator =  (const key_entry& V)                noexcept { m_Value = V.m_Value; m_Value2 = V.m_Value2; return *this; }

            union
            {
                atomic_key                  m_State;
                std::atomic<atomic_key>     m_AtomicState;
                std::uint64_t               m_Value;        // Simplify the data to a u64
            };

            union
            {
                std::uint64_t               m_Value2;       // Simplify the data to a u64
                struct
                {
                    std::uint32_t           m_HighHash;     // The hash key part 2
                    std::uint32_t           m_Index;        // The index of the data node
                };
            };

            std::uint32_t                   m_SameThreatCount;
            std::atomic<std::thread::id>    m_WriteThreadID;   // Allows for reentrace write lock...
        };

        using data_pair = std::pair<T_KEY, T_VALUE>;
        union data
        {
            ~data() = delete;
            std::uint32_t   m_NextEmpty;
            data_pair       m_Pair;
            struct 
            {
                T_KEY       m_Key;
                T_VALUE     m_Value;
            };
        };

        struct empty_list
        {
            std::uint64_t       m_Value;
            struct
            {
                std::uint32_t   m_Next;
                std::uint32_t   m_Count;
            };
        };

        //================================================================================================

        void GrowIfNecessary() noexcept
        {
            if (static_cast<std::int64_t>(m_Count.load(std::memory_order_relaxed)) > ((m_MaxDataCount - grow_threshold_v) ))
            {
                GlobalLockForWrite();
                if (static_cast<std::int64_t>(m_Count.load(std::memory_order_relaxed)) > ((m_MaxDataCount - grow_threshold_v)))
                    resize(m_MaxDataCount + m_MaxDataCount/2, true);
                GlobalUnlockWrite();
            }
        }

        //================================================================================================

        // Allocates the actual data as well as construct it in a thread safe manner
        template< typename T >
        std::uint32_t AllocData(T_KEY Key, T&& Callback) noexcept
        {
            auto Local = m_EmptyList.load(std::memory_order_relaxed);
            do
            {
                assert(Local.m_Next < m_MaxDataCount);

                auto& Entry = m_pData[Local.m_Next];
                empty_list NextEmpty;
                NextEmpty.m_Next = Entry.m_NextEmpty;
                NextEmpty.m_Count = Local.m_Count + 1;

                if (m_EmptyList.compare_exchange_weak(Local, NextEmpty, std::memory_order_release, std::memory_order_relaxed))
                {
                    auto& Data = m_pData[Local.m_Next];

                    // Construct the key + data
                    std::construct_at(&Data.m_Pair);

                    // Set the key of the entry
                    Data.m_Key = Key;

                    // mark the entry in the bit array as well
                    m_pBitArray[Local.m_Next / 64].fetch_or(1ULL << (Local.m_Next % 64), std::memory_order_release);

                    // Increment the count
                    m_Count.fetch_add(1, std::memory_order_release);

                    Callback(Data.m_Value);

                    break;
                }
            } while (true);

            return Local.m_Next;
        }

        //================================================================================================

        // Free the data and call the callback before the actual deletion
        template< typename T >
        void FreeData(std::uint32_t Index, T&& Callback) noexcept
        {
            // One less entry...
            m_Count.fetch_sub(1, std::memory_order_release);

            // Remove from the bit array
            m_pBitArray[Index / 64].fetch_and(~(1 << (Index % 64)), std::memory_order_release);

            // Callback before actual deletion
            Callback(m_pData[Index].m_Value);

            // Destroy the data
            std::destroy_at(&m_pData[Index].m_Pair);

            auto& EmptyNode = reinterpret_cast<empty_list&>(m_pData[Index]);

            // Insert node into the empty list
            auto Local = m_EmptyList.load(std::memory_order_relaxed);
            do
            {
                EmptyNode.m_Value = Local.m_Value;
                auto NewHeadEmpty = Local;

                NewHeadEmpty.m_Next = Index;
                NewHeadEmpty.m_Count += 1;

                if (m_EmptyList.compare_exchange_weak(Local, NewHeadEmpty, std::memory_order_release, std::memory_order_relaxed))
                    break;

            } while (true);
        }

        //================================================================================================

        int size() const noexcept
        {
            return m_Count.load(std::memory_order_acquire);
        }

        //================================================================================================

        bool empty() const noexcept
        {
            return size() == 0;
        }

        //================================================================================================

        ~unordered_lockless_map()
        {
            if (m_pData)
            {
                clear();
                _aligned_free(m_pData);
                _aligned_free(m_pKeys);
                _aligned_free(m_pBitArray);
            }
        }

        //================================================================================================

        template< typename T >
        void Insert(const T_KEY& Key, T&& Callback) noexcept
        {
            GrowIfNecessary();
            GlobalLockForRead();
            details::scope_guard Unlock([&]() { GlobalUnlockRead(); });

            // Find the key
            const std::size_t   FullHash = std::hash<T_KEY>{}(Key);
            auto                Walk     = FullHash % m_MaxDataCount;
            auto                Local    = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
            do
            {
                auto& Node = m_pKeys[Walk];

                if( Local.m_Used == false )
                {
                    // Assume this key has been discarded and we can use it
                    assert(Local.m_ReadLockCount     == 0);
                    assert(Local.m_WritePendingCount == 0);
                    assert(Local.m_WriteLock == 0);

                    // We can define our new atomic key base on this fact
                    auto NewState = Local;
                    NewState.m_LowHash          = static_cast<std::uint32_t>(FullHash);
                    NewState.m_Used             = true;
                    NewState.m_BeenUsedBefore   = true;
                    NewState.m_WriteLock        = true;

                    // Let us see if we are lucky and we can lock it
                    if (Node.m_AtomicState.compare_exchange_weak(Local, NewState, std::memory_order_release, std::memory_order_relaxed))
                    {
                        Node.m_WriteThreadID.store(std::this_thread::get_id(), std::memory_order_release);
                        Node.m_HighHash = static_cast<std::uint32_t>(FullHash >> 32);
                        Node.m_Index    = AllocData(Key, std::forward<T&&>(Callback));

                        ReleaseWriteLock(Node, Node.m_AtomicState.load(std::memory_order_relaxed));

                        // Done
                        break;
                    }
                }
                else
                {
                    Walk = (Walk + 1) % m_MaxDataCount;
                    Local = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
                }

            } while (true);
        }

        //================================================================================================

        int LockWriteWaitInQueueIfWeHaveTo( key_entry& Entry, atomic_key& Local ) noexcept
        {
            do
            {
                if (Local.m_Used == false) return 2;

                // Assume that we have a reason to be busy..
                if (Local.m_WriteLock || Local.m_ReadLockCount)
                {
                    const auto ThreadID = std::this_thread::get_id();

                    // Check the re-entrace case...
                    if (Entry.m_WriteThreadID.load(std::memory_order_relaxed) == ThreadID)
                    {
                        // We already have the lock
                        return 0;
                    }

                    // Ready to jump into the queue
                    auto NewValue = Local;
                    NewValue.m_WritePendingCount += 1;
                    if (Entry.m_AtomicState.compare_exchange_weak(Local, NewValue, std::memory_order_release, std::memory_order_relaxed))
                    {
                        // Now we are inline now we have to wait for our turn
                        do
                        {
                            Local = Entry.m_AtomicState.load(std::memory_order_relaxed);
                            if (Local.m_Used == false) return 2;
                            if (Local.m_WriteLock == 0 && Local.m_ReadLockCount == 0)
                            {
                                // Try to get our turn!
                                NewValue = Local;
                                NewValue.m_WritePendingCount -= 1;
                                NewValue.m_WriteLock = 1;
                                if (Entry.m_AtomicState.compare_exchange_weak(Local, NewValue, std::memory_order_release, std::memory_order_relaxed))
                                {
                                    Local = NewValue;
                                    Entry.m_WriteThreadID.store(ThreadID, std::memory_order_release);
                                    return true;
                                }
                            }
                            else
                            {
                                // Waiting for our turn... (spinning...)
                            }

                        } while (true);
                    }
                }
                else
                {
                    // Seems no one is doing anything with this node... is our turn to try to get in.
                    atomic_key NewValue = Local;
                    NewValue.m_WriteLock = 1;
                    if (Entry.m_AtomicState.compare_exchange_weak(Local, NewValue, std::memory_order_release, std::memory_order_relaxed))
                    {
                        Entry.m_WriteThreadID.store(std::this_thread::get_id(), std::memory_order_release);
                        return 1;
                    }
                        
                }
            } while(true);
        }

        //================================================================================================

        int LockReadWaitInQueueIfWeHaveTo(key_entry& Entry, atomic_key& Local) noexcept
        {
            do
            {
                if (Local.m_Used == false) return 2;
                if (Local.m_WriteLock || Local.m_WritePendingCount)
                {
                    if (Local.m_WriteLock)
                    {
                        const auto ThreadID = std::this_thread::get_id();

                        // Check the re-entrace case...
                        if (Entry.m_WriteThreadID.load(std::memory_order_relaxed) == ThreadID)
                        {
                            return 0;
                        }
                    }

                    // Nothing to do but to wait... Write locks are high priority
                    Local = Entry.m_AtomicState.load(std::memory_order_relaxed);
                }
                else
                {
                    // Ready to jump into the queue
                    atomic_key NewValue = Local;
                    NewValue.m_ReadLockCount += 1;
                    if (Entry.m_AtomicState.compare_exchange_weak(Local, NewValue, std::memory_order_release, std::memory_order_relaxed))
                    {
                        // Now we have our read lock
                        return 1;
                    }
                }
            } while (true);
        }

        //================================================================================================

        void ReleaseWriteWithDeleteLock(key_entry& Entry, atomic_key Local) noexcept
        {
            // Remove our current working thread id
            Entry.m_WriteThreadID.store({}, std::memory_order_release);

            do
            {
                auto NewValue = Local;
                NewValue.m_WriteLock            = 0;
                NewValue.m_WritePendingCount    = 0;
                NewValue.m_Used                 = false;
                NewValue.m_BeenUsedBefore       = true;
                NewValue.m_LowHash              = 0;
                NewValue.m_ReadLockCount        = 0;
                if (Entry.m_AtomicState.compare_exchange_weak(Local, NewValue, std::memory_order_release, std::memory_order_relaxed))
                    break;
            } while (true);
        }
        

        //================================================================================================

        void ReleaseWriteLock( key_entry& Entry, atomic_key Local ) noexcept
        {
            // Remove our current working thread id
            Entry.m_WriteThreadID.store({}, std::memory_order_release);

            do
            {
                auto NewValue = Local;
                NewValue.m_WriteLock = 0;
                if (Entry.m_AtomicState.compare_exchange_weak(Local, NewValue, std::memory_order_release, std::memory_order_relaxed))
                    break;
            } while (true);
        }

        //================================================================================================

        void ReleaseReadLock(key_entry& Entry, atomic_key Local) noexcept
        {
            do
            {
                auto NewValue = Local;
                NewValue.m_ReadLockCount -= 1;
                if (Entry.m_AtomicState.compare_exchange_weak(Local, NewValue, std::memory_order_release, std::memory_order_relaxed))
                    break;
            } while (true);
        }

        //================================================================================================

        template< typename T_CREATE_CALLBACK, typename T_WRITE_CALLBACK >
        requires details::is_first_arg_is_reference<T_WRITE_CALLBACK>
              && details::is_first_arg_is_reference<T_CREATE_CALLBACK>
        bool FindAsWriteOrCreate(T_KEY Key, T_CREATE_CALLBACK&& CreateCallback, T_WRITE_CALLBACK&& WriteCallBack) noexcept
        {
            GrowIfNecessary();
            GlobalLockForRead();
            details::scope_guard Unlock([&]() { GlobalUnlockRead(); });

            const std::size_t FullHash  = std::hash<T_KEY>{}(Key);
            std::uint32_t     Walk      = static_cast<std::uint32_t>(FullHash % m_MaxDataCount);
            auto              Local     = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
            std::uint32_t     FirstFree = ~0u;

            do
            {
                auto& Node = m_pKeys[Walk];

                if ( Local.m_Used )
                {
                    assert(Local.m_BeenUsedBefore);

                    // Does this guy looks like we may have found it?
                    if( Local.m_LowHash == static_cast<std::uint32_t>(FullHash) )
                    {
                        const int UnLock = LockWriteWaitInQueueIfWeHaveTo(Node, Local);
                        if (UnLock == 2) continue;

                        // We have the lock. let us see if we are the lucky one
                        // First we check the reminder of the hash key since if we fail we
                        // don't commit a cache miss. If we get that then we are pretty confident and
                        // we can check with the actual key.
                        if ( ((FullHash>>32) == Node.m_HighHash) && Key == m_pData[Node.m_Index].m_Key )
                        {
                            // Let the user do its thing...
                            WriteCallBack(m_pData[Node.m_Index].m_Value);

                            // If we had a temporary lock we need to release it
                            if (FirstFree != ~0u)
                            {
                                ReleaseWriteLock(m_pKeys[FirstFree], m_pKeys[FirstFree].m_AtomicState.load(std::memory_order_relaxed));
                            }

                            // Release All locks and return
                            if (UnLock) ReleaseWriteLock(Node, Local);
                            return true;
                        }

                        // Close but no cigar... release the lock and keep searching
                        if (UnLock) ReleaseWriteLock(Node, Local);
                    }

                    // Keeps searching
                    Walk = (Walk + 1) % m_MaxDataCount;
                    Local = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
                }
                else 
                {
                    if (FirstFree == ~0u)
                    {
                        // We lock this node since we will use it for the 
                        auto NewValue = Local;
                        NewValue.m_Used             = true;
                        NewValue.m_WriteLock        = true;
                        NewValue.m_BeenUsedBefore   = true; 
                        NewValue.m_LowHash          = static_cast<std::uint32_t>(FullHash);

                        if (Node.m_AtomicState.compare_exchange_weak(Local, NewValue, std::memory_order_release, std::memory_order_relaxed))
                        {
                            // We have officially reserved our node..
                            FirstFree = Walk;
                        }
                        else
                        {
                            // Something has changed so let us think about it again...
                            continue;
                        }
                    }

                    // We have reach the end of the search... So we should try to create ourself
                    if( Local.m_BeenUsedBefore == false )
                    {
                        auto& NewNode = m_pKeys[FirstFree];

                        NewNode.m_HighHash = static_cast<std::uint32_t>(FullHash >> 32);
                        NewNode.m_Index    = AllocData(Key, std::forward<T_CREATE_CALLBACK&&>(CreateCallback));

                        // Let the user also handle any write operation
                        WriteCallBack(m_pData[NewNode.m_Index].m_Value);

                        // Make sure we know the upto date value
                        // Release All locks and return
                        ReleaseWriteLock(NewNode, Local);
                        return false;
                    }
                    else
                    {
                        Walk  = (Walk + 1) % m_MaxDataCount;
                        Local = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
                    }
                }

            } while (true);
        }

        //================================================================================================
        
        template< typename T_CREATE_CALLBACK, typename T_READ_CALLBACK >
        requires details::is_first_arg_const<T_READ_CALLBACK>
              && details::is_first_arg_is_reference<T_READ_CALLBACK>
              && details::is_first_arg_is_reference<T_CREATE_CALLBACK>
        bool FindAsReadOnlyOrCreate(T_KEY Key, T_CREATE_CALLBACK&& CreateCallback, T_READ_CALLBACK&& ReadCallBack) noexcept
        {
            static_assert(details::is_first_arg_const<T_READ_CALLBACK>, "The first argument of ReadCallback must be const");

            GrowIfNecessary();
            GlobalLockForRead();
            details::scope_guard Unlock([&]() { GlobalUnlockRead(); });

            const std::size_t FullHash  = std::hash<T_KEY>{}(Key);
            std::uint32_t     Walk      = static_cast<std::uint32_t>(FullHash % m_MaxDataCount);
            atomic_key        Local     = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
            std::uint32_t     FirstFree = ~0u;

            do
            {
                auto& Node = m_pKeys[Walk];

                if (Local.m_Used)
                {
                    assert(Local.m_BeenUsedBefore);

                    // Does this guy looks like we may have found it?
                    if (Local.m_LowHash == static_cast<std::uint32_t>(FullHash))
                    {
                        const int UnLock = LockReadWaitInQueueIfWeHaveTo(Node, Local);
                        if (UnLock == 2) continue;

                        // We have the lock. let us see if we are the lucky one
                        // First we check the reminder of the hash key since if we fail we
                        // don't commit a cache miss. If we get that then we are pretty confident and
                        // we can check with the actual key.
                        if (((FullHash >> 32) == Node.m_HighHash) && Key == m_pData[Node.m_Index].m_Key)
                        {
                            // If we had a temporary lock we need to release it
                            if (FirstFree != ~0u)
                            {
                                ReleaseWriteLock(m_pKeys[FirstFree], m_pKeys[FirstFree].m_AtomicState.load(std::memory_order_relaxed));
                            }

                            // Let the user do its thing...
                            ReadCallBack( std::as_const(m_pData[Node.m_Index].m_Value) );

                            // Release All locks and return
                            if (UnLock) ReleaseReadLock(Node, Local);
                            return true;
                        }

                        // Close but no cigar... release the lock and keep searching
                        if (UnLock) ReleaseReadLock(Node, Local);
                    }

                    // Keeps searching
                    Walk = (Walk + 1) % m_MaxDataCount;
                    Local = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
                }
                else
                {
                    if (FirstFree == ~0u)
                    {
                        // We lock this node since we will use it for the 
                        auto NewValue = Local;
                        NewValue.m_Used             = true;
                        NewValue.m_WriteLock        = true;
                        NewValue.m_BeenUsedBefore   = true;
                        NewValue.m_LowHash          = static_cast<std::uint32_t>(FullHash);

                        if (Node.m_AtomicState.compare_exchange_weak(Local, NewValue, std::memory_order_release, std::memory_order_relaxed))
                        {
                            // We have officially reserved our node..
                            FirstFree = Walk;
                        }
                        else
                        {
                            // Something has changed so let us think about it again...
                            continue;
                        }
                    }

                    // We have reach the end of the search... So we should try to create ourself
                    if (Local.m_BeenUsedBefore == false)
                    {
                        auto& NewNode = m_pKeys[FirstFree];
                        NewNode.m_WriteThreadID.store(std::this_thread::get_id(), std::memory_order_release);
                        NewNode.m_HighHash = static_cast<std::uint32_t>(FullHash >> 32);
                        NewNode.m_Index    = AllocData(Key, std::forward<T_CREATE_CALLBACK&&>(CreateCallback));

                        // Let the user also handle any write operation
                        ReadCallBack(std::as_const(m_pData[NewNode.m_Index].m_Value));

                        // Make sure we know the upto date value
                        // Release All locks and return
                        ReleaseWriteLock(NewNode, Local);
                        return false;
                    }
                    else
                    {
                        Walk = (Walk + 1) % m_MaxDataCount;
                        Local = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
                    }
                }

            } while (true);
        }

        //================================================================================================

        template< typename T_CREATE_CALLBACK, typename T_READ_CALLBACK >
        requires details::is_first_arg_const<T_READ_CALLBACK>
              && details::is_first_arg_is_reference<T_READ_CALLBACK>
              && details::is_first_arg_is_reference<T_CREATE_CALLBACK>
        constexpr bool FindAsReadOnlyOrCreate(T_KEY Key, T_CREATE_CALLBACK&& CreateCallback, T_READ_CALLBACK&& ReadCallBack) const noexcept
        {
            static_assert(details::is_first_arg_const<T_READ_CALLBACK>, "The first argument of ReadCallback must be const");
            return const_cast<unordered_lockless_map*>(this)->FindAsReadOnlyOrCreate(Key, std::forward<T_CREATE_CALLBACK&&>(CreateCallback), std::forward<T_READ_CALLBACK&&>(ReadCallBack));
        }

        //================================================================================================
        // The write call back should return true if it wants to delete the entry
        // NOTE: WARNING: This function is untested...

        template<typename T_WRITE_CALLBACK>
          requires details::is_first_arg_is_reference<T_WRITE_CALLBACK>
        bool FindAsWriteAndOrDelete(T_KEY Key, T_WRITE_CALLBACK&& WriteCallback) noexcept
        {
            if (m_Count.load(std::memory_order_relaxed) == 0) return false;
            GlobalLockForRead();
            details::scope_guard Unlock([&]() { GlobalUnlockRead(); });

            const std::size_t   FullHash    = std::hash<T_KEY>{}(Key);
            auto                Walk        = FullHash % m_MaxDataCount;
            atomic_key          Local       = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);

            do
            {
                key_entry& Node = m_pKeys[Walk];

                if (Local.m_Used)
                {
                    assert(Local.m_BeenUsedBefore);

                    // Check if this entry matches the key
                    if (Local.m_LowHash == static_cast<std::uint32_t>(FullHash))
                    {
                        const int Unlock = LockWriteWaitInQueueIfWeHaveTo(Node, Local);
                        if (Unlock == 2) continue; // Key was deleted, continue searching

                        // Verify the full hash and key
                        if (((FullHash >> 32) == Node.m_HighHash) && Key == m_pData[Node.m_Index].m_Key)
                        {
                            // Execute the callback, which returns true to delete, false to keep
                            const bool shouldDelete = WriteCallback(m_pData[Node.m_Index].m_Value);

                            if (shouldDelete)
                            {
                                // Delete the entry: free data and reset key state
                                // Empty callback since cleanup should be pre-handled
                                FreeData(Node.m_Index, [](T_VALUE&) {}); 
                                if (Unlock) ReleaseWriteWithDeleteLock(Node, Local);
                            }
                            else
                            {
                                // Keep the entry, just release the write lock
                                if (Unlock) ReleaseWriteLock(Node, Local);
                            }
                            return true;
                        }

                        // No match, release lock and continue
                        if (Unlock) ReleaseWriteLock(Node, Local);
                    }

                    // Move to the next slot
                    Walk = (Walk + 1) % m_MaxDataCount;
                    Local = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
                }
                else
                {
                    // Reached an unused slot
                    if (Local.m_BeenUsedBefore)
                    {
                        Walk = (Walk + 1) % m_MaxDataCount;
                        Local = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
                    }
                    else
                    {
                        return false; // Key not found, no deletion possible
                    }
                }
            } while (true);
        }
        //================================================================================================

        template< typename T_WRITE_CALLBACK >
        requires details::is_first_arg_is_reference<T_WRITE_CALLBACK>
        bool FindAsWrite(T_KEY Key, T_WRITE_CALLBACK&& WriteCallBack ) noexcept
        {
            if (m_Count.load(std::memory_order_relaxed) == 0) return false;
            GlobalLockForRead();
            details::scope_guard Unlock([&]() { GlobalUnlockRead(); });

            const std::size_t FullHash  = std::hash<T_KEY>{}(Key);
            auto              Walk      = FullHash % m_MaxDataCount;
            atomic_key        Local     = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);

            do
            {
                key_entry& Node = m_pKeys[Walk];

                if (Local.m_Used)
                {
                    assert(Local.m_BeenUsedBefore);

                    // Does this guy looks like we may have found it?
                    if (Local.m_LowHash == static_cast<std::uint32_t>(FullHash))
                    {
                        const int Unlock = LockWriteWaitInQueueIfWeHaveTo(Node, Local);
                        if (Unlock == 2) continue;

                        // We have the lock. let us see if we are the lucky one
                        // First we check the reminder of the hash key since if we fail we
                        // don't commit a cache miss. If we get that then we are pretty confident, and
                        // we can check with the actual key.
                        if (((FullHash >> 32) == Node.m_HighHash) && Key == m_pData[Node.m_Index].m_Key)
                        {
                            // Let the user do its thing...
                            WriteCallBack(m_pData[Node.m_Index].m_Value);

                            // Release All locks and return
                            if (Unlock) ReleaseWriteLock(Node, Local);
                            return true;
                        }

                        // Close but no cigar... release the lock and keep searching
                        if (Unlock) ReleaseWriteLock(Node, Local);
                    }

                    // Keeps searching
                    Walk = (Walk + 1) % m_MaxDataCount;
                    Local = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
                }
                else
                {
                    // We have reach the end of the search... So we should try to create ourselves
                    if (Local.m_BeenUsedBefore)
                    {
                        Walk = (Walk + 1) % m_MaxDataCount;
                        Local = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
                    }
                    else
                    {
                        return false;
                    }
                }

            } while (true);
        }

        //================================================================================================

        template< typename T_WRITE_CALLBACK >
        requires details::is_first_arg_is_reference<T_WRITE_CALLBACK>
        bool FindForDelete(T_KEY Key, T_WRITE_CALLBACK&& DeleteCallBack) noexcept
        {
            if (m_Count.load(std::memory_order_relaxed) == 0) return false;
            GlobalLockForRead();
            details::scope_guard Unlock([&]() { GlobalUnlockRead(); });

            const std::size_t FullHash = std::hash<T_KEY>{}(Key);
            auto              Walk     = FullHash % m_MaxDataCount;
            atomic_key        Local    = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);

            do
            {
                key_entry& Node = m_pKeys[Walk];

                if (Local.m_Used)
                {
                    assert(Local.m_BeenUsedBefore);

                    // Does this guy looks like we may have found it?
                    if (Local.m_LowHash == static_cast<std::uint32_t>(FullHash))
                    {
                        const int Unlock = LockWriteWaitInQueueIfWeHaveTo(Node, Local);
                        if (Unlock == 2) continue;

                        // We have the lock. let us see if we are the lucky one
                        // First we check the reminder of the hash key since if we fail we
                        // don't commit a cache miss. If we get that then we are pretty confident, and
                        // we can check with the actual key.
                        if (((FullHash >> 32) == Node.m_HighHash) && Key == m_pData[Node.m_Index].m_Key)
                        {
                            // Release back the memory and call the user function
                            FreeData(Node.m_Index, std::forward<T_WRITE_CALLBACK&&>(DeleteCallBack));

                            // Release All locks and return
                            if (Unlock) ReleaseWriteWithDeleteLock(Node, Local);
                            return true;
                        }

                        // Close but no cigar... release the lock and keep searching
                        if (Unlock) ReleaseWriteLock(Node, Local);
                    }

                    // Keeps searching
                    Walk = (Walk + 1) % m_MaxDataCount;
                    Local = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
                }
                else
                {
                    // We have reach the end of the search... So we should try to create ourselves
                    if (Local.m_BeenUsedBefore)
                    {
                        Walk = (Walk + 1) % m_MaxDataCount;
                        Local = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
                    }
                    else
                    {
                        return false;
                    }
                }

            } while (true);
        }

        //================================================================================================

        template< typename T_READ_CALLBACK >
        requires details::is_first_arg_const<T_READ_CALLBACK>
              && details::is_first_arg_is_reference<T_READ_CALLBACK>
        bool FindAsReadOnly(T_KEY Key, T_READ_CALLBACK&& ReadCallBack) noexcept
        {
            if (m_Count.load(std::memory_order_relaxed) == 0) return false;
            static_assert(details::is_first_arg_const<T_READ_CALLBACK>, "The first argument of ReadCallback must be const");
            GlobalLockForRead();
            details::scope_guard Unlock([&]() { GlobalUnlockRead(); });

            const std::size_t FullHash  = std::hash<T_KEY>{}(Key);
            auto              Walk      = FullHash % m_MaxDataCount;
            atomic_key        Local     = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);

            do
            {
                key_entry& Node = m_pKeys[Walk];

                if (Local.m_Used)
                {
                    assert(Local.m_BeenUsedBefore);

                    // Does this guy looks like we may have found it?
                    if (Local.m_LowHash == static_cast<std::uint32_t>(FullHash))
                    {
                        const int UnLock = LockReadWaitInQueueIfWeHaveTo(Node, Local);
                        if (UnLock == 2) continue;

                        // We have the lock. let us see if we are the lucky one
                        // First we check the reminder of the hash key since if we fail we
                        // don't commit a cache miss. If we get that then we are pretty confident and
                        // we can check with the actual key.
                        if (((FullHash >> 32) == Node.m_HighHash) && Key == m_pData[Node.m_Index].m_Key)
                        {
                            // Let the user do its thing...
                            ReadCallBack(std::as_const(m_pData[Node.m_Index].m_Value));

                            // Release All locks and return
                            if (UnLock) ReleaseReadLock(Node, Local);
                            return true;
                        }

                        // Close but no cigar... release the lock and keep searching
                        if (UnLock) ReleaseReadLock(Node, Local);
                    }

                    // Keeps searching
                    Walk = (Walk + 1) % m_MaxDataCount;
                    Local = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
                }
                else
                {
                    // We have reach the end of the search... So we should try to create ourselves
                    if (Local.m_BeenUsedBefore)
                    {
                        Walk = (Walk + 1) % m_MaxDataCount;
                        Local = m_pKeys[Walk].m_AtomicState.load(std::memory_order_relaxed);
                    }
                    else
                    {
                        return false;
                    }
                }

            } while (true);
        }

        //================================================================================================

        template< typename T_READ_CALLBACK >
        requires details::is_first_arg_const<T_READ_CALLBACK>
              && details::is_first_arg_is_reference<T_READ_CALLBACK>
        constexpr bool FindAsReadOnly(T_KEY Key, T_READ_CALLBACK&& ReadCallBack) const noexcept
        {
            static_assert(details::is_first_arg_const<T_READ_CALLBACK>, "The first argument of ReadCallback must be const");
            return const_cast<unordered_lockless_map*>(this)->FindAsReadOnly(Key, std::forward<T_READ_CALLBACK&&>(ReadCallBack));
        }

        //================================================================================================

        void GlobalUnlockWrite() noexcept
        {
            auto Local = m_GlobalLock.load(std::memory_order_relaxed);
            do
            {
                assert(Local.m_WriteLock == 1);
                auto NewState = Local;
                NewState.m_WriteLock = 0;
                if (m_GlobalLock.compare_exchange_weak(Local, NewState, std::memory_order_release, std::memory_order_relaxed))
                    break;

            } while (true);
        }

        //================================================================================================

        void GlobalUnlockRead() noexcept
        {
            auto Local = m_GlobalLock.load(std::memory_order_relaxed);
            do
            {
                assert(Local.m_ReadLockCount >= 1);

                auto NewState = Local;
                NewState.m_ReadLockCount -= 1;
                if (m_GlobalLock.compare_exchange_weak(Local, NewState, std::memory_order_release, std::memory_order_relaxed))
                    break;

            } while(true);
        }

        //================================================================================================

        void GlobalLockForWrite() noexcept
        { 
            auto Local = m_GlobalLock.load(std::memory_order_relaxed);
            do
            {
                if (Local.m_WriteLock == 0 && Local.m_WritePendingCount == 0 && Local.m_ReadLockCount == 0)
                {
                    auto NewState = Local;
                    NewState.m_WriteLock = 1;
                    if (m_GlobalLock.compare_exchange_weak(Local, NewState, std::memory_order_release, std::memory_order_relaxed))
                    {
                        return;
                    }
                }
                else
                {
                    auto NewState = Local;
                    NewState.m_WritePendingCount += 1;
                    if (m_GlobalLock.compare_exchange_weak(Local, NewState, std::memory_order_release, std::memory_order_relaxed))
                    {
                        do
                        {
                            Local = m_GlobalLock.load(std::memory_order_relaxed);
                            if (Local.m_WriteLock == 0 && Local.m_ReadLockCount == 0)
                            {
                                NewState = Local;
                                NewState.m_WritePendingCount -= 1;
                                NewState.m_WriteLock          = 1;
                                if ( m_GlobalLock.compare_exchange_weak(Local, NewState, std::memory_order_release, std::memory_order_relaxed))
                                {
                                    return;
                                }
                            }
                        } while (true);
                    }
                }
            } while (true);
        }

        //================================================================================================

        void GlobalLockForRead() noexcept
        {
            auto Local = m_GlobalLock.load(std::memory_order_relaxed);
            do
            {
                auto NewState = Local;

                if (Local.m_WriteLock == 0 && Local.m_WritePendingCount == 0)
                {
                    NewState.m_ReadLockCount += 1;
                    if (m_GlobalLock.compare_exchange_weak(Local, NewState, std::memory_order_release, std::memory_order_relaxed))
                    {
                        return;
                    }
                }
                else
                {
                    Local = m_GlobalLock.load(std::memory_order_relaxed);
                }
            } while (true);
        }

        //================================================================================================

        struct end_iterator
        {
            constexpr end_iterator(unordered_lockless_map*){}
        };

        struct begin_iterator
        {
            unordered_lockless_map& m_This;
            std::int32_t   m_Index{ 0 };
            std::uint32_t  m_Count{ 0 };

            begin_iterator(unordered_lockless_map* x) : m_This(*x)
            {
                if (m_This.m_Count.load(std::memory_order_relaxed) == 0)
                {
                    m_Index = static_cast<std::int32_t>(m_This.m_MaxDataCount);
                }
                else
                {
                    // Find the first entry
                    do
                    {
                        auto BitEntry = m_This.m_pBitArray[m_Index / 64].load(std::memory_order_relaxed);
                        if (BitEntry & (1ULL << (m_Index & 63)))
                            break;

                        ++m_Index;
                        assert(m_Index < m_This.m_MaxDataCount);

                    } while (true);
                }
            }
            begin_iterator(const begin_iterator&) = default;
            begin_iterator(begin_iterator&&) = default;
            begin_iterator& operator=(const begin_iterator&) = default;
            begin_iterator& operator=(begin_iterator&&) = default;

            begin_iterator& operator++()
            {
                ++m_Count;
                if (m_Count < m_This.m_Count.load(std::memory_order_relaxed) )
                {
                    do
                    {
                        ++m_Index;
                        assert(m_Index < m_This.m_MaxDataCount);

                        auto BitEntry = m_This.m_pBitArray[m_Index / 64].load(std::memory_order_relaxed);
                        if (BitEntry & (1ULL << (m_Index & 63)))
                            break;

                    } while (true);
                }

                return *this;
            }

            constexpr bool operator==(const end_iterator& ) const noexcept
            {
                return m_Count == m_This.m_Count.load(std::memory_order_relaxed);
            }

            constexpr bool operator!=(const end_iterator& ) const noexcept
            {
                return m_Count != m_This.m_Count.load(std::memory_order_relaxed);
            }

            constexpr data_pair& operator*() noexcept
            {
                return m_This.m_pData[m_Index].m_Pair;
            }
        };

        begin_iterator begin() { return begin_iterator(this); }
        end_iterator   end() { return end_iterator(this); }

        // ========================================================================================
        // Allocates the Initial data

        void Initialize(std::size_t Size) noexcept
        {
            const auto NewSizeData     = Size * sizeof(data);
            const auto NewSizeKeys     = Size * sizeof(key_entry) * hash_key_ration_to_data_v;
            const auto NewSizeBitArray = (Size * sizeof(std::uint64_t)) / 64 + 1;

            m_MaxDataCount  = Size;
            m_pData         = static_cast<data*>                        (_aligned_malloc(NewSizeData, alignof(data)));
            m_pKeys         = static_cast<key_entry*>                   (_aligned_malloc(NewSizeKeys, alignof(key_entry)));
            m_pBitArray     = static_cast<std::atomic<std::uint64_t>*>  (_aligned_malloc(NewSizeBitArray, alignof(std::atomic<std::uint64_t>)));

            // Clear the keys and the bit array
            memset(m_pKeys, 0, NewSizeKeys);
            memset(m_pBitArray, 0, NewSizeBitArray);

            // Set the empty list
            for (std::uint32_t i = 0u; i < m_MaxDataCount; ++i) m_pData[i].m_NextEmpty = i + 1;
        }

        // ========================================================================================
        // Allocates the actual data as well as construct it in a thread safe manner
        void resize(std::size_t Size, bool bUnsafe = false ) noexcept
        {
            if (bUnsafe==false) GlobalLockForWrite();

            if ( m_MaxDataCount == 0 ) 
            {
                Initialize(128);
                if (bUnsafe == false) GlobalUnlockWrite();
                return;
            }
            assert(Size >= m_Count);

            unordered_lockless_map TempMap;
            TempMap.Initialize(Size);

            // By reinserting the nodes one by one we can ensure that all the nodes are
            // optimize in the right order, which should allow to run more efficiently.
            for (data_pair& E : *this)
            {
                TempMap.Insert(E.first, [&]( T_VALUE& V)
                {
                    V = std::move(E.second);
                    //std::memcpy( &V, &E.second, sizeof(V));
                });
            }

            // Free old memory (no need to call the destructor as the std::move should have done that...
            _aligned_free(m_pData);
            _aligned_free(m_pKeys);
            _aligned_free(m_pBitArray);

            // Copy over the new memory
            m_MaxDataCount  = TempMap.m_MaxDataCount;
            m_pData         = TempMap.m_pData;
            m_pKeys         = TempMap.m_pKeys;
            m_pBitArray     = TempMap.m_pBitArray;

            // clear the temp
            TempMap.m_Count         = 0;
            TempMap.m_MaxDataCount  = 0;
            TempMap.m_pData         = nullptr;
            TempMap.m_pKeys         = nullptr;
            TempMap.m_pBitArray     = nullptr;

            if (bUnsafe == false) GlobalUnlockWrite();
        }

        // ========================================================================================
        // Clears the map
        void clear() noexcept
        {
            GlobalLockForWrite();

            // Release all the data
            for (auto& E : *this)
            {
                std::destroy_at(&E);
            }

            // Reset all the keys
            memset(m_pKeys,     0, sizeof(m_pKeys[0]) * m_MaxDataCount);
            memset(m_pBitArray, 0, sizeof(m_pBitArray[0]) * (m_MaxDataCount / 64 + 1));
            m_Count = 0;

            // Set the empty list
            for (std::uint32_t i = 0u; i < m_MaxDataCount; ++i) m_pData[i].m_NextEmpty = i + 1;

            GlobalUnlockWrite();
        }

        std::atomic<atomic_key>         m_GlobalLock    = {};
        std::atomic_uint                m_Count         = 0;
        std::int64_t                    m_MaxDataCount  = 0;
        data*                           m_pData         = nullptr;
        key_entry*                      m_pKeys         = nullptr;
        std::atomic<std::uint64_t>*     m_pBitArray     = nullptr;
        std::atomic<empty_list>         m_EmptyList     = {};
    };
} // namespace

#endif
