#pragma once
#include "Multi-threading/SystemOrchestrator.hpp"
#include "xscheduler/xscheduler.h"
#include "ECS/ECSRegistry.hpp"

class ParallelSystemOrchestrator : public SystemOrchestrator {
public:
    void Update() override;
    void Draw() override;

private:
    xscheduler::system scheduler{ 4 };
};
