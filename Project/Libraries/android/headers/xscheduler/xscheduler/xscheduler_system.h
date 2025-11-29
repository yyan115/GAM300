#ifndef XSHEDULER_SYSTEM_H
#define XSHEDULER_SYSTEM_H
#pragma once

namespace xscheduler
{
    //
    // Scheduler system class
    //
    static thread_local int thread_id_v = 0;
    class system
    {
    public:

        inline                  system              (void)                                                  noexcept = default;

        inline                  system              (int NWorkers)                                          noexcept;

        inline void             Init                (int NWorkers = -1)                                     noexcept;

        inline                 ~system              (void)                                                  noexcept;

        inline void             Shutdown            (void)                                                  noexcept;

        inline void             SubmitJob           (job_base& Job)                                         noexcept xquatum;

        template<typename T_LAMBDA>
        inline void             WorkerStartWorking  (T_LAMBDA&& Lambda)                                     noexcept;

        template<typename T_FUNCTION> requires (std::invocable<T_FUNCTION> || std::invocable<T_FUNCTION, job_base&>)
        inline void             SubmitLambda        (const universal_string& Name, T_FUNCTION&& Func, complexity Complexity = complexity::LIGHT, priority Priority = priority::NORMAL, affinity Affinity = affinity::ANY) noexcept xquatum;

        template<typename T_FUNCTION> requires (std::invocable<T_FUNCTION> || std::invocable<T_FUNCTION, job_base&>)
        job_base&               AllocLambda         (const universal_string& Name, T_FUNCTION&& Func, job_definition Definition) noexcept xquatum;

        template<std::integral T = int>
        inline T  getWorkerCount      (void) const                                            noexcept xquatum;

    protected:

        enum class state : std::uint8_t
        { NOT_INITIALIZE
        , WORKING
        , EXITING
        , DONE
        };

        using queue = xcontainer::queue::mpmc_bounded<job_base*, 1024>;
        using affinity_job_queue_array = std::array< queue, static_cast<int>(affinity::ENUM_COUNT) >;
        using priority_jobs_queue_array = std::array< affinity_job_queue_array, static_cast<int>(priority::ENUM_COUNT) >;
        using lambda_pool = details::lambda_pool;

        struct worker_kit
        {
            affinity_job_queue_array    m_LightJobQueue {};
            lambda_pool                 m_JobPool       {};
            std::uint16_t               m_iNextKit      {};
        };

        inline void                 setWorkerName       (const wchar_t* pName)                              noexcept;
        inline job_base*            getLightJob         (worker_kit& Kit)                                   noexcept xquatum;
        inline job_base*            getJob              (worker_kit& Kit)                                   noexcept xquatum;
        inline void                 WorkerDoJob         (job_base& Job)                             const   noexcept;
        inline void                 WorkerLoop          (worker_kit& Kit, std::atomic<bool>& Exit)          noexcept;
        inline queue&               getQueue            (const job_definition Definition)                   noexcept xquatum;

        priority_jobs_queue_array               m_JobQueue{};
        xcontainer::unique_ptr<worker_kit[]>    m_WorkerKits{};
        xcontainer::unique_ptr<std::jthread[]>  m_Workers{};
        std::mutex                              m_SleepWorkerMutex{};
        std::condition_variable                 m_SleepWorkerCV{};
        std::atomic<state>                      m_State{ state::NOT_INITIALIZE };
        std::atomic<bool>                       m_Exit{ false };
        std::atomic<int>                        m_ReadyWorkers{ 0 };
        std::atomic<bool>                       m_MainThreadShouldWork{ false };
    };

    //
    // Add a global instance (But let the user initialize it)
    //
    inline system g_System;
}

#endif