#pragma once
#include <string>

class IScene {
public:
	IScene() = default;
	IScene(const std::string& path) : scenePath(path) {}
	virtual ~IScene() = default;

	virtual void Initialize() = 0;
	virtual void InitializeJoltPhysics() = 0;
	virtual void InitializePhysics() = 0;
	virtual void Update(double dt) = 0;
	virtual void Draw() = 0;
	virtual void Exit() = 0;
	virtual void ShutDownPhysics() = 0;

	bool updateSynchronized = true;
	bool drawSynchronized = true;

protected:
	std::string scenePath{};
};