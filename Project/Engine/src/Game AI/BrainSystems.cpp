#include "pch.h"
#include "Game AI/BrainSystems.hpp"
#include "Game AI/Brain.hpp"
#include "Game AI/BrainFactory.hpp"

void RunBrainInitSystem(ECSManager& ecs) {
    const auto& all = ecs.GetAllEntities();
    for (Entity e : all) {
        if (!ecs.HasComponent<Brain>(e)) continue;

        auto brainOpt = ecs.TryGetComponent<Brain>(e);   // your API: optional<reference_wrapper<T>>
        if (!brainOpt) continue;
        Brain& brain = brainOpt->get();

        if (!brain.impl)
            brain.impl = game_ai::CreateFor(ecs, e, brain.kind);

        if (brain.impl && !brain.started) {
            brain.impl->onEnter(ecs, e);
            brain.started = true;
        }
    }
}

void RunBrainUpdateSystem(ECSManager& ecs, float dt) {
    const auto& all = ecs.GetAllEntities();
    for (Entity e : all) {
        if (!ecs.HasComponent<Brain>(e)) continue;

        auto brainOpt = ecs.TryGetComponent<Brain>(e);
        if (!brainOpt) continue;
        Brain& brain = brainOpt->get();

        if (!brain.impl || !brain.started) continue;
        brain.impl->onUpdate(ecs, e, dt);
		brain.activeState = brain.impl->activeStateName();
    }
}