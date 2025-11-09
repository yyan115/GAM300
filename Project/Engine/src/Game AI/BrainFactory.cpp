#include "pch.h"
#include "Game AI/BrainFactory.hpp"
#include "Game AI/HfsmBrain.hpp"
#include "Game AI/GruntBrain.hpp"
#include "Game AI/BossBrain.hpp"

namespace game_ai {

    ENGINE_API std::unique_ptr<IBrain> CreateFor(ECSManager& ecs, Entity e, BrainKind kind) {
        e, ecs;
        switch (kind) {
        case BrainKind::Grunt: return std::make_unique<HfsmBrain<GruntFSM>>();
        case BrainKind::Boss:  return std::make_unique<HfsmBrain<BossFSM>>();
        default:               return {};
        }
    }

} // namespace game_ai