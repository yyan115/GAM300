#include "pch.h"
#include "Game AI/BrainFactory.hpp"
#include "Game AI/HfsmBrain.hpp"
#include "Game AI/GruntBrain.hpp"
#include "Game AI/BossBrain.hpp"

namespace game_ai {

    std::unique_ptr<IBrain> CreateFor(ECSManager& ecs, Entity e) {
        // TODO: replace with your real predicate
        const bool isBoss = /* ecs.HasComponent<BossTag>(e) */ false;

        if (isBoss)
            return std::make_unique<HfsmBrain<BossFSM>>();
        else
            return std::make_unique<HfsmBrain<GruntFSM>>();
    }

} // namespace game_ai