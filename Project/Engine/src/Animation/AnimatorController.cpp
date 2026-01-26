#include "pch.h"
#include "Animation/AnimatorController.hpp"
#include <fstream>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include "Logging.hpp"

using namespace rapidjson;

// Helper to convert AnimParamType to string
static const char* ParamTypeToString(AnimParamType type)
{
	switch (type)
	{
		case AnimParamType::Bool: return "Bool";
		case AnimParamType::Int: return "Int";
		case AnimParamType::Float: return "Float";
		case AnimParamType::Trigger: return "Trigger";
		default: return "Bool";
	}
}

static AnimParamType StringToParamType(const char* str)
{
	if (strcmp(str, "Bool") == 0) return AnimParamType::Bool;
	if (strcmp(str, "Int") == 0) return AnimParamType::Int;
	if (strcmp(str, "Float") == 0) return AnimParamType::Float;
	if (strcmp(str, "Trigger") == 0) return AnimParamType::Trigger;
	return AnimParamType::Bool;
}

// Helper to convert AnimConditionMode to string
static const char* ConditionModeToString(AnimConditionMode mode)
{
	switch (mode)
	{
		case AnimConditionMode::Equals: return "Equals";
		case AnimConditionMode::NotEquals: return "NotEquals";
		case AnimConditionMode::Greater: return "Greater";
		case AnimConditionMode::Less: return "Less";
		case AnimConditionMode::GreaterOrEqual: return "GreaterOrEqual";
		case AnimConditionMode::LessOrEqual: return "LessOrEqual";
		case AnimConditionMode::TriggerFired: return "TriggerFired";
		default: return "Equals";
	}
}

static AnimConditionMode StringToConditionMode(const char* str)
{
	if (strcmp(str, "Equals") == 0) return AnimConditionMode::Equals;
	if (strcmp(str, "NotEquals") == 0) return AnimConditionMode::NotEquals;
	if (strcmp(str, "Greater") == 0) return AnimConditionMode::Greater;
	if (strcmp(str, "Less") == 0) return AnimConditionMode::Less;
	if (strcmp(str, "GreaterOrEqual") == 0) return AnimConditionMode::GreaterOrEqual;
	if (strcmp(str, "LessOrEqual") == 0) return AnimConditionMode::LessOrEqual;
	if (strcmp(str, "TriggerFired") == 0) return AnimConditionMode::TriggerFired;
	return AnimConditionMode::Equals;
}

bool AnimatorController::SaveToFile(const std::string& filePath) const
{
	Document doc;
	doc.SetObject();
	auto& allocator = doc.GetAllocator();

	// Name
	doc.AddMember("name", Value(mName.c_str(), allocator), allocator);

	// Entry state
	doc.AddMember("entryState", Value(mEntryState.c_str(), allocator), allocator);

	// Entry node position
	Value entryPos(kObjectType);
	entryPos.AddMember("x", mEntryNodePosition.x, allocator);
	entryPos.AddMember("y", mEntryNodePosition.y, allocator);
	doc.AddMember("entryNodePosition", entryPos, allocator);

	// Any state position
	Value anyPos(kObjectType);
	anyPos.AddMember("x", mAnyStatePosition.x, allocator);
	anyPos.AddMember("y", mAnyStatePosition.y, allocator);
	doc.AddMember("anyStatePosition", anyPos, allocator);

	// Parameters
	Value paramsArray(kArrayType);
	for (const auto& param : mParameters)
	{
		Value paramObj(kObjectType);
		paramObj.AddMember("name", Value(param.name.c_str(), allocator), allocator);
		paramObj.AddMember("type", Value(ParamTypeToString(param.type), allocator), allocator);
		paramObj.AddMember("defaultValue", param.defaultValue, allocator);
		paramsArray.PushBack(paramObj, allocator);
	}
	doc.AddMember("parameters", paramsArray, allocator);

	// States
	Value statesArray(kArrayType);
	for (const auto& [stateId, config] : mStates)
	{
		Value stateObj(kObjectType);
		stateObj.AddMember("id", Value(stateId.c_str(), allocator), allocator);
		stateObj.AddMember("clipIndex", static_cast<uint64_t>(config.clipIndex), allocator);
		stateObj.AddMember("loop", config.loop, allocator);
		stateObj.AddMember("speed", config.speed, allocator);
		stateObj.AddMember("crossfadeDuration", config.crossfadeDuration, allocator);

		Value posObj(kObjectType);
		posObj.AddMember("x", config.nodePosition.x, allocator);
		posObj.AddMember("y", config.nodePosition.y, allocator);
		stateObj.AddMember("nodePosition", posObj, allocator);

		statesArray.PushBack(stateObj, allocator);
	}
	doc.AddMember("states", statesArray, allocator);

	// Transitions
	Value transitionsArray(kArrayType);
	for (const auto& trans : mTransitions)
	{
		Value transObj(kObjectType);
		transObj.AddMember("from", Value(trans.from.c_str(), allocator), allocator);
		transObj.AddMember("to", Value(trans.to.c_str(), allocator), allocator);
		transObj.AddMember("anyState", trans.anyState, allocator);
		transObj.AddMember("hasExitTime", trans.hasExitTime, allocator);
		transObj.AddMember("exitTime", trans.exitTime, allocator);
		transObj.AddMember("transitionDuration", trans.transitionDuration, allocator);

		// Conditions
		Value conditionsArray(kArrayType);
		for (const auto& cond : trans.conditions)
		{
			Value condObj(kObjectType);
			condObj.AddMember("paramName", Value(cond.paramName.c_str(), allocator), allocator);
			condObj.AddMember("mode", Value(ConditionModeToString(cond.mode), allocator), allocator);
			condObj.AddMember("threshold", cond.threshold, allocator);
			conditionsArray.PushBack(condObj, allocator);
		}
		transObj.AddMember("conditions", conditionsArray, allocator);

		transitionsArray.PushBack(transObj, allocator);
	}
	doc.AddMember("transitions", transitionsArray, allocator);

	// Clip paths
	Value clipPathsArray(kArrayType);
	for (const auto& path : mClipPaths)
	{
		clipPathsArray.PushBack(Value(path.c_str(), allocator), allocator);
	}
	doc.AddMember("clipPaths", clipPathsArray, allocator);

	// Write to file
	StringBuffer buffer;
	PrettyWriter<StringBuffer> writer(buffer);
	doc.Accept(writer);

	std::ofstream file(filePath);
	if (!file.is_open())
	{
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AnimatorController] Failed to open file for writing: ", filePath, "\n");
		return false;
	}

	file << buffer.GetString();
	file.close();

	ENGINE_PRINT("[AnimatorController] Saved controller to: ", filePath, "\n");
	return true;
}

bool AnimatorController::LoadFromFile(const std::string& filePath)
{
	std::ifstream file(filePath);
	if (!file.is_open())
	{
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AnimatorController] Failed to open file for reading: ", filePath, "\n");
		return false;
	}

	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();

	Document doc;
	if (doc.Parse(content.c_str()).HasParseError())
	{
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AnimatorController] Failed to parse JSON: ", filePath, "\n");
		return false;
	}

	// Clear existing data
	mStates.clear();
	mTransitions.clear();
	mParameters.clear();
	mClipPaths.clear();

	// Name
	if (doc.HasMember("name") && doc["name"].IsString())
		mName = doc["name"].GetString();

	// Entry state
	if (doc.HasMember("entryState") && doc["entryState"].IsString())
		mEntryState = doc["entryState"].GetString();

	// Entry node position
	if (doc.HasMember("entryNodePosition") && doc["entryNodePosition"].IsObject())
	{
		const auto& pos = doc["entryNodePosition"];
		if (pos.HasMember("x")) mEntryNodePosition.x = pos["x"].GetFloat();
		if (pos.HasMember("y")) mEntryNodePosition.y = pos["y"].GetFloat();
	}

	// Any state position
	if (doc.HasMember("anyStatePosition") && doc["anyStatePosition"].IsObject())
	{
		const auto& pos = doc["anyStatePosition"];
		if (pos.HasMember("x")) mAnyStatePosition.x = pos["x"].GetFloat();
		if (pos.HasMember("y")) mAnyStatePosition.y = pos["y"].GetFloat();
	}

	// Parameters
	if (doc.HasMember("parameters") && doc["parameters"].IsArray())
	{
		for (const auto& paramVal : doc["parameters"].GetArray())
		{
			AnimParamDefinition param;
			if (paramVal.HasMember("name")) param.name = paramVal["name"].GetString();
			if (paramVal.HasMember("type")) param.type = StringToParamType(paramVal["type"].GetString());
			if (paramVal.HasMember("defaultValue")) param.defaultValue = paramVal["defaultValue"].GetFloat();
			mParameters.push_back(param);
		}
	}

	// States
	if (doc.HasMember("states") && doc["states"].IsArray())
	{
		for (const auto& stateVal : doc["states"].GetArray())
		{
			AnimStateID id;
			AnimStateConfig config;

			if (stateVal.HasMember("id")) id = stateVal["id"].GetString();
			if (stateVal.HasMember("clipIndex")) config.clipIndex = stateVal["clipIndex"].GetUint64();
			if (stateVal.HasMember("loop")) config.loop = stateVal["loop"].GetBool();
			if (stateVal.HasMember("speed")) config.speed = stateVal["speed"].GetFloat();
			if (stateVal.HasMember("crossfadeDuration")) config.crossfadeDuration = stateVal["crossfadeDuration"].GetFloat();

			if (stateVal.HasMember("nodePosition") && stateVal["nodePosition"].IsObject())
			{
				const auto& pos = stateVal["nodePosition"];
				if (pos.HasMember("x")) config.nodePosition.x = pos["x"].GetFloat();
				if (pos.HasMember("y")) config.nodePosition.y = pos["y"].GetFloat();
			}

			mStates[id] = config;
		}
	}

	// Transitions
	if (doc.HasMember("transitions") && doc["transitions"].IsArray())
	{
		for (const auto& transVal : doc["transitions"].GetArray())
		{
			AnimTransition trans;

			if (transVal.HasMember("from")) trans.from = transVal["from"].GetString();
			if (transVal.HasMember("to")) trans.to = transVal["to"].GetString();
			if (transVal.HasMember("anyState")) trans.anyState = transVal["anyState"].GetBool();
			if (transVal.HasMember("hasExitTime")) trans.hasExitTime = transVal["hasExitTime"].GetBool();
			if (transVal.HasMember("exitTime")) trans.exitTime = transVal["exitTime"].GetFloat();
			if (transVal.HasMember("transitionDuration")) trans.transitionDuration = transVal["transitionDuration"].GetFloat();

			// Conditions
			if (transVal.HasMember("conditions") && transVal["conditions"].IsArray())
			{
				for (const auto& condVal : transVal["conditions"].GetArray())
				{
					AnimCondition cond;
					if (condVal.HasMember("paramName")) cond.paramName = condVal["paramName"].GetString();
					if (condVal.HasMember("mode")) cond.mode = StringToConditionMode(condVal["mode"].GetString());
					if (condVal.HasMember("threshold")) cond.threshold = condVal["threshold"].GetFloat();
					trans.conditions.push_back(cond);
				}
			}

			mTransitions.push_back(trans);
		}
	}

	// Clip paths
	if (doc.HasMember("clipPaths") && doc["clipPaths"].IsArray())
	{
		for (const auto& pathVal : doc["clipPaths"].GetArray())
		{
			mClipPaths.push_back(pathVal.GetString());
		}
	}

	ENGINE_PRINT("[AnimatorController] Loaded controller from: ", filePath, "\n");
	return true;
}

void AnimatorController::ApplyToStateMachine(AnimationStateMachine* stateMachine) const
{
	if (!stateMachine) return;

	stateMachine->Clear();

	// Set controller name
	stateMachine->SetName(mName);

	// Add parameters
	AnimParamSet& params = stateMachine->GetParams();
	for (const auto& paramDef : mParameters)
	{
		params.AddParam(paramDef.name, paramDef.type);
		// Set default value
		switch (paramDef.type)
		{
			case AnimParamType::Bool:
				params.SetBool(paramDef.name, paramDef.defaultValue > 0.5f);
				break;
			case AnimParamType::Int:
				params.SetInt(paramDef.name, static_cast<int>(paramDef.defaultValue));
				break;
			case AnimParamType::Float:
				params.SetFloat(paramDef.name, paramDef.defaultValue);
				break;
			case AnimParamType::Trigger:
				// Triggers start unconsumed
				break;
		}
	}

	// Add states
	for (const auto& [stateId, config] : mStates)
	{
		stateMachine->AddState(stateId, config);
	}

	// Add transitions
	for (const auto& trans : mTransitions)
	{
		stateMachine->AddTransition(trans);
	}

	// Set entry state
	if (!mEntryState.empty())
	{
		stateMachine->SetEntryState(mEntryState);
	}
}

void AnimatorController::ExtractFromStateMachine(const AnimationStateMachine* stateMachine)
{
	if (!stateMachine) return;

	mStates.clear();
	mTransitions.clear();
	mParameters.clear();

	// Extract name
	mName = stateMachine->GetName();

	// Extract states
	mStates = stateMachine->GetAllStates();

	// Extract transitions
	mTransitions = stateMachine->GetAllTransitions();

	// Extract parameters
	const AnimParamSet& params = stateMachine->GetParams();
	for (const auto& [name, param] : params.GetAllParams())
	{
		AnimParamDefinition def;
		def.name = name;
		def.type = param.type;
		switch (param.type)
		{
			case AnimParamType::Bool:
				def.defaultValue = std::get<bool>(param.value) ? 1.0f : 0.0f;
				break;
			case AnimParamType::Int:
				def.defaultValue = static_cast<float>(std::get<int>(param.value));
				break;
			case AnimParamType::Float:
				def.defaultValue = std::get<float>(param.value);
				break;
			case AnimParamType::Trigger:
				def.defaultValue = 0.0f;
				break;
		}
		mParameters.push_back(def);
	}

	// Extract entry state
	mEntryState = stateMachine->GetEntryState();
}

void AnimatorController::RemoveState(const AnimStateID& id)
{
	mStates.erase(id);

	// Remove transitions involving this state
	mTransitions.erase(
		std::remove_if(mTransitions.begin(), mTransitions.end(),
			[&id](const AnimTransition& t) {
				return t.from == id || t.to == id;
			}),
		mTransitions.end());

	// Update entry state if needed
	if (mEntryState == id)
	{
		mEntryState = mStates.empty() ? "" : mStates.begin()->first;
	}
}

void AnimatorController::RemoveTransition(size_t index)
{
	if (index < mTransitions.size())
	{
		mTransitions.erase(mTransitions.begin() + index);
	}
}

void AnimatorController::AddParameter(const std::string& name, AnimParamType type)
{
	// Check if already exists
	for (const auto& p : mParameters)
	{
		if (p.name == name) return;
	}

	AnimParamDefinition def;
	def.name = name;
	def.type = type;
	def.defaultValue = 0.0f;
	mParameters.push_back(def);
}

void AnimatorController::RemoveParameter(const std::string& name)
{
	mParameters.erase(
		std::remove_if(mParameters.begin(), mParameters.end(),
			[&name](const AnimParamDefinition& p) { return p.name == name; }),
		mParameters.end());

	// Also remove conditions using this parameter
	for (auto& trans : mTransitions)
	{
		trans.conditions.erase(
			std::remove_if(trans.conditions.begin(), trans.conditions.end(),
				[&name](const AnimCondition& c) { return c.paramName == name; }),
			trans.conditions.end());
	}
}

void AnimatorController::RenameParameter(const std::string& oldName, const std::string& newName)
{
	for (auto& p : mParameters)
	{
		if (p.name == oldName)
		{
			p.name = newName;
			break;
		}
	}

	// Update conditions using this parameter
	for (auto& trans : mTransitions)
	{
		for (auto& cond : trans.conditions)
		{
			if (cond.paramName == oldName)
			{
				cond.paramName = newName;
			}
		}
	}
}
