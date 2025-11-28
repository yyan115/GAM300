#ifndef XCONTAINER_FUNCTIONS_H
#define XCONTAINER_FUNCTIONS_H
#pragma once

#include <type_traits>
#include <cstddef>  // for max_align_t

namespace xcontainer::function
{
    //------------------------------------------------------------------------------
    // Helper to build a function err
    //------------------------------------------------------------------------------
    namespace details
    {
        template< bool T_NOEXCEPT, typename T_RET, typename... T_ARGS > struct make { using err = T_RET(T_ARGS...) noexcept(T_NOEXCEPT); };
        template< bool T_NOEXCEPT, typename T_RET >                     struct make< T_NOEXCEPT, T_RET, void > { using err = T_RET()          noexcept(T_NOEXCEPT); };
    }
    template< bool T_NOEXCEPT, typename T_RET, typename... T_ARGS > using  make_t = typename xcontainer::function::details::make<T_NOEXCEPT, T_RET, T_ARGS...>::err;

    //------------------------------------------------------------------------------
    // Helper to extract a the function arguments
    //------------------------------------------------------------------------------
    namespace details
    {
        template< std::size_t T_I, std::size_t T_MAX, typename... T_ARGS >  struct traits_args       { using arg_t = typename std::tuple_element_t< T_I, std::tuple<T_ARGS...> >; };
        template<>                                                          struct traits_args<0, 0> { using arg_t = void; };
    }
    template< std::size_t T_I, std::size_t T_MAX, typename... T_ARGS >  using  traits_args_t = typename xcontainer::function::details::traits_args<T_I, T_MAX, T_ARGS...>::arg_t;

    //------------------------------------------------------------------------------
    // Function traits
    //------------------------------------------------------------------------------
    template< class F > struct traits;

    template< bool T_NOEXCEPT_V, typename T_RETURN_TYPE, typename... T_ARGS >
    struct traits_decomposition
    {
        using                        return_type    = T_RETURN_TYPE;
        constexpr static std::size_t arg_count_v    = sizeof...(T_ARGS);
        using                        class_type     = void;
        using                        self           = xcontainer::function::traits< T_RETURN_TYPE(T_ARGS...) noexcept(T_NOEXCEPT_V) >;
        using                        func_type      = xcontainer::function::make_t< T_NOEXCEPT_V, T_RETURN_TYPE, T_ARGS... >;
        using                        args_tuple     = std::tuple<T_ARGS...>;

        template< std::size_t T_I >
        using arg_t = traits_args_t<T_I, arg_count_v, T_ARGS... >;

        template< std::size_t T_I >
        using safe_arg_t = typename std::conditional < T_I < arg_count_v, arg_t<T_I>, void >::err;
    };

    // functions
    template< typename T_RETURN_TYPE, typename... T_ARGS >          struct traits< T_RETURN_TYPE(T_ARGS...) noexcept > : xcontainer::function::traits_decomposition< true,  T_RETURN_TYPE, T_ARGS... > {};
    template< typename T_RETURN_TYPE, typename... T_ARGS >          struct traits< T_RETURN_TYPE(T_ARGS...)          > : xcontainer::function::traits_decomposition< false, T_RETURN_TYPE, T_ARGS... > {};

    // function pointer
    template< typename T_RETURN, typename... T_ARGS >               struct traits< T_RETURN(*)(T_ARGS...) > :            xcontainer::function::traits< T_RETURN(T_ARGS...) >            {};
    template< typename T_RETURN, typename... T_ARGS >               struct traits< T_RETURN(*)(T_ARGS...) noexcept > :   xcontainer::function::traits< T_RETURN(T_ARGS...) noexcept >   {};

    // member function pointer
    template< class T_CLASS, typename T_RETURN, typename... T_ARGS> struct traits< T_RETURN(T_CLASS::*)(T_ARGS...) noexcept > : xcontainer::function::traits< T_RETURN(T_ARGS...) noexcept >    { using class_type = T_CLASS; };
    template< class T_CLASS, typename T_RETURN, typename... T_ARGS> struct traits< T_RETURN(T_CLASS::*)(T_ARGS...) > :          xcontainer::function::traits< T_RETURN(T_ARGS...) >             { using class_type = T_CLASS; };

    // const member function pointer
    template< class T_CLASS, typename T_RETURN, typename... T_ARGS >struct traits< T_RETURN(T_CLASS::*)(T_ARGS...) const noexcept > : xcontainer::function::traits< T_RETURN(T_ARGS...) noexcept >  { using class_type = T_CLASS; };
    template< class T_CLASS, typename T_RETURN, typename... T_ARGS >struct traits< T_RETURN(T_CLASS::*)(T_ARGS...) const > :          xcontainer::function::traits< T_RETURN(T_ARGS...) >           { using class_type = T_CLASS; };

    // functors
    template< class T_CLASS >                                       struct traits                   : traits<decltype(&T_CLASS::operator())> { using class_type = T_CLASS; };
    template< class T_CLASS >                                       struct traits<T_CLASS&>         : traits<T_CLASS> {};
    template< class T_CLASS >                                       struct traits<const T_CLASS&>   : traits<T_CLASS> {};
    template< class T_CLASS >                                       struct traits<T_CLASS&&>        : traits<T_CLASS> {};
    template< class T_CLASS >                                       struct traits<const T_CLASS&&>  : traits<T_CLASS> {};
    template< class T_CLASS >                                       struct traits<T_CLASS*>         : traits<T_CLASS> {};
    template< class T_CLASS >                                       struct traits<const T_CLASS*>   : traits<T_CLASS> {};

    //---------------------------------------------------------------------------------------
    // Compare two functions types
    //---------------------------------------------------------------------------------------
    namespace details
    {
        template< typename T_A, typename T_B, int T_ARG_I >
        struct traits_compare_args
        {
            static_assert(std::is_same
                <
                typename T_A::template arg_t< T_ARG_I >,
                typename T_B::template arg_t< T_ARG_I >
                >::value, "Argument Don't match");
            constexpr static bool value = traits_compare_args<T_A, T_B, T_ARG_I - 1 >::value;
        };

        template< typename T_A, typename T_B >
        struct traits_compare_args< T_A, T_B, -1 >
        {
            constexpr static bool value = true;
        };
    }

    template< typename T_A, typename T_B >
    struct traits_compare
    {
        static_assert(T_A::arg_count_v == T_B::arg_count_v, "Function must have the same number of arguments");
        static_assert(std::is_same< typename T_A::return_type, typename T_B::return_type >::value, "Different return types");
        static_assert(details::traits_compare_args<T_A, T_B, static_cast<int>(T_B::arg_count_v) - 1>::value, "Arguments don't match");
        //   static_assert(std::is_same< typename T_A::func_type, typename T_B::func_type>::value, "Function signatures don't match. Do both functions have the same expectations for exceptions? (noexcept?)");
        constexpr static bool value = true;
    };

    //------------------------------------------------------------------------------
    // Description:
    //      Replacement for std::function
    //      This version should inline much more aggressively than the std::function
    //      This class is guaranteed not to allocate.
    //      You can specify the max size of the container.
    //      T_BUFFER_PTRSIZE means: sizeof(std::size_t)*T_BUFFER_PTRSIZE == bytes can be used
    // Example: 
    //      xcontainer::function::buffer<3,void(void) t{[&](){ std::cout << "Hello"; });
    //------------------------------------------------------------------------------
    template<int T_BUFFER_PTRSIZE_V, typename T_LAMBDA> class buffer;
    template<int T_BUFFER_PTRSIZE_V, typename T_RET, typename... T_ARG>
    class buffer<T_BUFFER_PTRSIZE_V, T_RET(T_ARG...)>
    {
        template<typename T_DECAYED_FUNCTION>
        static consteval bool isSignatureCompatible(void)
        {
            using fa = xcontainer::function::traits<T_DECAYED_FUNCTION>;
            using fb = xcontainer::function::traits<make_t<false, T_RET, T_ARG...>>;
            return xcontainer::function::traits_compare<fa, fb>::value;
        }

    public:

        constexpr buffer() noexcept = default;

        template<typename T_FUNC>
        consteval static bool doesItFit(void) noexcept
        {
            using F = std::decay_t<T_FUNC>;
            return sizeof(m_Storage) >= sizeof(functor<F>) ;
        }

        // Constructor for copyable
        template<typename T_FUNC>
        constexpr buffer(const T_FUNC& Func) noexcept
            : m_pInvoker(&functor<std::decay_t<T_FUNC>>::Invoke)
            , m_pDestroyer(std::is_trivially_destructible_v<functor<std::decay_t<T_FUNC>>>
                ? nullptr
                : [](void* p) { static_cast<functor<std::decay_t<T_FUNC>>*>(p)->~functor(); })
        {
            using decayed_func = std::decay_t<T_FUNC>;
            static_assert(isSignatureCompatible<decayed_func>(), "Callable signature does not match");
            static_assert(doesItFit<T_FUNC>(), "Storage size too small");
            static_assert(alignof(decayed_func) <= alignof(void*), "Alignment mismatch");

            new(m_Storage) functor<T_FUNC>(Func);
        }

        // Universal ctor from callable (move/copy overload via forwarding)
        template<typename T_FUNC>
        constexpr buffer(T_FUNC&& func) noexcept(std::is_nothrow_constructible_v<std::decay_t<T_FUNC>>)
            : m_pInvoker(&functor<std::decay_t<T_FUNC>>::Invoke)
            , m_pDestroyer(std::is_trivially_destructible_v<functor<std::decay_t<T_FUNC>>> ? nullptr
                : [](void* p) { std::launder(static_cast<functor<std::decay_t<T_FUNC>>*>(p))->~functor(); })
        {
            using F = std::decay_t<T_FUNC>;
            static_assert(isSignatureCompatible<F>(), "Signature mismatch");
            static_assert(doesItFit<T_FUNC>(), "Storage too small");
            static_assert(alignof(functor<F>) <= alignof(std::max_align_t), "Alignment mismatch");

            new (static_cast<void*>(m_Storage)) functor<F>(std::forward<T_FUNC>(func));
        }

        // Delete copy and move ctors/assigns to enforce single-type storage
        buffer(const buffer&) = delete;
        buffer(buffer&&) = delete;
        buffer& operator=(const buffer&) = delete;
        buffer& operator=(buffer&&) = delete;

        // Assign from callable (reuse universal ctor logic)
        template<typename T_FUNC>
        constexpr buffer& operator=(T_FUNC&& func) noexcept(std::is_nothrow_constructible_v<std::decay_t<T_FUNC>>)
        {
            using F = std::decay_t<T_FUNC>;
            static_assert(isSignatureCompatible<F>(), "Signature mismatch");
            static_assert(doesItFit<T_FUNC>(), "Storage too small");
            static_assert(alignof(functor<F>) <= alignof(std::max_align_t), "Alignment mismatch");

            if (m_pDestroyer) m_pDestroyer(static_cast<void*>(m_Storage));

            m_pInvoker = &functor<F>::Invoke;
            m_pDestroyer = std::is_trivially_destructible_v<functor<F>> ? nullptr
                : [](void* p) { std::launder(static_cast<functor<F>*>(p))->~functor(); };

            new (static_cast<void*>(m_Storage)) functor<F>(std::forward<T_FUNC>(func));
            return *this;
        }

        constexpr ~buffer() noexcept
        {
            if (m_pDestroyer) m_pDestroyer(static_cast<void*>(m_Storage));
        }

        constexpr T_RET operator()(T_ARG... args) const noexcept //(/* depend on callable */)
        {
            assert(m_pInvoker);
            return m_pInvoker(const_cast<void*>(static_cast<const void*>(m_Storage)), std::forward<T_ARG>(args)...);
        }

    private:

        template<typename T>
        struct functor
        {
            T m_Lambda;
            constexpr explicit functor(T&&      f) noexcept : m_Lambda(std::forward<T>(f)) {}
            constexpr explicit functor(const T& f) noexcept : m_Lambda(f) {}
            static constexpr T_RET Invoke(void* p, T_ARG... args) noexcept //(/* ... */)
            {
                return static_cast<functor*>(p)->m_Lambda(std::forward<T_ARG>(args)...);
            }
        };

        using invoker_fnptr   = T_RET(*)(void*, T_ARG...);
        using destroyer_fnptr = void(*)(void*);

        alignas(std::max_align_t) char  m_Storage[sizeof(void*) * T_BUFFER_PTRSIZE_V]{};
        invoker_fnptr                   m_pInvoker      = nullptr;
        destroyer_fnptr                 m_pDestroyer    = nullptr;
    };

    //---------------------------------------------------------------------------------------
    // Compare two functions types
    //---------------------------------------------------------------------------------------
    template< typename T_LAMBDA, typename T_FUNCTION_TYPE >
    struct is_lambda_signature_same
    {
        constexpr static bool value = std::is_same_v
            <
            typename traits< typename std::remove_reference_t<T_LAMBDA> >::func_type,
            T_FUNCTION_TYPE
            >;
    };
    template< typename T_LAMBDA, typename T_FUNCTION_TYPE >
    constexpr static bool is_lambda_signature_same_v = is_lambda_signature_same<T_LAMBDA, T_FUNCTION_TYPE>::value;
}
#endif