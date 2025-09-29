#pragma once
#include "Reflection/ReflectionBase.hpp"

class IRenderComponent {
public:
	REFL_SERIALIZABLE

	bool isVisible = true;
	int renderOrder = 100;

	IRenderComponent() = default;
	virtual ~IRenderComponent() = default;

};