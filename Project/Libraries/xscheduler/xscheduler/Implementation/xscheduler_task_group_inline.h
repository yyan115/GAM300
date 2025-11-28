#pragma once

namespace xscheduler
{
    //------------------------------------------------------------------------------

    task_group::task_group(const universal_string& GroupName, system& System, int MaxJobs, job_definition Def) noexcept
        : job_base(GroupName, [](job_definition& D) { D.m_Complexity = complexity::LIGHT; D.m_WhenDone = when_done::DELETE; return D; }(Def))
        , m_System      { System }
        , m_MaxJobs     { MaxJobs < 0 ? System.getWorkerCount<int>() : MaxJobs }
    {
        assert(m_MaxJobs >= 1);
    }

    //------------------------------------------------------------------------------
    task_group::~task_group()
    {
        // make sure we can join...
        join();
    }
    //------------------------------------------------------------------------------

    template<typename T_LAMBDA> requires std::invocable<T_LAMBDA>
    void task_group::Submit(T_LAMBDA&& Func) noexcept
    {
        job_base& Job = m_System.AllocLambda(*this->m_pName, std::forward<T_LAMBDA>(Func), m_Definition);

        // Make the lambda notify us when it is done...
        Job.AppendJobToBeTrigger(*this);

        // Let us count the number of jobs in the queue
        m_nJobsInQueue.fetch_add(1, std::memory_order_release);

        // Ok let the system work on it...
        m_System.SubmitJob(Job);

        // If we are adding too much work then we should just wait a little
        if ((m_nJobsInQueue.load() >= m_MaxJobs))
        {
            m_System.WorkerStartWorking([&]
            {
                return m_nJobsInQueue.load() >= m_MaxJobs;
            });
        }
    }

    //------------------------------------------------------------------------------

    void task_group::join(void) noexcept
    {
        // User officially called join... so we can remove the initial 1
        if (m_nJobsInQueue.load(std::memory_order_relaxed)) m_nJobsInQueue.fetch_sub(1, std::memory_order_release);

        // If we are adding too much work then we should just wait a little
        if (m_nJobsInQueue.load(std::memory_order_relaxed))
        {
            m_System.WorkerStartWorking([&]
            {
                return m_nJobsInQueue.load(std::memory_order_relaxed);
            });
        }
    }

    //------------------------------------------------------------------------------

    void task_group::OnRun(void) noexcept
    {
        // No one should be running this guy...
        assert(false);
    }

    //------------------------------------------------------------------------------

    template< typename T_CONTAINER, typename T_LAMBDA > requires xcontainer::function::is_lambda_signature_same_v< T_LAMBDA, void(std::span<typename T_CONTAINER::value_type>) >
    void task_group::ForeachLog(T_CONTAINER& Container, const std::size_t Divider, const std::size_t Cutoff, T_LAMBDA&& func) noexcept
    {
        assert(Divider > 0 && "Divider must be positive");
        assert(Cutoff > 0 && "Cutoff must be positive");

        auto        Total = Container.size();
        std::size_t Start = 0;

        // Do the log
        auto R = Total / Divider;
        if (R > Cutoff) do
        {
            Submit([&Container, Start, R, &func]() noexcept
            {
                func(std::span<typename T_CONTAINER::value_type>(Container.data() + Start, R));
            });

            Start += R;
            Total -= R;
            R = Total / Divider;

        } while (R > Cutoff);

        // Do the linear 
        if (Total > 0) do
        {
            R = std::min(Cutoff, Total);
            Submit([&Container, Start, R, &func]() noexcept
            {
                func(std::span<typename T_CONTAINER::value_type>(Container.data() + Start, R));
            });
            Start += R;
            Total -= R;

        } while (Total > 0);
    }

    //------------------------------------------------------------------------------

    template< typename T_CONTAINER, typename T_LAMBDA > requires xcontainer::function::is_lambda_signature_same_v< T_LAMBDA, void(std::span<typename T_CONTAINER::value_type& >) >
    void task_group::ForeachLog(T_CONTAINER& Container, const std::size_t Divider, const std::size_t Cutoff, T_LAMBDA&& func) noexcept
    {
        ForeachLog(Container, Divider, Cutoff, [&func](std::span<typename T_CONTAINER::value_type> View)
            {
                for (auto& E : View)
                {
                    func(E);
                }
            });
    }

    //------------------------------------------------------------------------------
    template< typename T_CONTAINER, typename T_LAMBDA > requires xcontainer::function::is_lambda_signature_same_v< T_LAMBDA, void(std::span<typename T_CONTAINER::value_type>) >
    void task_group::ForeachFlat(T_CONTAINER& Container, const std::size_t Divider, T_LAMBDA&& func) noexcept
    {
        // get ready
        const auto   Total = Container.size();

        if (Total == 0) return;

        assert(Divider);
        assert(Total / Divider);
        std::size_t   Start = 0;
        const auto    nEntries = std::max<std::size_t>(1, Total / Divider);

        // Process all the entries
        do
        {
            const auto Count = std::min(Total - Start, Divider);
            Submit([View = std::span<typename T_CONTAINER::value_type>(Container.data() + Start, Count), &func]()
            {
                func(View);
            });

            Start += Count;
        } while (Start < Total);
    }

    //------------------------------------------------------------------------------
    template< typename T_CONTAINER, typename T_LAMBDA > requires xcontainer::function::is_lambda_signature_same_v< T_LAMBDA, void(typename T_CONTAINER::value_type&) >
    void task_group::ForeachFlat(T_CONTAINER& Container, const std::size_t Diviser, T_LAMBDA&& func) noexcept
    {
        ForeachFlat(Container, Diviser, [&func](std::span<typename T_CONTAINER::value_type>& View)
        {
            for (auto& E : View)
            {
                func(E);
            }
        });
    }

    //------------------------------------------------------------------------------
    void task_group::OnNotifyTrigger(xscheduler::system& Sys) noexcept
    {
        m_nJobsInQueue.fetch_sub(1, std::memory_order_acq_rel);
    }

}