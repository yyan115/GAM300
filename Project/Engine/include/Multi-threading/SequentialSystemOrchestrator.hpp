#pragma once
#include "Multi-threading/SystemOrchestrator.hpp"

class SequentialSystemOrchestrator : public SystemOrchestrator {
public:
	SequentialSystemOrchestrator() = default;
	void Update() override;
	void Draw() override;
};