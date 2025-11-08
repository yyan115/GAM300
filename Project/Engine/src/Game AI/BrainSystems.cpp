#include "pch.h"
#include "Game AI/BrainSystems.hpp"
#include "Game AI/BrainComponent.hpp"
#include "Game AI/BrainFactory.hpp"

void RunBrainInitSystem(ECSManager& ecs) {
    const auto& all = ecs.GetAllEntities();
    for (Entity e : all) {
        if (!ecs.HasComponent<BrainComponent>(e))
            continue;

        auto brainOpt = ecs.TryGetComponent<BrainComponent>(e);
        if (!brainOpt)
            continue;

        BrainComponent& brain = brainOpt->get();

        if (!brain.enabled)            // <-- gate
            continue;

        // Create impl if it's missing (first time or after Stop/Rebuild)
        if (!brain.impl) {
            brain.impl = game_ai::CreateFor(ecs, e, brain.kind);
            // do NOT set started here; let the next block decide
        }

        // Call onEnter exactly once per (re)build
        if (brain.impl && !brain.started) {
            brain.impl->onEnter(ecs, e);
            brain.started = true;
        }
    }
}

void RunBrainUpdateSystem(ECSManager& ecs, float dt) {
    if (Engine::IsEditMode()) {
        return;
    }

    const auto& all = ecs.GetAllEntities();
    for (Entity e : all) {
        if (!ecs.HasComponent<BrainComponent>(e))
            continue;

        auto brainOpt = ecs.TryGetComponent<BrainComponent>(e);
        if (!brainOpt)
            continue;

        BrainComponent& brain = brainOpt->get();

        // Only tick brains that have entered
        if (!brain.impl || !brain.started || !brain.enabled)
            continue;

        brain.impl->onUpdate(ecs, e, dt);
    }
}