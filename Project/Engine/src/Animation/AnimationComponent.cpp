#include "pch.h"
#include "Animation/AnimationComponent.hpp"

AnimationComponent::AnimationComponent(Model* model) : model(model) 
{
    animator = std::make_unique<Animator>(nullptr); // set clip later
}

void AnimationComponent::Update(float dt) 
{
	if (!animator) return;

    if (isPlay && !clips.empty())
    {
        animator->UpdateAnimation(dt * speed, isLoop);
        if (!isLoop)
        {
			float dur = clips[activeClip]->GetDuration() / clips[activeClip]->GetTicksPerSecond();
            if (animator->GetCurrentTime() >= dur)
            {
				isPlay = false; // stop at end
            }

        }
    }
}


void AnimationComponent::Play() { isPlay = true; }
void AnimationComponent::Pause() { isPlay = false; }

void AnimationComponent::Stop() 
{ 
    isPlay = false; 
    animator->PlayAnimation(clips[activeClip].get()); 
}

void AnimationComponent::SetLooping(bool v) { isLoop = v; }
void AnimationComponent::SetSpeed(float s) { speed = std::max(0.0f, s); }


void AnimationComponent::SetClip(size_t index)
{
    if (index >= clips.size() || index == activeClip) return;
    activeClip = index;
    SyncAnimatorToActiveClip();
}

Animator& AnimationComponent::GetAnimator() { return *animator; }
const Animator& AnimationComponent::GetAnimator() const { return *animator; }
Animation& AnimationComponent::GetClip(size_t i) { return *clips[i]; }
const Animation& AnimationComponent::GetClip(size_t i) const { return *clips[i]; }
const std::vector<std::unique_ptr<Animation>>& AnimationComponent::GetClips() const { return clips; }
size_t AnimationComponent::GetActiveClipIndex() const { return activeClip; }


void AnimationComponent::SyncAnimatorToActiveClip()
{
    Animation* clip = clips[activeClip].get();
    animator->PlayAnimation(clip);         // resets Animator time to 0 (do this in Animator)
}