#ifndef XSHEDULER_TRIGGERS_H
#define XSHEDULER_TRIGGERS_H
#pragma once

namespace xscheduler
{
    //
    // Trigger job class
    //
    template< int T_MAX_JOBS_V >
    class trigger : public job_base
    {
    public:
                trigger             (const universal_string& GroupName) noexcept : job_base(GroupName, job_definition::make<priority::LOW>()) {}
        void    JobWillNotifyMe     (job_base& Job)             noexcept;

    protected:
        void OnNotifyTrigger        (xscheduler::system& Sys)   noexcept override xquatum;
        void OnRun                  (void)                      noexcept override xquatum;
        void OnAddDependent         (job_base& Dependent)       noexcept override xquatum;
        void OnTriggered            (void)                      noexcept override;

        std::atomic<std::uint16_t>              m_TriggerCounter        {};
        std::array<job_base*, T_MAX_JOBS_V>     m_DependentJobs         {};
        std::mutex                              m_Mutex                 {};
        std::size_t                             m_DependentCount        {};
    };
}
#endif