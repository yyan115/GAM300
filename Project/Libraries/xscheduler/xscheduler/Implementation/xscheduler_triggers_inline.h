#pragma once
namespace xscheduler
{
    //-----------------------------------------------------------------------------
    // Out-of-line definitions for trigger
    template< int T_MAX_JOBS_V >
    void trigger<T_MAX_JOBS_V>::JobWillNotifyMe(job_base& Job) noexcept
    {
        m_TriggerCounter.fetch_add(1, std::memory_order_relaxed);
        Job.AppendJobToBeTrigger(*this);
    }

    //-----------------------------------------------------------------------------
    // Out-of-line definitions for trigger
    template< int T_MAX_JOBS_V >
    void trigger<T_MAX_JOBS_V>::OnNotifyTrigger(xscheduler::system& Sys) noexcept xquatum
    {
        if (m_TriggerCounter.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            m_pSystem = &Sys;
            OnTriggered();
            OnDone();
        }
    }

    //-----------------------------------------------------------------------------
    // Out-of-line definitions for trigger
    template< int T_MAX_JOBS_V >
    void trigger<T_MAX_JOBS_V>::OnTriggered(void) noexcept xquatum
    {
        // Ok now we can release the list
        std::scoped_lock Ln(m_Mutex);

        // Release all the jobs
        for (auto* pDependent : std::span{ m_DependentJobs.data(), m_DependentCount })
        {
            // are we dealing with a co_routine
            m_pSystem->SubmitJob(*pDependent);
        }
    }

    //-----------------------------------------------------------------------------
    // Out-of-line definitions for trigger
    template< int T_MAX_JOBS_V >
    void trigger<T_MAX_JOBS_V>::OnRun() noexcept xquatum
    {
        assert(m_TriggerCounter == 0 && "Trigger counter should be zero before self-trigger");
        m_TriggerCounter.fetch_add(1, std::memory_order_acq_rel);
        OnNotifyTrigger(*m_pSystem);
    }

    //-----------------------------------------------------------------------------
    // Out-of-line definitions for trigger
    template< int T_MAX_JOBS_V >
    void trigger<T_MAX_JOBS_V>::OnAddDependent(job_base& Dependent) noexcept xquatum
    {
        std::scoped_lock Ln(m_Mutex);
        if (m_pSystem && m_TriggerCounter.load(std::memory_order_relaxed) == 0)
        {
            Dependent.OnNotifyTrigger(*m_pSystem);
        }
        else
        {
            assert(m_pSystem == nullptr && "System should not be set before trigger execution");
            assert(m_DependentCount < m_DependentJobs.size() && "Dependent count exceeds array size");
            m_DependentJobs[m_DependentCount++] = &Dependent;
        }
    }
}