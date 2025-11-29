#ifndef XCONTAINER_BASICS_H
#define XCONTAINER_BASICS_H
#pragma once

//
// Common headers
//
#include <atomic>
#include <cassert>
#include <concepts>
#include <type_traits>
#include <memory>

//
// deal with microsoft...
//
#ifndef XALIGN_ALLOC
#   define XALIGN_ALLOC
#   if defined(_MSC_VER)
namespace std
{
    inline void* aligned_alloc(std::size_t alignment, std::size_t size)
    {
        // Trying to prevent the user for switching around the parameters...
        // We should be following the C++ standard rathern than whatever MS wants...
        assert(size > alignment);
        return _aligned_malloc(size, alignment);
    }

    inline void aligned_free(void* ptr)
    {
        _aligned_free(ptr);
    }
}
#   else
namespace std
{
    inline void aligned_free(void* ptr)
    {
        free(ptr);
    }
}
#   endif // #if defined(_MSC_VER)
#endif // #ifndef XALIGN_ALLOC

//
// Helpers
//
namespace xcontainer
{
    namespace details
    {
        // Helper to extract the first argument type
        template<typename T>
        struct first_arg_type;

        template<typename R, typename Arg1, typename... Args>
        struct first_arg_type<R(Arg1, Args...)>
        {
            using type = Arg1;
        };

        template<typename R, typename C, typename Arg1, typename... Args>
        struct first_arg_type<R(C::*)(Arg1, Args...)>
        {
            using type = Arg1;
        };

        template<typename R, typename C, typename Arg1, typename... Args>
        struct first_arg_type<R(C::*)(Arg1, Args...) const>
        {
            using type = Arg1;
        };

        // Concept to check if the first argument is const
        template<typename T>
        concept is_first_arg_const = std::is_const_v<std::remove_reference_t<typename first_arg_type<decltype(&T::operator())>::type>>;

        template<typename T>
        concept is_first_arg_is_reference = std::is_reference_v<typename first_arg_type<decltype(&T::operator())>::type>;
    }

    namespace details
    {
        //------------------------------------------------------------------------------
        // Description:
        //      Computes the Log2 of an integral value. 
        //      It answer the question: how many bits do I need to rshift 'y' to make this expression true: 
        //      (input) x == 1 << 'y'. Assuming x was originally a power of 2.
        //------------------------------------------------------------------------------
        template< typename T> constexpr
        T Log2Int(T x, int p = 0) noexcept
        {
            return (x <= 1) ? p : Log2Int(x >> 1, p + 1);
        }
    }

    //--------------------------------------------------------------------------------------------
    // Makes a object type unique
    //--------------------------------------------------------------------------------------------
    namespace details
    {
        template< std::size_t T_MAX_SIZE_V, template< typename, std::size_t > class T_PARENT  >
        struct container_static_to_dynamic
        {
            template< typename T >
            struct type : T_PARENT<T, T_MAX_SIZE_V>
            {
                type() = default;
            };
        };
    }
    template< template< typename, std::size_t > class T_PARENT, std::size_t T_MAX_SIZE_V >
    using container_static_to_dynamic = typename details::container_static_to_dynamic<T_MAX_SIZE_V, T_PARENT>;

    //--------------------------------------------------------------------------------------------
    // raw array and raw entry
    //--------------------------------------------------------------------------------------------

    template< typename T>
    struct alignas(T) raw_entry : std::array<char, sizeof(T)>
    {};

    template< typename T, std::size_t T_COUNT_V>
    struct raw_array : std::array< raw_entry<T>, T_COUNT_V >
    {};

    //------------------------------------------------------------------------------
    // Right now just assume that is always 64... but we need to add more supports for different platforms
    //------------------------------------------------------------------------------
    constexpr std::size_t getCacheLineSize(void) noexcept
    {
        return 64;
    }

}
#endif
