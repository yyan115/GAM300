#pragma once
#include "ECS/System.hpp"
#include "FogComponent.hpp"

class FogSystem : public System {
public:
	bool Initialise(bool forceInit = false);
	void Update();
	void Shutdown();

private:
	void InitializeFogComponent(FogVolumeComponent& fogComp);
	bool fogSystemInitialised = false;
};