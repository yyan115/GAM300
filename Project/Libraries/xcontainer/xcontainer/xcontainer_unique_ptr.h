#ifndef XCONTAINER_UNIQUE_PTR_H
#define XCONTAINER_UNIQUE_PTR_H
#pragma once

#ifndef XCONTAINER_BASICS_H
    #include "xcontainer_basics.h"
#endif

#include <cstddef>
#include <utility>
#include <iterator> // for std::reverse_iterator
#include <memory>

namespace xcontainer
{
    template <typename T>
    struct unique_raw_ptr;

    template <typename T>
    struct unique_ptr : std::unique_ptr<T>
    {
        using std::unique_ptr<T>::unique_ptr;
    };

    template <typename T>
    struct unique_ptr<T[]>
    {
        using iterator                  = T*;
        using const_iterator            = const T*;
        using reverse_iterator          = std::reverse_iterator<T*>;
        using const_reverse_iterator    = std::reverse_iterator<const T*>;

        unique_ptr() = default;

        explicit unique_ptr(std::size_t count) : m_Data(new T[count]), m_Count(count) {}

        unique_ptr(const unique_ptr&) = delete;
        unique_ptr(unique_ptr&& other) noexcept
            : m_Data(std::exchange(other.m_Data, nullptr)),
              m_Count(std::exchange(other.m_Count, 0)) {}

        unique_ptr& operator=(const unique_ptr&) = delete;
        unique_ptr& operator=(unique_ptr&& other) noexcept
        {
            if (this != &other)
            {
                Delete();
                m_Data  = std::exchange(other.m_Data, nullptr);
                m_Count = std::exchange(other.m_Count, 0);
            }
            return *this;
        }

        ~unique_ptr(void)
        {
            Delete();
        }

        void New(std::size_t Count)
        {
            Delete();
            m_Data = new T[Count];
            m_Count = Count;
        }

        void Delete(void)
        {
            if(m_Data) delete[] m_Data;
            m_Data = nullptr;
            m_Count = 0;
        }

        void reset(T* new_data = nullptr, std::size_t new_count = 0)
        {
            Delete();
            m_Data  = new_data;
            m_Count = new_count;
        }

        T* release()
        {
            T* temp = m_Data;
            m_Data = nullptr;
            m_Count = 0;
            return temp;
        }

        void swap(unique_ptr& other) noexcept
        {
            std::swap(m_Data,  other.m_Data);
            std::swap(m_Count, other.m_Count);
        }

        T& at(std::size_t index) const
        {
            assert(index < m_Count);
            return m_Data[index];
        }

        T& operator[](std::size_t index)
        {
            assert(index < m_Count);
            return m_Data[index];
        }

        const T& operator [](std::size_t index) const
        {
            assert(index<m_Count);
            return m_Data[index];
        }


        [[nodiscard]] T*                    get             (void)      const   { return m_Data; }
        [[nodiscard]] T*                    data            (void)      const   { return m_Data; } 
        [[nodiscard]] std::size_t           size            (void)      const   { return m_Count; }    
        [[nodiscard]] bool                  empty           (void)      const   { return m_Count == 0; }   
        explicit                            operator bool   (void)      const   { return m_Data != nullptr; }

        // Range support
        iterator                            begin           (void)      const   { return m_Data; }
        iterator                            end             (void)      const   { return m_Data + m_Count; }
        const_iterator                      cbegin          (void)      const   { return m_Data; }
        const_iterator                      cend            (void)      const   { return m_Data + m_Count; }

        // Mutable reverse iterators
        reverse_iterator                    rbegin          (void)              { return reverse_iterator(end()); }
        reverse_iterator                    rend            (void)              { return reverse_iterator(begin()); }

        // Const reverse iterators
        const_reverse_iterator              rbegin          (void)      const   { return const_reverse_iterator(end()); }
        const_reverse_iterator              rend            (void)      const   { return const_reverse_iterator(begin()); }

        // Const-qualified reverse iterators (for use in const contexts)
        const_reverse_iterator              crbegin         (void)      const   { return const_reverse_iterator(cend()); }
        const_reverse_iterator              crend           (void)      const   { return const_reverse_iterator(cbegin()); }

        T*          m_Data  = nullptr;
        std::size_t m_Count = 0;
    };

    template <typename T>
    struct unique_raw_ptr<T[]>
    {
        using iterator                  = T*;
        using const_iterator            = const T*;
        using reverse_iterator          = std::reverse_iterator<T*>;
        using const_reverse_iterator    = std::reverse_iterator<const T*>;

        unique_raw_ptr() = default;

        explicit unique_raw_ptr(std::size_t count) : m_Data(std::aligned_alloc(alignof(T), sizeof(T)* count)), m_Count(count) {}

        unique_raw_ptr(const unique_raw_ptr&) = delete;
        unique_raw_ptr(unique_raw_ptr&& other) noexcept
            : m_Data(std::exchange(other.m_Data, nullptr)),
              m_Count(std::exchange(other.m_Count, 0)) {}

        unique_raw_ptr& operator=(const unique_raw_ptr&) = delete;
        unique_raw_ptr& operator=(unique_raw_ptr&& other) noexcept
        {
            if (this != &other)
            {
                Free();
                m_Data  = std::exchange(other.m_Data, nullptr);
                m_Count = std::exchange(other.m_Count, 0);
            }
            return *this;
        }

        ~unique_raw_ptr(void)
        {
            Free();
        }

        void Alloc(std::size_t Count)
        {
            Free();
            m_Data = new T[Count];
            m_Count = Count;
        }

        void Free(void)
        {
            if(m_Data) std::aligned_free( m_Data );
            m_Data = nullptr;
            m_Count = 0;
        }

        void reset(T* new_data = nullptr, std::size_t new_count = 0)
        {
            Free();
            m_Data  = new_data;
            m_Count = new_count;
        }

        T* release()
        {
            T* temp = m_Data;
            m_Data = nullptr;
            m_Count = 0;
            return temp;
        }

        void swap(unique_raw_ptr& other) noexcept
        {
            std::swap(m_Data,  other.m_Data);
            std::swap(m_Count, other.m_Count);
        }

        T& at(std::size_t index) const
        {
            assert(index < m_Count);
            return m_Data[index];
        }

        T& operator[](std::size_t index)
        {
            assert(index < m_Count);
            return m_Data[index];
        }

        const T& operator [](std::size_t index) const
        {
            assert(index < m_Count);
            return m_Data[index];
        }

        [[nodiscard]] T*                    get             (void)      const   { return m_Data; }
        [[nodiscard]] T*                    data            (void)      const   { return m_Data; } 
        [[nodiscard]] std::size_t           size            (void)      const   { return m_Count; }    
        [[nodiscard]] bool                  empty           (void)      const   { return m_Count == 0; }   
        explicit                            operator bool   (void)      const   { return m_Data != nullptr; }

        // Range support
        iterator                            begin           (void)      const   { return m_Data; }
        iterator                            end             (void)      const   { return m_Data + m_Count; }
        const_iterator                      cbegin          (void)      const   { return m_Data; }
        const_iterator                      cend            (void)      const   { return m_Data + m_Count; }

        // Mutable reverse iterators
        reverse_iterator                    rbegin          (void)              { return reverse_iterator(end()); }
        reverse_iterator                    rend            (void)              { return reverse_iterator(begin()); }

        // Const reverse iterators
        const_reverse_iterator              rbegin          (void)      const   { return const_reverse_iterator(end()); }
        const_reverse_iterator              rend            (void)      const   { return const_reverse_iterator(begin()); }

        // Const-qualified reverse iterators (for use in const contexts)
        const_reverse_iterator              crbegin         (void)      const   { return const_reverse_iterator(cend()); }
        const_reverse_iterator              crend           (void)      const   { return const_reverse_iterator(cbegin()); }

        T*          m_Data  = nullptr;
        std::size_t m_Count = 0;
    };
}
#endif