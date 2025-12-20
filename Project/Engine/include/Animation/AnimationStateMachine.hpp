#pragma once
#include <pch.h>
#include <vector>
#include <functional>
#include "Animation/AnimationParam.hpp"
#include "Animation/AnimationComponent.hpp"

using AnimStateID = std::string;
class AnimationComponent;

struct AnimStateConfig
{
	std::size_t clipIndex = 0;
	bool loop = true;
	float crossfadeDuration = 0.0f; // Mayb in future
};

struct AnimTransition
{
	AnimStateID from;
	AnimStateID to;
	bool anyState = false;

	std::function<bool(const AnimParamSet&)> condition;
};

class AnimationStateMachine
{
public:
	void SetOwner(AnimationComponent* comp) { mOwner = comp; }

	AnimParamSet&		GetParams()			{ return mParam; }
	const AnimParamSet& GetParams() const	{ return mParam; }

	void AddState(const AnimStateID& id, const AnimStateConfig& config) { mStates[id] = config; }
	void AddTransition(const AnimTransition& transition) { mTransitions.push_back(transition); }
	void SetInitialState(const AnimStateID& id) 
	{
		mCurrentState = id;
		EnterState(id);
	}

	const AnimStateID& GetCurrentState() const { return mCurrentState; }

	void Update(float dt);
	

private:
	void EnterState(const AnimStateID& id);

private:
	AnimationComponent* mOwner = nullptr;
	AnimStateID mCurrentState;
	float mStateTime = 0.0f;

	AnimParamSet mParam;
	std::unordered_map<AnimStateID, AnimStateConfig> mStates;
	std::vector<AnimTransition> mTransitions;
};