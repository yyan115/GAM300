#pragma once
#include "Multi-threading/SystemOrchestrator.hpp"
#include "xscheduler/xscheduler.h"
#include "ECS/ECSRegistry.hpp"

class ParallelSystemOrchestrator : public SystemOrchestrator {
public:
    void Update() override;
    void Draw() override;

private:
#ifdef ANDROID
    // Two worker threads cover the heavy frame jobs without competing with
    // Jolt's nested worker pool on mobile big.LITTLE CPUs.
    xscheduler::system scheduler{ 3 };
#else
    xscheduler::system scheduler{ 5 };
#endif
};
