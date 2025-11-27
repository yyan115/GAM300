#pragma once
#include "ECS/System.hpp"
#include "UI/Button/ButtonComponent.hpp"
#include "Math/Vector3D.hpp"

class ButtonSystem : public System {
public:
	ButtonSystem() = default;
	~ButtonSystem() = default;

	void Initialise();
	void Update();
	void Shutdown();

	void OnClickAddListener(ButtonComponent& button, lua_State* L, int funcIndex);

private:
	void HandleMouseClick(Entity buttonEntity, Vector3D mousePos);
	bool HitTest(Entity buttonEntity, Vector3D mousePos);
};