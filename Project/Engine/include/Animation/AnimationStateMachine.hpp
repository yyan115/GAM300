#pragma once
#include <pch.h>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include "Animation/AnimationParam.hpp"
#include "ECS/Entity.hpp"

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

using AnimStateID = std::string;
class AnimationComponent;  // Forward declaration only - no include to avoid circular dependency

// State configuration (serializable)
struct ENGINE_API AnimStateConfig
{
	std::size_t clipIndex = 0;
	bool loop = true;
	float speed = 1.0f;
	float crossfadeDuration = 0.0f; // Maybe in future
	glm::vec2 nodePosition = {0.0f, 0.0f}; // Position in node graph editor
};

// Transition between states (serializable)
struct ENGINE_API AnimTransition
{
	AnimStateID from;
	AnimStateID to;
	bool anyState = false;
	bool hasExitTime = false;     // Wait for animation to finish before transitioning
	float exitTime = 1.0f;        // Normalized time (0-1) when transition can occur
	float transitionDuration = 0.0f; // Blend duration (future)

	// Serializable conditions (replaces lambda)
	std::vector<AnimCondition> conditions;

	// Legacy lambda support (for backwards compatibility, not serialized)
	std::function<bool(const AnimParamSet&)> conditionFunc;
};

class ENGINE_API AnimationStateMachine
{
public:
	void SetOwner(AnimationComponent* comp) { mOwner = comp; }
	AnimationComponent* GetOwner() const { return mOwner; }

	AnimParamSet&		GetParams()			{ return mParam; }
	const AnimParamSet& GetParams() const	{ return mParam; }

	// State management
	void AddState(const AnimStateID& id, const AnimStateConfig& config) { mStates[id] = config; }
	void RemoveState(const AnimStateID& id);
	void RenameState(const AnimStateID& oldId, const AnimStateID& newId);
	bool HasState(const AnimStateID& id) const { return mStates.find(id) != mStates.end(); }
	AnimStateConfig* GetState(const AnimStateID& id);
	const AnimStateConfig* GetState(const AnimStateID& id) const;

	// Transition management
	void AddTransition(const AnimTransition& transition) { mTransitions.push_back(transition); }
	void RemoveTransition(size_t index);
	AnimTransition* GetTransition(size_t index);
	const AnimTransition* GetTransition(size_t index) const;

	// Controller name (for display in inspector)
	void SetName(const std::string& name) { mName = name; }
	const std::string& GetName() const { return mName; }

	// Initial/Entry state
	void SetInitialState(const AnimStateID& id, Entity entity)
	{
		mEntryState = id;
		mCurrentState = id;
		EnterState(id, entity);
	}
	const AnimStateID& GetEntryState() const { return mEntryState; }
	void SetEntryState(const AnimStateID& id) { mEntryState = id; }

	const AnimStateID& GetCurrentState() const { return mCurrentState; }
	float GetStateTime() const { return mStateTime; }

	// Editor access to internal data
	const std::unordered_map<AnimStateID, AnimStateConfig>& GetAllStates() const { return mStates; }
	std::unordered_map<AnimStateID, AnimStateConfig>& GetAllStates() { return mStates; }
	const std::vector<AnimTransition>& GetAllTransitions() const { return mTransitions; }
	std::vector<AnimTransition>& GetAllTransitions() { return mTransitions; }

	// Runtime
	void Update(float dt, Entity entity);
	void Reset(Entity entity); // Reset to entry state

	// Clear all data
	void Clear();

private:
	void EnterState(const AnimStateID& id, Entity entity);
	bool EvaluateTransitionConditions(const AnimTransition& transition) const;

private:
	AnimationComponent* mOwner = nullptr;
	std::string mName;
	AnimStateID mCurrentState;
	AnimStateID mEntryState;
	float mStateTime = 0.0f;

	AnimParamSet mParam;
	std::unordered_map<AnimStateID, AnimStateConfig> mStates;
	std::vector<AnimTransition> mTransitions;
};