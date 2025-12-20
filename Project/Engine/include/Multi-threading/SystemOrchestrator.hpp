#pragma once
#include "xscheduler/xscheduler.h"

// Abstract orchestrator interface
class SystemOrchestrator {
public:
    virtual ~SystemOrchestrator() = default;

    // Update systems (physics, transform, etc.)
    virtual void Update() = 0;

    // Draw systems (model, text, sprite, etc.)
    virtual void Draw() = 0;
};
