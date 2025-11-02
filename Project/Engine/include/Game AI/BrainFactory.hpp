#pragma once
#include <memory>
#include "HfsmCommon.hpp"
#include "Brain.hpp"

namespace game_ai {

	// Factory that returns an IBrain for entity `e`
	std::unique_ptr<IBrain> CreateFor(ECSManager& ecs, Entity e);

} // namespace game_ai