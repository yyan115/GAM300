#pragma once
#include <UI/Button/LuaEvent.hpp>

class ButtonComponent {
public:
	bool interactable = true;
	LuaEvent onClick;
};