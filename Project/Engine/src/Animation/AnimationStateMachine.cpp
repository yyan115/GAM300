#include "Animation/AnimationStateMachine.hpp"
#include "Animation/AnimationComponent.hpp"

void AnimationStateMachine::Update(float dt, Entity entity)
{
	// Safety check: mOwner must be valid
	if (!mOwner) return;

	mStateTime += dt;

	AnimStateID nextState = mCurrentState;
	bool found = false;
	AnimTransition* triggeredTransition = nullptr;

	// Get current animation progress for exit time checks
	float normalizedTime = mOwner->GetNormalizedTime();
	bool animationFinished = mOwner->IsAnimationFinished();
	// Check if a loop just completed (for looping animations with exitTime ~1.0)
	bool loopJustCompleted = mOwner->HasLoopJustCompleted();

	for (auto& t : mTransitions)
	{
		if (!t.anyState && t.from != mCurrentState)
			continue;

		// Check exit time first (if required)
		// hasExitTime means we must wait until animation reaches exitTime before transitioning
		if (t.hasExitTime)
		{
			// For looping animations with exitTime >= 1.0, use loop completion detection
			// For other cases, check normalized time against exitTime
			bool exitTimeReached = false;
			if (t.exitTime >= 0.99f && loopJustCompleted)
			{
				// Exit time is ~1.0 and a loop just completed
				exitTimeReached = true;
			}
			else
			{
				// Normal case: check if normalized time reached exitTime, or animation finished
				exitTimeReached = (normalizedTime >= t.exitTime) || animationFinished;
			}
			if (!exitTimeReached)
			{
				continue;  // Skip this transition - exit time not reached yet
			}
		}

		// Now check conditions
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
			// No conditions:
			// - If hasExitTime is true, transition when animation reaches exit time (Unity behavior)
			// - If hasExitTime is false, transition immediately
			if (t.hasExitTime)
			{
				// We already checked exit time above, so if we're here, exit time is reached
				conditionMet = true;
			}
			else
			{
				// No exit time and no conditions = immediate transition
				conditionMet = true;
			}
		}

		if (conditionMet)
		{
			nextState = t.to;
			triggeredTransition = &t;
			found = true;
			break;
		}
	}

	if (found && triggeredTransition)
	{
		// Consume any triggers that were used in this transition
		ConsumeTriggers(*triggeredTransition);

		EnterState(nextState, entity, triggeredTransition->transitionDuration);
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

void AnimationStateMachine::ConsumeTriggers(const AnimTransition& transition)
{
	// After a transition fires, consume any trigger parameters that were used
	for (const auto& cond : transition.conditions)
	{
		if (cond.mode == AnimConditionMode::TriggerFired)
		{
			// Consume the trigger by calling GetTrigger (which sets consumed = true)
			mParam.GetTrigger(cond.paramName);
		}
	}
}

void AnimationStateMachine::EnterState(const AnimStateID& id, Entity entity, float transitionCrossfade)
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

	// Safety: validate clipIndex is within bounds
	if (config.clipIndex >= mOwner->GetClips().size()) {
		return;
	}

	// Apply speed
	mOwner->SetSpeed(config.speed);

	// Use transition duration if set, otherwise fall back to state's crossfade duration
	float crossfadeDur = (transitionCrossfade > 0.0f) ? transitionCrossfade : config.crossfadeDuration;

	if (crossfadeDur > 0.0f)
	{
		mOwner->PlayClipWithCrossfade(config.clipIndex, config.loop, crossfadeDur, entity);
	}
	else
	{
		if (config.loop)
			mOwner->PlayClip(config.clipIndex, true, entity);
		else
			mOwner->PlayOnce(config.clipIndex, entity);
	}
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

void AnimationStateMachine::Reset(Entity entity)
{
	mCurrentState = mEntryState;
	mStateTime = 0.0f;
	//if (!mEntryState.empty())
	//{
	//	EnterState(mEntryState, entity);
	//}
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