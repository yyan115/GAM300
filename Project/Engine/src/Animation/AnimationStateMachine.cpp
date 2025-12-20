#include "Animation/AnimationStateMachine.hpp"
#include "Animation/AnimationComponent.hpp"

void AnimationStateMachine::Update(float dt)
{
	mStateTime += dt;

	AnimStateID nextState = mCurrentState;
	bool found = false;

	for (const auto& t : mTransitions)
	{
		if (!t.anyState && t.from != mCurrentState)
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

void AnimationStateMachine::EnterState(const AnimStateID& id)
{
	mCurrentState = id;
	mStateTime = 0.0f;

	auto it = mStates.find(id);
	if (it == mStates.end() || !mOwner)
		return;

	const AnimStateConfig& config = it->second;

	if (config.loop)
		mOwner->PlayClip(config.clipIndex, true);
	else
		mOwner->PlayOnce(config.clipIndex);
}