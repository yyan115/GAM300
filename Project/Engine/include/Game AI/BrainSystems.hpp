#pragma once
#include "ECS/ECSManager.hpp"

void RunBrainInitSystem(ECSManager& ecs);
void RunBrainUpdateSystem(ECSManager& ecs, float dt);