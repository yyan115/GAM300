#pragma once
#include <pch.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include "Animation/AnimationParam.hpp"
#include "Animation/AnimationStateMachine.hpp"

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

// Serializable parameter definition (for storing default params in controller)
struct ENGINE_API AnimParamDefinition
{
	std::string name;
	AnimParamType type = AnimParamType::Bool;
	float defaultValue = 0.0f; // For Bool: 0=false, 1=true; For Trigger: always 0
};

// AnimatorController - A serializable asset that defines animation state machine behavior
// Similar to Unity's AnimatorController asset
class ENGINE_API AnimatorController
{
public:
	AnimatorController() = default;
	~AnimatorController() = default;

	// File I/O
	bool SaveToFile(const std::string& filePath) const;
	bool LoadFromFile(const std::string& filePath);

	// Apply this controller to a state machine
	void ApplyToStateMachine(AnimationStateMachine* stateMachine) const;

	// Extract data from a state machine (for saving)
	void ExtractFromStateMachine(const AnimationStateMachine* stateMachine);

	// Accessors
	const std::string& GetName() const { return mName; }
	void SetName(const std::string& name) { mName = name; }

	// States
	std::unordered_map<AnimStateID, AnimStateConfig>& GetStates() { return mStates; }
	const std::unordered_map<AnimStateID, AnimStateConfig>& GetStates() const { return mStates; }

	void AddState(const AnimStateID& id, const AnimStateConfig& config) { mStates[id] = config; }
	void RemoveState(const AnimStateID& id);
	bool HasState(const AnimStateID& id) const { return mStates.find(id) != mStates.end(); }

	// Transitions
	std::vector<AnimTransition>& GetTransitions() { return mTransitions; }
	const std::vector<AnimTransition>& GetTransitions() const { return mTransitions; }

	void AddTransition(const AnimTransition& transition) { mTransitions.push_back(transition); }
	void RemoveTransition(size_t index);

	// Parameters
	std::vector<AnimParamDefinition>& GetParameters() { return mParameters; }
	const std::vector<AnimParamDefinition>& GetParameters() const { return mParameters; }

	void AddParameter(const std::string& name, AnimParamType type);
	void RemoveParameter(const std::string& name);
	void RenameParameter(const std::string& oldName, const std::string& newName);

	// Entry state
	const AnimStateID& GetEntryState() const { return mEntryState; }
	void SetEntryState(const AnimStateID& id) { mEntryState = id; }

	// Any State position (for editor)
	glm::vec2 GetAnyStatePosition() const { return mAnyStatePosition; }
	void SetAnyStatePosition(const glm::vec2& pos) { mAnyStatePosition = pos; }

	// Entry node position (for editor)
	glm::vec2 GetEntryNodePosition() const { return mEntryNodePosition; }
	void SetEntryNodePosition(const glm::vec2& pos) { mEntryNodePosition = pos; }

	// Clip paths (for editor reference)
	std::vector<std::string>& GetClipPaths() { return mClipPaths; }
	const std::vector<std::string>& GetClipPaths() const { return mClipPaths; }

private:
	std::string mName = "New Animator";
	AnimStateID mEntryState;

	std::unordered_map<AnimStateID, AnimStateConfig> mStates;
	std::vector<AnimTransition> mTransitions;
	std::vector<AnimParamDefinition> mParameters;
	std::vector<std::string> mClipPaths; // Reference to animation clip paths

	// Editor node positions for special nodes
	glm::vec2 mAnyStatePosition = {-200.0f, 0.0f};
	glm::vec2 mEntryNodePosition = {-200.0f, -100.0f};
};
