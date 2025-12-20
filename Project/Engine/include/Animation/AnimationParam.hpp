#pragma once
#include <pch.h>
#include <string>
#include <unordered_map>
#include <variant>

enum class AnimParamType
{
	Bool,
	Int,
	Float,
	Trigger
};

struct AnimParam
{
	AnimParamType type;
	std::variant<bool, int, float> value;
	bool consumed = false; // For Trigger type to indicate if it has been consumed
};

class AnimParamSet
{
public:
	// Setters
	void SetBool(const std::string& name, bool v)	{	Set(name, AnimParamType::Bool, v);	}
	void SetInt(const std::string& name, int i)		{	Set(name, AnimParamType::Int, i);	}
	void SetFloat(const std::string& name, float f) {	Set(name, AnimParamType::Float, f); }

	void SetTrigger(const std::string& name)
	{
		Set(name, AnimParamType::Trigger, true);
		mParams[name].consumed = false;
	}

	// Getters
	bool GetBool(const std::string& name, bool def = false) const
	{
		auto it = mParams.find(name);
		if(it == mParams.end() || it->second.type != AnimParamType::Bool)
			return def;
		return std::get<bool>(it->second.value);
	}

	int GetInt(const std::string& name, int def = 0) const
	{
		auto it = mParams.find(name);
		if(it == mParams.end() || it->second.type != AnimParamType::Int)
			return def;
		return std::get<int>(it->second.value);
	}

	float GetFloat(const std::string& name, float def = 0.0f) const
	{
		auto it = mParams.find(name);
		if(it == mParams.end() || it->second.type != AnimParamType::Float)
			return def;
		return std::get<float>(it->second.value);
	}

	bool GetTrigger(const std::string& name)
	{
		auto it = mParams.find(name);
		if(it == mParams.end() || it->second.type != AnimParamType::Trigger)
			return false;
		if(it->second.consumed)
			return false;
		it->second.consumed = true;
		return true;
	}

private:
	void Set(const std::string& name, AnimParamType type, std::variant<bool, int, float> value)
	{
		AnimParam p;
		p.type = type;
		p.value = value;
		p.consumed = false;
		mParams[name] = p;
	}

	mutable std::unordered_map<std::string, AnimParam> mParams;
};