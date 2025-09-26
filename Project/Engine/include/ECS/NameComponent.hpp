#pragma once
#include <string>
#include "Reflection/ReflectionBase.hpp"
struct NameComponent
{
	REFL_SERIALIZABLE
	std::string name;

	NameComponent() = default;
	NameComponent(const std::string _name) : name(_name) {}
};