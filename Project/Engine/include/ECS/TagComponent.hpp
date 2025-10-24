#pragma once
#include <string>
#include "Reflection/ReflectionBase.hpp"

struct TagComponent
{
	REFL_SERIALIZABLE
	bool overrideFromPrefab = false;
	int tagIndex = 0; // Index into TagManager's tag list

	TagComponent() = default;
	TagComponent(int index) : tagIndex(index) {}

	// Helper methods
	void SetTag(const std::string& tagName);
	const ENGINE_API std::string& GetTagName() const;
	bool HasTag(const std::string& tagName) const;
};