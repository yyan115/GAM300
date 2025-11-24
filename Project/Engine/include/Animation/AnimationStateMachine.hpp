#pragma once
#include <vector>
#include <functional>
#include "Animation/AnimationParam.hpp"
#include "Animation/AnimationComponent.hpp"

using AnimStateID = std::string;

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

	void Update(float dt)
	{
		mStateTime += dt;

		AnimStateID nextState = mCurrentState;
		bool found = false;

		for(const auto& t : mTransitions)
		{
			if(!t.anyState && t.from != mCurrentState)
				continue;

			if (t.condition(mParam))
			{
				nextState = t.to;
				found = true;
				break;
			}
		}

		if (found)
		{
			EnterState(nextState);
		}

	}
	

private:
	void EnterState(const AnimStateID& id)
	{
		mCurrentState = id;
		mStateTime = 0.0f;

		auto it = mStates.find(id);
		if(it == mStates.end() || !mOwner)
			return;

		const AnimStateConfig& config = it->second;

		if(config.loop)
			mOwner->PlayClip(config.clipIndex, true);
		else
			mOwner->PlayOnce(config.clipIndex);
	}

private:
	AnimationComponent* mOwner = nullptr;
	AnimStateID mCurrentState;
	float mStateTime = 0.0f;

	AnimParamSet mParam;
	std::unordered_map<AnimStateID, AnimStateConfig> mStates;
	std::vector<AnimTransition> mTransitions;
};