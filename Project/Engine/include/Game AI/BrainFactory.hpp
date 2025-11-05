#pragma once
#include <memory>
#include "HfsmCommon.hpp"
#include "Brain.hpp"

namespace game_ai {

	// Factory that returns an IBrain for entity `e`
	ENGINE_API std::unique_ptr<IBrain> CreateFor(ECSManager& ecs, Entity e, BrainKind kind);

} // namespace game_ai