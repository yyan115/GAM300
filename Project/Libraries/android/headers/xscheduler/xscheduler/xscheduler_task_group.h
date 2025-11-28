#ifndef XSHEDULER_TASK_GROUP_H
#define XSHEDULER_TASK_GROUP_H
#pragma once

namespace xscheduler
{
    //
    // Channel for easy / focus job submission
    //
    class task_group final : public job_base
    {
    public:
        inline          task_group          (const universal_string& GroupName, system& System = xscheduler::g_System, int MaxJobs = -1, job_definition Def = {}) noexcept;
        inline         ~task_group          (void)                          noexcept override;

        template<typename T_LAMBDA> requires std::invocable<T_LAMBDA>
        inline void     Submit              (T_LAMBDA&& Func)               noexcept;

        inline void     join                (void)                          noexcept;

        template< typename T_CONTAINER, typename T_LAMBDA > requires xcontainer::function::is_lambda_signature_same_v< T_LAMBDA, void(std::span<typename T_CONTAINER::value_type>) >
        inline void     ForeachLog          (T_CONTAINER& Container, const std::size_t Divider, const std::size_t Cutoff, T_LAMBDA&& func) noexcept;

        template< typename T_CONTAINER, typename T_LAMBDA > requires xcontainer::function::is_lambda_signature_same_v< T_LAMBDA, void(std::span<typename T_CONTAINER::value_type& >) >
        inline void     ForeachLog          (T_CONTAINER& Container, const std::size_t Divider, const std::size_t Cutoff, T_LAMBDA&& func) noexcept;

        template< typename T_CONTAINER, typename T_LAMBDA > requires xcontainer::function::is_lambda_signature_same_v< T_LAMBDA, void(std::span<typename T_CONTAINER::value_type>) >
        inline void     ForeachFlat         (T_CONTAINER& Container, const std::size_t Divider, T_LAMBDA&& func) noexcept;

        template< typename T_CONTAINER, typename T_LAMBDA > requires xcontainer::function::is_lambda_signature_same_v< T_LAMBDA, void(typename T_CONTAINER::value_type&) >
        inline void     ForeachFlat         (T_CONTAINER& Container, const std::size_t Diviser, T_LAMBDA&& func) noexcept;

        inline int      getJobsInQueue      (void) const noexcept { return m_nJobsInQueue ? m_nJobsInQueue -1 : 0; }

    protected:

        inline void     OnRun               (void)                          noexcept override;
        inline void     OnNotifyTrigger     (xscheduler::system& Sys)       noexcept override;

    private:

        system&                     m_System;
        std::atomic<int>            m_nJobsInQueue{ 1 };        // Initializes with always 1 job in the queue to make sure it does not preemptively exist
        int                         m_MaxJobs;
    };
}

#endif