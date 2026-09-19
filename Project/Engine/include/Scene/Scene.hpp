#pragma once
#include <string>
#include "ECS/Entity.hpp"

class IScene {
public:
	IScene() = default;
	IScene(const std::string& path) : scenePath(path) {}
	virtual ~IScene() = default;

	virtual void Initialize() = 0;
	virtual void InitializeJoltPhysics() = 0;
	virtual void InitializePhysics() = 0;
	virtual void Update(double dt) = 0;
	// Runs only the modal prompt rooted at modalRoot and holds the rest of the
	// scene still. Its buttons take presses only when acceptPresses is set.
	virtual void UpdateModal(Entity modalRoot, bool acceptPresses) = 0;
	virtual void Draw() = 0;
	virtual void Exit() = 0;
	virtual void ShutDownPhysics() = 0;
	virtual void initializeOrchestrator() {}

	bool updateSynchronized = true;
	bool drawSynchronized = true;

protected:
	std::string scenePath{};
};