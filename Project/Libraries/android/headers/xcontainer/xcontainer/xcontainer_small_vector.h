#ifndef XCONTAINER_SMALL_VECTOR_H
#define XCONTAINER_SMALL_VECTOR_H
#pragma once

namespace xcontainer
{
    template<typename T, std::size_t T_INLINE_COUNT = 4>
    class small_vector
    {
    public:

        using iterator               = T*;
        using const_iterator         = const T*;
        using reverse_iterator       = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        // Constructors
        small_vector() noexcept = default;

        // Copy constructor
        small_vector(const small_vector& other)
        {
            reserve(other.m_Size);
            std::uninitialized_copy(other.m_pData, other.m_pData + other.m_Size, m_pData);
            m_Size = other.m_Size;
        }

        // Move constructor
        small_vector(small_vector&& other) noexcept
            : m_Size(other.m_Size)
            , m_Capacity(other.m_Capacity)
            , m_pData(other.m_pData)
        {
            if (other.m_pData == reinterpret_cast<T*>(other.m_Inline))
            {
                // Move inline elements
                std::uninitialized_move(other.m_pData, other.m_pData + other.m_Size, m_pData);
                std::destroy(other.m_pData, other.m_pData + other.m_Size);
            }
            else
            {
                // Take ownership of heap buffer
                std::copy(std::begin(other.m_Inline), std::end(other.m_Inline), m_Inline);
            }

            other.m_Size = 0;
            other.m_Capacity = T_INLINE_COUNT;
            other.m_pData = reinterpret_cast<T*>(other.m_Inline);
        }

        // Destructor
        ~small_vector()
        {
            std::destroy(m_pData, m_pData + m_Size);

            if (m_pData != reinterpret_cast<T*>(m_Inline))
            {
                _aligned_free(m_pData);
            }
        }

        void reset()
        {
            clear();

            if (m_pData != reinterpret_cast<T*>(m_Inline))
            {
                _aligned_free(m_pData);
            }

            m_pData = reinterpret_cast<T*>(m_Inline);
            m_Capacity = T_INLINE_COUNT;
        }

        // Copy assignment
        small_vector& operator = (const small_vector& other)
        {
            reset();
            return *new(this) T(other);
        }

        // Move assignment
        small_vector& operator = (small_vector&& other) noexcept
        {
            reset();
            return *new(this) T(std::move(other));
        }

        // Push back
        void push_back(const T& value)
        {
            if (m_Size >= m_Capacity)
            {
                grow();
            }
            new (m_pData + m_Size) T(value);
            ++m_Size;
        }

        void push_back(T&& value)
        {
            if (m_Size >= m_Capacity)
            {
                grow();
            }
            new (m_pData + m_Size) T(std::move(value));
            ++m_Size;
        }

        template<typename... Args>
        void emplace_back(Args&&... args)
        {
            if (m_Size >= m_Capacity)
            {
                grow();
            }

            new (m_pData + m_Size) T(std::forward<Args>(args)...);
            ++m_Size;
        }

        // Accessors
        T& operator[](size_t index) noexcept
        {
            assert(index < m_Size);
            return m_pData[index];
        }

        const T& operator[](size_t index) const noexcept
        {
            assert(index < m_Size);
            return m_pData[index];
        }

        T& at(size_t index)
        {
            if (index >= m_Size) throw std::out_of_range("Index out of bounds");
            return m_pData[index];
        }

        const T& at(size_t index) const
        {
            if (index >= m_Size) throw std::out_of_range("Index out of bounds");
            return m_pData[index];
        }

        // Size and capacity
        std::size_t size()      const noexcept { return m_Size; }
        std::size_t capacity()  const noexcept { return m_Capacity; }
        bool        empty()     const noexcept { return m_Size == 0; }

        void shrink_to_fit()
        {
            std::size_t new_capacity = std::max(m_Size, T_INLINE_COUNT);
            if (new_capacity < m_Capacity)
            {
                T* new_data = reinterpret_cast<T*>(_aligned_malloc(new_capacity * sizeof(T), alignof(T)));
                if (!new_data) throw std::bad_alloc();

                std::uninitialized_move(m_pData, m_pData + m_Size, new_data);
                std::destroy(m_pData, m_pData + m_Size);

                if (m_pData != reinterpret_cast<T*>(m_Inline))
                {
                    _aligned_free(m_pData);
                }

                m_pData = new_data;
                m_Capacity = new_capacity;
            }
        }
        // Modifiers
        void reserve(std::size_t new_capacity)
        {
            if (new_capacity > m_Capacity)
            {
                T* new_data = reinterpret_cast<T*>(_aligned_malloc(new_capacity * sizeof(T), alignof(T)));
                if (!new_data) throw std::bad_alloc();

                std::uninitialized_move(m_pData, m_pData + m_Size, new_data);
                std::destroy(m_pData, m_pData + m_Size);

                if (m_pData != reinterpret_cast<T*>(m_Inline))
                {
                    _aligned_free(m_pData);
                }

                m_pData = new_data;
                m_Capacity = new_capacity;
            }
        }

        // Resize
        void resize(std::size_t new_size)
        {
            if (new_size > m_Capacity)
            {
                reserve(std::max(new_size, T_INLINE_COUNT));
            }

            if (new_size > m_Size)
            {
                std::uninitialized_default_construct(m_pData + m_Size, m_pData + new_size);
            }
            else if (new_size < m_Size)
            {
                std::destroy(m_pData + new_size, m_pData + m_Size);
            }

            m_Size = new_size;
        }

        void resize(std::size_t new_size, const T& value)
        {
            if (new_size > m_Capacity)
            {
                reserve(std::max(new_size, T_INLINE_COUNT));
            }

            if (new_size > m_Size)
            {
                std::uninitialized_fill(m_pData + m_Size, m_pData + new_size, value);
            }
            else if (new_size < m_Size)
            {
                std::destroy(m_pData + new_size, m_pData + m_Size);
            }
            m_Size = new_size;
        }

        void clear() noexcept
        {
            std::destroy(m_pData, m_pData + m_Size);
            m_Size = 0;
        }

        // Iterators
        iterator        rbegin()            noexcept { return reverse_iterator(end()); }
        iterator        rend()              noexcept { return reverse_iterator(begin()); }
        const_iterator  rbegin()    const   noexcept { return const_reverse_iterator(end()); }
        const_iterator  rend()      const   noexcept { return const_reverse_iterator(begin()); }
        const_iterator  crbegin()   const   noexcept { return const_reverse_iterator(cend()); }
        const_iterator  crend()     const   noexcept { return const_reverse_iterator(cbegin()); }
        iterator        begin()             noexcept { return m_pData; }
        iterator        end()               noexcept { return m_pData + m_Size; }
        const_iterator  begin()     const   noexcept { return m_pData; }
        const_iterator  end()       const   noexcept { return m_pData + m_Size; }
        const_iterator  cbegin()    const   noexcept { return m_pData; }
        const_iterator  cend()      const   noexcept { return m_pData + m_Size; }

        // Swap
        friend void swap(small_vector& a, small_vector& b) noexcept
        {
            using std::swap;
            if (a.m_pData == reinterpret_cast<T*>(a.m_Inline) &&
                b.m_pData == reinterpret_cast<T*>(b.m_Inline))
            {
                small_vector tmp;
                tmp.m_Size = a.m_Size;

                std::uninitialized_move(a.m_pData, a.m_pData + a.m_Size, tmp.m_pData);
                std::destroy(a.m_pData, a.m_pData + a.m_Size);
                a.m_Size = b.m_Size;

                std::uninitialized_move(b.m_pData, b.m_pData + b.m_Size, a.m_pData);
                std::destroy(b.m_pData, b.m_pData + b.m_Size);
                b.m_Size = tmp.m_Size;

                std::uninitialized_move(tmp.m_pData, tmp.m_pData + tmp.m_Size, b.m_pData);
            }
            else
            {
                swap(a.m_pData, b.m_pData);
                swap(a.m_Inline, b.m_Inline);
            }
            swap(a.m_Size, b.m_Size);
            swap(a.m_Capacity, b.m_Capacity);
        }

    private:

        void grow()
        {
            std::size_t new_capacity = m_Capacity == 0 ? T_INLINE_COUNT : m_Capacity * 3 / 2; // 1.5x growth
            reserve(new_capacity);
        }

        std::size_t             m_Size = 0;
        std::size_t             m_Capacity = T_INLINE_COUNT;
        T* m_pData = reinterpret_cast<T*>(m_Inline);
        alignas(T) std::byte    m_Inline[sizeof(T) * T_INLINE_COUNT]{};
    };
}

#endif