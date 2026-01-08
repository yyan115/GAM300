#include "Animation/AnimationStateMachine.hpp"
#include "Animation/AnimationComponent.hpp"

void AnimationStateMachine::Update(float dt)
{
	mStateTime += dt;

	AnimStateID nextState = mCurrentState;
	bool found = false;

	for (auto& t : mTransitions)
	{
		if (!t.anyState && t.from != mCurrentState)
			continue;

		// Use serializable conditions if available, otherwise fall back to lambda
		bool conditionMet = false;
		if (!t.conditions.empty())
		{
			conditionMet = EvaluateTransitionConditions(t);
		}
		else if (t.conditionFunc)
		{
			conditionMet = t.conditionFunc(mParam);
		}
		else
		{
			// No conditions means always transition (immediate)
			conditionMet = true;
		}

		if (conditionMet)
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

bool AnimationStateMachine::EvaluateTransitionConditions(const AnimTransition& transition) const
{
	// All conditions must be true
	for (const auto& cond : transition.conditions)
	{
		if (!mParam.EvaluateCondition(cond))
			return false;
	}
	return true;
}

void AnimationStateMachine::EnterState(const AnimStateID& id)
{
	mCurrentState = id;
	mStateTime = 0.0f;

	auto it = mStates.find(id);
	if (it == mStates.end() || !mOwner)
		return;

	const AnimStateConfig& config = it->second;

	// Safety: check if clips are loaded before trying to play
	if (mOwner->GetClips().empty()) {
		return;
	}

	// Apply speed
	mOwner->SetSpeed(config.speed);

	if (config.loop)
		mOwner->PlayClip(config.clipIndex, true);
	else
		mOwner->PlayOnce(config.clipIndex);
}

void AnimationStateMachine::RemoveState(const AnimStateID& id)
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

void AnimationStateMachine::RenameState(const AnimStateID& oldId, const AnimStateID& newId)
{
	auto it = mStates.find(oldId);
	if (it == mStates.end()) return;

	AnimStateConfig config = it->second;
	mStates.erase(it);
	mStates[newId] = config;

	// Update transitions
	for (auto& t : mTransitions)
	{
		if (t.from == oldId) t.from = newId;
		if (t.to == oldId) t.to = newId;
	}

	// Update entry state if needed
	if (mEntryState == oldId) mEntryState = newId;
	if (mCurrentState == oldId) mCurrentState = newId;
}

AnimStateConfig* AnimationStateMachine::GetState(const AnimStateID& id)
{
	auto it = mStates.find(id);
	return it != mStates.end() ? &it->second : nullptr;
}

const AnimStateConfig* AnimationStateMachine::GetState(const AnimStateID& id) const
{
	auto it = mStates.find(id);
	return it != mStates.end() ? &it->second : nullptr;
}

void AnimationStateMachine::RemoveTransition(size_t index)
{
	if (index < mTransitions.size())
	{
		mTransitions.erase(mTransitions.begin() + index);
	}
}

AnimTransition* AnimationStateMachine::GetTransition(size_t index)
{
	return index < mTransitions.size() ? &mTransitions[index] : nullptr;
}

const AnimTransition* AnimationStateMachine::GetTransition(size_t index) const
{
	return index < mTransitions.size() ? &mTransitions[index] : nullptr;
}

void AnimationStateMachine::Reset()
{
	mCurrentState = mEntryState;
	mStateTime = 0.0f;
	if (!mEntryState.empty())
	{
		EnterState(mEntryState);
	}
}

void AnimationStateMachine::Clear()
{
	mStates.clear();
	mTransitions.clear();
	mParam = AnimParamSet();
	mCurrentState = "";
	mEntryState = "";
	mStateTime = 0.0f;
}