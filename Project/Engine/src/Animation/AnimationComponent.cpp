#include "pch.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Animation/AnimationComponent.hpp"

AnimationComponent::AnimationComponent()
{
    animator = std::make_unique<Animator>(nullptr); // set clip later
}


AnimationComponent::AnimationComponent(const AnimationComponent& other)
    : isPlay(other.isPlay)
    , isLoop(other.isLoop)
    , speed(other.speed)
    , activeClip(other.activeClip)
{
    // clone each clip
    clips.reserve(other.clips.size());
    for (const auto& c : other.clips) {
        clips.emplace_back(std::make_unique<Animation>(*c)); // requires Animation to be copyable
    }

    // rebuild animator pointing at the cloned active clip
    Animation* active = clips.empty() ? nullptr
        : clips[std::min(activeClip, clips.size() - 1)].get();
    animator = std::make_unique<Animator>(active);

    // optional: copy playback time/state from the other animator
    if (active && other.animator) {
        // start from the beginning or replicate time
        // animator->SetCurrentTime(other.animator->GetCurrentTime());
    }
}

AnimationComponent& AnimationComponent::operator=(AnimationComponent other) noexcept 
{
    swap(*this, other);
    return *this;
}

void swap(AnimationComponent& a, AnimationComponent& b) noexcept 
{
    using std::swap;
    swap(a.isPlay, b.isPlay);
    swap(a.isLoop, b.isLoop);
    swap(a.speed, b.speed);
    swap(a.activeClip, b.activeClip);
    swap(a.clips, b.clips);
    swap(a.animator, b.animator);
}

void AnimationComponent::Update(float dt) 
{
	if (!animator) return;


    if (isPlay && !clips.empty())
    {   
        animator->UpdateAnimation(dt * speed, isLoop);
        if (!isLoop)
        {
            const float durTicks = clips[activeClip]->GetDuration();       // ticks
            if (animator->GetCurrentTime() >= durTicks) isPlay = false;
        }
    }
}

void AnimationComponent::AddClipFromFile(const std::string& path, const std::map<std::string, BoneInfo>& boneInfoMap, int boneCount)
{
	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        ENGINE_PRINT("ERROR:ASSIMP:: ", importer.GetErrorString(), "\n");
        return;
	}

	aiAnimation* aiAnim = scene->mAnimations[0]; // for now, just load the first animation

    auto anim = std::make_unique<Animation>(aiAnim, scene->mRootNode, boneInfoMap, boneCount);
	clips.push_back(std::move(anim));
}


void AnimationComponent::Play() { isPlay = true; }
void AnimationComponent::Pause() { isPlay = false; }

void AnimationComponent::Stop() 
{ 
    isPlay = false;
    if(animator)
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
Animator* AnimationComponent::GetAnimatorPtr() { return animator.get(); }
const Animator* AnimationComponent::GetAnimatorPtr() const { return animator.get(); }

Animator* AnimationComponent::EnsureAnimator() 
{
    if (!animator)
        animator = std::make_unique<Animator>(nullptr); // no clip yet
    return animator.get();
}

Animation& AnimationComponent::GetClip(size_t i) { return *clips[i]; }
const Animation& AnimationComponent::GetClip(size_t i) const { return *clips[i]; }
const std::vector<std::unique_ptr<Animation>>& AnimationComponent::GetClips() const { return clips; }
size_t AnimationComponent::GetActiveClipIndex() const { return activeClip; }


void AnimationComponent::SyncAnimatorToActiveClip()
{
    Animation* clip = clips[activeClip].get();
    animator->PlayAnimation(clip);         // resets Animator time to 0 (do this in Animator)
}