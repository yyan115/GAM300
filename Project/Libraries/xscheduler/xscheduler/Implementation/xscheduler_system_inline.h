#pragma once

namespace xscheduler
{
    //-------------------------------------------------------------------------

    system::system(int NWorkers) noexcept
    {
        Init(NWorkers);
    }

    //-------------------------------------------------------------------------

    system::~system() noexcept
    {
        Shutdown();
    }

    //-------------------------------------------------------------------------

    void system::Shutdown() noexcept
    {
        if (m_State != state::WORKING) return;

        // OK Time to exit
        m_Exit = true;
        m_State = state::EXITING;
        m_SleepWorkerCV.notify_all();

        // Main thread exists officially
        m_ReadyWorkers.fetch_sub(1, std::memory_order_release);

        // Let all other workers also exit
        for (auto i = 1u; i < m_Workers.size(); ++i)
        {
            if (m_Workers[i].joinable()) m_Workers[i].join();
        }

        // Wait for everyone to exit
        while (m_ReadyWorkers.load() != 0)
        {
            // Waiting...
            std::this_thread::yield();
        }

        // Officially destroy all the workers
        m_Workers.Delete();
        m_State = state::DONE;
    }

    //-------------------------------------------------------------------------

    void system::SubmitJob(job_base& Job) noexcept xquatum
    {
        // Only async jobs can be resubmitted 
        assert(Job.m_Definition.m_IsAsync || Job.m_pSystem == nullptr && "Job already submitted to a system");

        // If it is an async job, and it is being resubmitted, then the system better match!
        assert((!Job.m_Definition.m_IsAsync || Job.m_Definition.m_IsAsync && Job.m_pSystem == nullptr || Job.m_Definition.m_IsAsync && Job.m_pSystem == this) && "Job already submitted to a system");

        // Let the job know that it is officially registered with this system
        Job.m_pSystem = this;

        // Back up this just in case our job disappears after we insert it in the queue
        const bool bForTheMainThread = Job.m_Definition.m_Affinity == affinity::MAIN_THREAD;

        // Insert the job in the queue
        auto& Queue = getQueue(Job.m_Definition);
        Queue.push(&Job);

        // Now lets us notify our guys just in case they are sleeping...
        if (bForTheMainThread)  m_SleepWorkerCV.notify_all();
        else                    m_SleepWorkerCV.notify_one();
    }

    //-------------------------------------------------------------------------

    template<typename T_LAMBDA>
    void system::WorkerStartWorking(T_LAMBDA&& Lambda) noexcept
    {
        auto& Kit = m_WorkerKits[thread_id_v];

        while (Lambda())
        {
            job_base* pJob = getJob(Kit);
            if (pJob)
            {
                WorkerDoJob(*pJob);
            }
        }
    }

    //-------------------------------------------------------------------------

    template<typename T_FUNCTION> requires (std::invocable<T_FUNCTION> || std::invocable<T_FUNCTION, job_base&>)
    void system::SubmitLambda(const universal_string& Name, T_FUNCTION&& Func, complexity Complexity, priority Priority, affinity Affinity) noexcept xquatum
    {
        auto& Kit = m_WorkerKits[thread_id_v];
        auto& Job = *Kit.m_JobPool.pop(Name, std::forward<T_FUNCTION>(Func), &Kit.m_JobPool);
        Job.m_Definition.m_Complexity = Complexity;
        Job.m_Definition.m_Priority = Priority;
        Job.m_Definition.m_Affinity = Affinity;

        if constexpr (std::invocable<T_FUNCTION, job_base&>) Job.m_Definition.m_IsAsync = true;
        else                                                 Job.m_Definition.m_IsAsync = false;
        SubmitJob(Job);
    }

    //-------------------------------------------------------------------------

    template<typename T_FUNCTION> requires (std::invocable<T_FUNCTION> || std::invocable<T_FUNCTION, job_base&>)
    job_base& system::AllocLambda(const universal_string& Name, T_FUNCTION&& Func, job_definition Definition) noexcept xquatum
    {
        auto& Kit = m_WorkerKits[thread_id_v];
        auto& Job = *Kit.m_JobPool.pop(Name, std::forward<T_FUNCTION>(Func), &Kit.m_JobPool);

        Job.m_Definition = Definition;

        if constexpr (std::invocable<T_FUNCTION, job_base&>) Job.m_Definition.m_IsAsync = true;
        else                                                 Job.m_Definition.m_IsAsync = false;

        return Job;
    }

    //-------------------------------------------------------------------------

    template<std::integral T>
    [[nodiscard]] T system::getWorkerCount() const noexcept xquatum
    {
        return static_cast<T>(m_WorkerKits.size());
    }

    //-------------------------------------------------------------------------

    void system::Init(int NWorkers) noexcept
    {
        assert(m_State == state::NOT_INITIALIZE && "System already initialized");
        auto WorkerCount = NWorkers <= 0 ? std::thread::hardware_concurrency() : NWorkers;

        // Allocate workers
        m_Workers.New(WorkerCount);
        m_WorkerKits.New(WorkerCount);

        // Main thread is always the zero entry worker
        if (++m_ReadyWorkers == static_cast<int>(m_Workers.size()))
        {
            // OK we are on...
            m_State = state::WORKING;
        }

        // Get all other workers ready
        for (auto i = 1u; i < WorkerCount; ++i)
        {
            m_Workers[i] = std::jthread([this, Index = static_cast<int>(i)]
                {
                    // Set the worker name
                    setWorkerName(std::format(L"WORKER: {}", Index).c_str());

                    // Officially register my ID
                    thread_id_v = Index;

                    //
                    // Let the world know I am alive
                    //
                    if (++m_ReadyWorkers == static_cast<int>(m_Workers.size()))
                    {
                        // OK we are on...
                        m_State = state::WORKING;
                    }
                    else
                    {
                        // Wait for everyone to get ready
                        while (m_State.load() != state::WORKING)
                        {
                            std::this_thread::yield();
                        }
                    }

                    // Work like a good boy
                    WorkerLoop(m_WorkerKits[Index], m_Exit);

                    // I am dead...
                    --m_ReadyWorkers;
                });
        }

        //
        // Wait for every one to get ready
        //
        setWorkerName(std::format(L"WORKER (Main): {}", 0).c_str());
        thread_id_v = 0;
        while (m_State.load() != state::WORKING)
        {
            std::this_thread::yield();
        }
    }

    //-------------------------------------------------------------------------

    void system::setWorkerName(const wchar_t* pName) noexcept
    {
#ifdef _WIN32
        SetThreadDescription(GetCurrentThread(), pName);
#elif __linux__
        pthread_setname_np(pthread_self(), pName);
#elif __APPLE__
        pthread_setname_np(pName);
#endif
    }

    //-------------------------------------------------------------------------

    job_base* system::getLightJob(worker_kit& Kit) noexcept xquatum
    {
        job_base* pJob = nullptr;

        // Are we the main thread?
        if (thread_id_v == 0)
        {
            if (Kit.m_LightJobQueue[static_cast<int>(affinity::MAIN_THREAD)].pop(pJob)) return pJob;
            if (Kit.m_LightJobQueue[static_cast<int>(affinity::ANY)].pop(pJob)) return pJob;

            for (auto i = 0u; i < m_WorkerKits.size(); ++i)
            {
                if (auto& NewKit = m_WorkerKits[Kit.m_iNextKit]; &NewKit != &Kit)
                {
                    if (NewKit.m_LightJobQueue[static_cast<int>(affinity::ANY)].pop(pJob)) return pJob;
                }

                // Otherwise just keep searching
                Kit.m_iNextKit = (Kit.m_iNextKit + 1) % m_WorkerKits.size();
            }
        }
        else
        {
            if (Kit.m_LightJobQueue[static_cast<int>(affinity::NOT_MAIN_THREAD)].pop(pJob)) return pJob;
            if (Kit.m_LightJobQueue[static_cast<int>(affinity::ANY)].pop(pJob)) return pJob;
            for (auto i = 0u; i < m_WorkerKits.size(); ++i)
            {
                if (auto& NewKit = m_WorkerKits[Kit.m_iNextKit]; &NewKit != &Kit)
                {
                    if (NewKit.m_LightJobQueue[static_cast<int>(affinity::NOT_MAIN_THREAD)].pop(pJob)) return pJob;
                    if (NewKit.m_LightJobQueue[static_cast<int>(affinity::ANY)].pop(pJob)) return pJob;
                }

                // Otherwise just keep searching
                Kit.m_iNextKit = (Kit.m_iNextKit + 1) % m_WorkerKits.size();
            }
        }
        return pJob;
    }

    //-------------------------------------------------------------------------

    job_base* system::getJob(worker_kit& Kit) noexcept xquatum
    {
        job_base* pJob = nullptr;

        //
        // We always prioritize small jobs first
        //
        if ((pJob = getLightJob(Kit)) != nullptr) return pJob;
        constexpr static auto QueOrder = std::array{ static_cast<int>(priority::HIGH)
                                                   , static_cast<int>(priority::NORMAL)
                                                   , static_cast<int>(priority::LOW) };

        // Are we the main thread?
        if (thread_id_v == 0)
        {
            for (const int iPriority : QueOrder)
            {
                if (m_JobQueue[iPriority][static_cast<int>(affinity::MAIN_THREAD)].pop(pJob)) return pJob;
                if (m_JobQueue[iPriority][static_cast<int>(affinity::ANY)].pop(pJob)) return pJob;
            }
        }
        else
        {
            for (const int iPriority : QueOrder)
            {
                if (m_JobQueue[iPriority][static_cast<int>(affinity::NOT_MAIN_THREAD)].pop(pJob)) return pJob;
                if (m_JobQueue[iPriority][static_cast<int>(affinity::ANY)].pop(pJob)) return pJob;
            }
        }

        return pJob;
    }

    //-------------------------------------------------------------------------

    void system::WorkerDoJob(job_base& Job) const noexcept
    {
        //
        // Check if we need to handle a coroutine
        // If it is a regular job it is easier...
        //
        if (Job.m_Definition.m_IsAsync)
        {
            auto& Handle = Job.m_AsyncHandle.m_Handle;
            assert(Job.m_pSystem != nullptr && "Async job must have system set");

            if (!Handle)
            {
                // First run including initial resume
                Job.OnRun();
            }
            else if (!Handle.done())
            {
                assert(Handle.address() != nullptr && "Invalid coroutine handle");

                // Resume coroutine
                Handle.resume();
            }

            // Final suspend handles OnDone/OnDelete!!
        }
        else
        {
            Job.OnRun();

            const bool bDeleteWhenDone = Job.m_Definition.m_WhenDone == when_done::DELETE;
            Job.OnDone();
            if (bDeleteWhenDone)
            {
                Job.OnDelete();
            }
            else
            {
                Job.OnReset();
            }
        }
    }

    //-------------------------------------------------------------------------

    void system::WorkerLoop(worker_kit& Kit, std::atomic<bool>& Exit) noexcept
    {
        std::unique_lock<std::mutex> Lock(m_SleepWorkerMutex, std::defer_lock);

        while (Exit.load(std::memory_order_relaxed) == false)
        {
            job_base* pJob = getJob(Kit);
            if (pJob == nullptr)
            {
                Lock.lock();
                m_SleepWorkerCV.wait(Lock, [&pJob, &Kit, &Exit, this]()
                    {
                        return Exit.load(std::memory_order_relaxed) || !!(pJob = getJob(Kit));
                    });
                Lock.unlock();
            }

            //
            // Check if someone just wake us up for more work....
            // Or if we have work from the queue
            //
            if (pJob)
            {
                // Let the worker get busy
                WorkerDoJob(*pJob);
            }
        }
    }

    //-------------------------------------------------------------------------

    system::queue& system::getQueue(const job_definition Definition) noexcept xquatum
    {
        if (Definition.m_Complexity == complexity::LIGHT)
        {
            switch (Definition.m_Affinity)
            {
            case affinity::MAIN_THREAD:         return m_WorkerKits[0].m_LightJobQueue[static_cast<int>(affinity::MAIN_THREAD)];
            case affinity::ANY:                 return m_WorkerKits[thread_id_v].m_LightJobQueue[static_cast<int>(affinity::ANY)];
            case affinity::NOT_MAIN_THREAD:
                if (thread_id_v == 0)
                {
                    assert(m_WorkerKits.size() > 1 && "No worker kits available for NOT_MAIN_THREAD");
                    return m_WorkerKits[1].m_LightJobQueue[static_cast<int>(affinity::NOT_MAIN_THREAD)];
                }
                else
                {
                    return m_WorkerKits[thread_id_v].m_LightJobQueue[static_cast<int>(affinity::NOT_MAIN_THREAD)];
                }
            default: assert(false && "Invalid affinity");
            }
        }
        return m_JobQueue[static_cast<int>(Definition.m_Priority)][static_cast<int>(Definition.m_Affinity)];
    }
}