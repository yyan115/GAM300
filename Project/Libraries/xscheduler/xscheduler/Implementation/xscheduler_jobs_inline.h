#pragma once
namespace xscheduler
{
    //
    // inline functions
    //
    namespace details
    {
        //-----------------------------------------------------------------------------
        void awaitable_01::await_suspend(std::coroutine_handle<> Handle) noexcept
        {
            auto    cor_handle = std::coroutine_handle<async_job_promise>::from_address(Handle.address());
            auto* current_job = cor_handle.promise().m_pJob;

            assert(current_job != nullptr && "Current job must be set in promise");

            if (auto* sys = current_job->getSystem())
            {
                assert(sys != nullptr && "System must be set for submission");
                sys->SubmitJob(*current_job);
            }
        }

        //-----------------------------------------------------------------------------
        bool awaitable_02::await_suspend(std::coroutine_handle<> Handle) noexcept
        {
            auto    cor_handle = std::coroutine_handle<async_job_promise>::from_address(Handle.address());
            auto* current_job = cor_handle.promise().m_pJob;

            assert(current_job != nullptr && "Current job must be set in promise");

            m_Job.AppendJobToBeTrigger(*current_job);
            if (m_Job.isDone())
            {
                return false; // Resume immediately
            }

            return true; // Suspend
        }
    }

    //-----------------------------------------------------------------------------
    // Out-of-line definitions for async_job
    template<std::size_t T_DEPENDENCY_COUNT_V>
    void async_job<T_DEPENDENCY_COUNT_V>::OnRun(void) noexcept
    {
        if (!this->m_AsyncHandle.m_Handle)
        {
            this->m_AsyncHandle = OnAsyncRun();
        }
        
        if (this->m_AsyncHandle.m_Handle)
        {
            this->m_AsyncHandle.m_Handle.promise().m_pJob = this;
            this->m_AsyncHandle.m_Handle.resume();
        }
    }

    //-----------------------------------------------------------------------------
    // Out-of-line definitions for async_job
    template<std::size_t T_DEPENDENCY_COUNT_V>
    void async_job<T_DEPENDENCY_COUNT_V>::OnNotifyTrigger(xscheduler::system& Sys) noexcept xquatum
    {
        this->OnTriggered();
    }

    //-----------------------------------------------------------------------------
    // Specialization for async_job<0>
    void async_job<0>::OnRun(void) noexcept
    {
        if (!this->m_AsyncHandle.m_Handle)
        {
            this->m_AsyncHandle = OnAsyncRun();
        }

        if (this->m_AsyncHandle.m_Handle)
        {
            this->m_AsyncHandle.m_Handle.promise().m_pJob = this;
            this->m_AsyncHandle.m_Handle.resume();
        }
    }

    //-----------------------------------------------------------------------------
    // Specialization for async_job<0>
    void async_job<0>::OnNotifyTrigger(xscheduler::system& Sys) noexcept xquatum
    {
        // Assert to prevent submission if already associated with a system (adjust if resumption allows)
        assert(this->m_pSystem == nullptr && "Async job already in scheduler; potential double submission");
        OnTriggered();
    }

    //-----------------------------------------------------------------------------
    // Specialization for async_job<0>
    void async_job<0>::OnTriggered(void) noexcept xquatum
    {
        this->m_pSystem->SubmitJob(*this);
    }

    //-----------------------------------------------------------------------------
    // Lambda job
    void details::lambda_job::OnDelete(void) noexcept xquatum
    {
        static_cast<lambda_pool*>(m_pJobPool)->push(*this);
    }

    //-----------------------------------------------------------------------------
    // Out-of-line for lambda_job OnRun
    void details::lambda_job::OnRun() noexcept xquatum
    {
        // Run the function
        std::visit([&]<typename T>(T && V) constexpr noexcept
        {
            if constexpr (std::is_same_v<t3, std::decay_t<T>> || std::is_same_v<t4, std::decay_t<T>>)
            {
                assert(m_Definition.m_IsAsync && "Lambda job must be async for coroutine handling");

                // Only call the lambda the first time
                if (!m_AsyncHandle.m_Handle)
                {
                    m_AsyncHandle = V(*this);
                }

                // Backup for promises...
                if (m_AsyncHandle.m_Handle)
                {
                    m_AsyncHandle.m_Handle.promise().m_pJob = this;
                    m_AsyncHandle.m_Handle.resume();
                }
            }
            else
            {
                V();
            }
        }, m_Func);
    }

    //-----------------------------------------------------------------------------

    void details::final_awaiter::await_suspend(std::coroutine_handle<async_job_promise> Handle) noexcept
    {
        auto* job = Handle.promise().m_pJob;
        assert(job && "Job must be set");
        const bool bDelete = job->getDefinition().m_WhenDone == when_done::DELETE;
        job->OnDone();
        if (bDelete) job->OnDelete();
    }
}