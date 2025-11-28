#ifndef XSHEDULER_JOBS_H
#define XSHEDULER_JOBS_H
#pragma once

namespace xscheduler
{
    // Enums for job properties
    enum class affinity : std::uint8_t
    { ANY
    , MAIN_THREAD
    , NOT_MAIN_THREAD
    , ENUM_COUNT
    };

    enum class priority : std::uint8_t
    { NORMAL
    , LOW
    , HIGH
    , ENUM_COUNT
    };

    enum class complexity : std::uint8_t
    { NORMAL
    , LIGHT
    , HEAVY
    };

    enum class when_done : std::uint8_t
    { DO_NOTHING
    , DELETE
    };

    //
    // Job definition bitfield
    //
    struct job_definition
    {
        when_done   m_WhenDone      : 1;
        complexity  m_Complexity    : 2;
        affinity    m_Affinity      : 2;
        priority    m_Priority      : 2;
        bool        m_IsAsync       : 1;

        template <auto T_V>
        consteval static std::uint8_t make_mask(void) noexcept
        {
                 if constexpr (std::is_same_v<std::decay_t<decltype(T_V)>, when_done>)  return static_cast<std::uint8_t>(T_V) << (0);
            else if constexpr (std::is_same_v<std::decay_t<decltype(T_V)>, complexity>) return static_cast<std::uint8_t>(T_V) << (1);
            else if constexpr (std::is_same_v<std::decay_t<decltype(T_V)>, affinity>)   return static_cast<std::uint8_t>(T_V) << (2 + 1);
            else if constexpr (std::is_same_v<std::decay_t<decltype(T_V)>, priority>)   return static_cast<std::uint8_t>(T_V) << (2 + 2 + 1);
            else                                                                        return 0;
        }

        consteval static job_definition make_from_mask (std::uint8_t value) noexcept
        {
            job_definition def{};
            def.m_WhenDone   = static_cast<when_done> (value >> (0));
            def.m_Complexity = static_cast<complexity>(value >> (1));
            def.m_Affinity   = static_cast<affinity>  (value >> (2 + 1));
            def.m_Priority   = static_cast<priority>  (value >> (2 + 2 + 1));
            def.m_IsAsync    = false; 
            return def;
        }

        template <auto... T_ARGS_V>
        consteval static job_definition make(void) noexcept { return make_from_mask((make_mask<T_ARGS_V>() | ...)); }

        inline std::string ToString(void) const noexcept
        {
            return std::format( "definition {{ WhenDone: {}, Complexity: {}, Affinity: {}, Priority: {}, IsAsync: {} }}",
                static_cast<int>(m_WhenDone),
                static_cast<int>(m_Complexity),
                static_cast<int>(m_Affinity),
                static_cast<int>(m_Priority),
                m_IsAsync ? "true" : "false"
            );
        }
    };

    //
    // Coroutine task type
    //
    struct async_job_promise;
    struct async_handle
    {
        using promise_type = async_job_promise;

                        async_handle            (void)                                          noexcept = default;
                        async_handle            (const async_handle&)                           noexcept = delete;
        async_handle&   operator=               (const async_handle&)                           noexcept = delete;
                        async_handle            (async_handle&& other)                          noexcept : m_Handle{ std::exchange(other.m_Handle, {}) } {}
                        async_handle            (std::coroutine_handle<promise_type> Handle)    noexcept : m_Handle{ Handle } {}
                       ~async_handle            (void)                                          noexcept { if (m_Handle) m_Handle.destroy(); }
        async_handle&   operator=               (async_handle&& other)                          noexcept
        {
            if (this != &other)
            {
                if (m_Handle) m_Handle.destroy();
                m_Handle = std::exchange(other.m_Handle, {});
            }
            return *this;
        }

        std::coroutine_handle<promise_type> m_Handle;
    };

    //
    // Base job class
    //
    namespace details{ struct final_awaiter; }
    class job_base
    {
    public:
        constexpr                       job_base            (const universal_string& Name, job_definition Def) noexcept : m_Definition{Def}, m_pName{&Name}{}
        constexpr                       job_base            (void)                      noexcept = default;
        virtual                        ~job_base            (void)                      noexcept = default;
        job_base&                       setupDefinition     (job_definition Def)        noexcept { m_Definition = Def; return *this; }
        constexpr job_definition getDefinition(void)              const   noexcept xquatum { return m_Definition; }
        [[nodiscard]] auto&             getAsyncHandle      (void)                      noexcept xquatum { return m_AsyncHandle; }
        void                            AppendJobToBeTrigger(job_base& Dependent)       noexcept xquatum { OnAddDependent(Dependent); }
        [[nodiscard]] bool              isDone              (void)              const   noexcept xquatum { return m_isDone.load(std::memory_order_acquire); }
        void                            Wait                (std::chrono::milliseconds ms = {}) noexcept xquatum { std::this_thread::yield(); if (ms.count() > 0) std::this_thread::sleep_for(ms); else std::this_thread::yield(); }
        xscheduler::system*             getSystem           (void)                      noexcept xquatum { return m_pSystem; }
        virtual void                    OnNotifyTrigger     (xscheduler::system& Sys)   noexcept xquatum { OnTriggered(); }

    protected:

        virtual void                    OnAddDependent      (job_base& Dependent)       noexcept { assert(false && "Cannot add dependent to base job"); }
        virtual void                    OnTriggered         (void)                      noexcept xquatum { /* Default: do nothing */ }
        virtual void                    OnRun               (void)                      noexcept xquatum = 0;
        virtual void                    OnDone              (void)                      noexcept xquatum { m_isDone.store(true, std::memory_order_release); }
        virtual void                    OnDelete            (void)                      noexcept xquatum { delete this; }
        virtual void                    OnReset             (void)                      noexcept xquatum { m_isDone.store( false, std::memory_order_relaxed); m_pSystem = nullptr; m_AsyncHandle = async_handle{}; }

    protected:
        async_handle            m_AsyncHandle       {};
        std::atomic<bool>       m_isDone            { false };
        system*                 m_pSystem           { nullptr };
        job_definition          m_Definition        {};
        const universal_string* m_pName             { nullptr };

        friend class system;
        friend struct details::final_awaiter;
    };

    //
    // Job template
    //
    template<std::size_t T_DEPENDENCY_COUNT_V>
    class job : public job_base
    {
    public:
        constexpr               job             (const universal_string& Name, job_definition Def={}) noexcept : job_base{ Name, Def } {}

    protected:
        void                    OnDone          (void)                      noexcept { OnNotifyTrigger(*m_pSystem); job_base::OnDone(); }
        void                    OnAddDependent  (job_base& Dependent)       noexcept override xquatum { assert(m_DependentCount < T_DEPENDENCY_COUNT_V && "Dependent count exceeds limit"); m_DependentJobs[m_DependentCount++] = &Dependent; }
        void                    OnTriggered     (void)                      noexcept override xquatum { for (auto* pDependent : std::span{ m_DependentJobs.data(), m_DependentCount }) pDependent->OnNotifyTrigger(*m_pSystem); }

    protected:
        std::array<job_base*, T_DEPENDENCY_COUNT_V> m_DependentJobs{};
        std::size_t                                 m_DependentCount{ 0 };
    };

    //
    // Job that does not need to notify anyone...
    //
    template<>
    class job<0> : public job_base
    {
    public:
        constexpr               job             (const universal_string& Name, job_definition Def={}) noexcept : job_base{ Name, Def } {}

    protected:
        void                    OnAddDependent  (job_base& Dependent)       noexcept override { assert(false && "Cannot add dependent to job<0>"); }
    };

    //
    // Async Job template
    //
    template<std::size_t T_DEPENDENCY_COUNT_V>
    class async_job : public job<T_DEPENDENCY_COUNT_V>
    {
    public:
        constexpr               async_job       (const universal_string& Name, job_definition Def={}) noexcept : job<T_DEPENDENCY_COUNT_V>{ Name, [&]{Def.m_IsAsync = true; return Def; }() } {}

    protected:
        virtual async_handle    OnAsyncRun      (void)                      noexcept = 0;
        void                    OnRun           (void)                      noexcept final;
        inline void             OnNotifyTrigger (xscheduler::system& Sys)   noexcept override xquatum;
    };

    //
    // Async Job that does not need to notify anyone...
    //
    template<>
    class async_job<0> : public job_base
    {
    public:
        constexpr               async_job       (const universal_string& Name, job_definition Def={}) noexcept : job_base{ Name, [&] {Def.m_IsAsync = true; return Def; }() } {}

    protected:
        virtual void            OnDone          (void)                      noexcept xquatum { OnTriggered(); m_isDone.store(true, std::memory_order_release); }
        virtual async_handle    OnAsyncRun      (void)                      noexcept = 0;
        inline void             OnRun           (void)                      noexcept final;
        inline void             OnNotifyTrigger (xscheduler::system& Sys)   noexcept override xquatum;
        inline void             OnTriggered     (void)                      noexcept override xquatum;
        void                    OnAddDependent  (job_base& Dependent)       noexcept override { assert(false && "Cannot add dependent to async_job<0>"); }
    };

    //
    // Coroutine task type
    //
    namespace details
    {
        struct awaitable_01
        {
            job_base& m_Job;
                        awaitable_01        (job_base& Job)                     noexcept : m_Job{ Job } {}
            bool        await_ready         (void)                              noexcept { return m_Job.isDone(); }
            void        await_resume        (void)                              noexcept {}
            inline void await_suspend       (std::coroutine_handle<> Handle)    noexcept;
        };

        struct awaitable_02
        {
            job_base& m_Job;
            bool        await_ready         (void)                              noexcept { return m_Job.isDone(); }
            void        await_resume        (void)                              noexcept {}
            inline bool await_suspend       (std::coroutine_handle<> Handle)    noexcept;
        };

        struct final_awaiter
        {
            bool        await_ready         (void)                                              noexcept { return false; }
            void        await_resume        (void)                                              noexcept {}
            inline void await_suspend       (std::coroutine_handle<async_job_promise> Handle)   noexcept;
        };
    }

    struct async_job_promise
    {
        job_base* m_pJob{ nullptr };

        async_handle            get_return_object       (void)              noexcept { return { std::coroutine_handle<async_job_promise>::from_promise(*this) }; }
        std::suspend_always     initial_suspend         (void)              noexcept { return {}; }
        void                    return_void             (void)              noexcept {}
        void                    unhandled_exception     (void)              noexcept {}
        auto                    yield_value             (job_base& Job)     noexcept { return details::awaitable_01{ Job }; }
        auto                    await_transform         (job_base& Job)     noexcept { return details::awaitable_02{ Job };}
        auto                    final_suspend           (void)              noexcept { return details::final_awaiter{}; }
    };

    //
    // Details Private 
    //
    namespace details
    {
        //
        // Lambda Job
        //
        struct lambda_job final : job<1>
        {
            using t1    = xcontainer::function::buffer<8, void(void)>;
            using t2    = std::function<void(void)>;
            using t3    = xcontainer::function::buffer<8, async_handle(job_base&)>;
            using t4    = std::function<async_handle(job_base&)>;
            using data  = std::variant< t1, t2, t3, t4 >;

            data        m_Func;
            void*       m_pJobPool;

            template<typename T>
            constexpr static auto make_data(T&& func) noexcept
            {
                using decayed_t = std::decay_t<T>;
                using ret_t     = typename xcontainer::function::traits<decayed_t>::return_type;

                if constexpr (std::is_same_v< ret_t, async_handle >)
                {
                    if constexpr (t3::doesItFit<decayed_t>())   return data{ std::in_place_type<t3>, std::move(func) };
                    else                                        return data{ std::in_place_type<t4>, std::move(func) };
                }
                else
                {
                    if constexpr (t1::doesItFit<decayed_t>())   return data{ std::in_place_type<t1>, std::move(func) };
                    else                                        return data{ std::in_place_type<t2>, std::move(func) };
                }
            }

            template<typename T_LAMBDA>
            constexpr           lambda_job      (const universal_string& Name, T_LAMBDA&& Func, void* pPool)  noexcept : job<1>(Name), m_Func(make_data(std::forward<T_LAMBDA>(Func))), m_pJobPool(pPool){}
            inline void         OnRun           (void)                          noexcept override xquatum;
            inline void         OnDelete        (void)                          noexcept override xquatum;
        };

        using lambda_pool = xcontainer::pool::mpmc_bounded_jitc<details::lambda_job, 1024>;
    }
}
#endif