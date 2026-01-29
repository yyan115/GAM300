#pragma once
#include <pch.h>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// DLL export/import macro
#ifndef ENGINE_API
    #ifdef _WIN32
        #ifdef ENGINE_EXPORTS
            #define ENGINE_API __declspec(dllexport)
        #else
            #define ENGINE_API __declspec(dllimport)
        #endif
    #else
        #if __GNUC__ >= 4
            #define ENGINE_API __attribute__((visibility("default")))
        #else
            #define ENGINE_API
        #endif
    #endif
#endif

enum class AnimParamType
{
	Bool,
	Int,
	Float,
	Trigger
};

// Comparison modes for conditions (serializable)
enum class AnimConditionMode
{
	Equals,         // ==
	NotEquals,      // !=
	Greater,        // >
	Less,           // <
	GreaterOrEqual, // >=
	LessOrEqual,    // <=
	TriggerFired    // For triggers - just checks if trigger was set
};

// Serializable condition structure (replaces lambda)
struct ENGINE_API AnimCondition
{
	std::string paramName;
	AnimConditionMode mode = AnimConditionMode::Equals;
	float threshold = 0.0f;  // Used for numeric comparisons, or 1.0 for true, 0.0 for false
};

struct ENGINE_API AnimParam
{
	AnimParamType type;
	std::variant<bool, int, float> value;
	bool consumed = false; // For Trigger type to indicate if it has been consumed
};

class ENGINE_API AnimParamSet
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

	// Check trigger without consuming it (for editor preview)
	bool PeekTrigger(const std::string& name) const
	{
		auto it = mParams.find(name);
		if(it == mParams.end() || it->second.type != AnimParamType::Trigger)
			return false;
		return !it->second.consumed;
	}

	// Editor support - enumerate all parameters
	const std::unordered_map<std::string, AnimParam>& GetAllParams() const { return mParams; }

	// Check if parameter exists
	bool HasParam(const std::string& name) const { return mParams.find(name) != mParams.end(); }

	// Get parameter type
	AnimParamType GetParamType(const std::string& name) const
	{
		auto it = mParams.find(name);
		if(it == mParams.end()) return AnimParamType::Bool;
		return it->second.type;
	}

	// Add a new parameter with default value
	void AddParam(const std::string& name, AnimParamType type)
	{
		switch(type)
		{
			case AnimParamType::Bool: SetBool(name, false); break;
			case AnimParamType::Int: SetInt(name, 0); break;
			case AnimParamType::Float: SetFloat(name, 0.0f); break;
			case AnimParamType::Trigger: Set(name, AnimParamType::Trigger, false); break;
		}
	}

	// Remove a parameter
	void RemoveParam(const std::string& name) { mParams.erase(name); }

	// Rename a parameter
	void RenameParam(const std::string& oldName, const std::string& newName)
	{
		auto it = mParams.find(oldName);
		if(it == mParams.end()) return;
		AnimParam p = it->second;
		mParams.erase(it);
		mParams[newName] = p;
	}

	// Evaluate a single condition
	bool EvaluateCondition(const AnimCondition& cond) const
	{
		auto it = mParams.find(cond.paramName);
		if(it == mParams.end()) return false;

		const AnimParam& param = it->second;

		// Handle trigger specially
		if(param.type == AnimParamType::Trigger)
		{
			if(cond.mode == AnimConditionMode::TriggerFired)
			{
				// Trigger is "fired" only if value is true AND not yet consumed
				bool triggerValue = std::get<bool>(param.value);
				return triggerValue && !param.consumed;
			}
			return false;
		}

		// Get numeric value for comparison
		float value = 0.0f;
		switch(param.type)
		{
			case AnimParamType::Bool:
				value = std::get<bool>(param.value) ? 1.0f : 0.0f;
				break;
			case AnimParamType::Int:
				value = static_cast<float>(std::get<int>(param.value));
				break;
			case AnimParamType::Float:
				value = std::get<float>(param.value);
				break;
			default:
				return false;
		}

		// Perform comparison
		switch(cond.mode)
		{
			case AnimConditionMode::Equals:         return value == cond.threshold;
			case AnimConditionMode::NotEquals:      return value != cond.threshold;
			case AnimConditionMode::Greater:        return value > cond.threshold;
			case AnimConditionMode::Less:           return value < cond.threshold;
			case AnimConditionMode::GreaterOrEqual: return value >= cond.threshold;
			case AnimConditionMode::LessOrEqual:    return value <= cond.threshold;
			default: return false;
		}
	}

	// Evaluate multiple conditions (all must be true)
	bool EvaluateConditions(const std::vector<AnimCondition>& conditions) const
	{
		for(const auto& cond : conditions)
		{
			if(!EvaluateCondition(cond))
				return false;
		}
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