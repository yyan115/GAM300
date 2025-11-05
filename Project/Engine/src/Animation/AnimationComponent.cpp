#include "pch.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Animation/AnimationComponent.hpp"
#include <Platform/IPlatform.h>
#include <WindowManager.hpp>

#pragma region Reflection
REFL_REGISTER_START(AnimationComponent)
	REFL_REGISTER_PROPERTY(enabled)
	REFL_REGISTER_PROPERTY(isPlay)
	REFL_REGISTER_PROPERTY(isLoop)
	REFL_REGISTER_PROPERTY(speed)
	REFL_REGISTER_PROPERTY(clipCount)
	REFL_REGISTER_PROPERTY(clipPaths)
	REFL_REGISTER_PROPERTY(clipGUIDs)
REFL_REGISTER_END
#pragma endregion

AnimationComponent::AnimationComponent()
{
    animator = std::make_unique<Animator>(nullptr); // set clip later
}


AnimationComponent::AnimationComponent(const AnimationComponent& other)
    : enabled(other.enabled)
    , isPlay(other.isPlay)
    , isLoop(other.isLoop)
    , speed(other.speed)
    , clipCount(other.clipCount)
    , clipPaths(other.clipPaths)
    , clipGUIDs(other.clipGUIDs)
    , activeClip(other.activeClip)
{
    clips.reserve(other.clips.size());
    for (const auto& c : other.clips) {
        clips.emplace_back(std::make_unique<Animation>(*c));
    }

    Animation* active = clips.empty() ? nullptr
        : clips[std::min(activeClip, clips.size() - 1)].get();
    animator = std::make_unique<Animator>(active);

    if (active && other.animator) {
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
    swap(a.enabled, b.enabled);
    swap(a.isPlay, b.isPlay);
    swap(a.isLoop, b.isLoop);
    swap(a.speed, b.speed);
    swap(a.clipCount, b.clipCount);
    swap(a.clipPaths, b.clipPaths);
    swap(a.clipGUIDs, b.clipGUIDs);
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

std::unique_ptr<Animation> AnimationComponent::LoadClipFromPath(const std::string& path, const std::map<std::string, BoneInfo>& boneInfoMap, int boneCount)
{
    if (path.empty()) return nullptr;

    IPlatform* platform = WindowManager::GetPlatform();
    if (!platform) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AnimationComponent] ERROR: Platform not available for asset discovery!", "\n");
        return nullptr;
    }

    std::vector<uint8_t> buffer = platform->ReadAsset(path);

    Assimp::Importer importer;

    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
        aiComponent_NORMALS | aiComponent_TANGENTS_AND_BITANGENTS);

    const aiScene* scene = importer.ReadFileFromMemory(buffer.data(), buffer.size(), aiProcess_Triangulate | aiProcess_FlipUVs, "fbx");

    if (!scene) {
        ENGINE_PRINT("[Anim] ReadFile failed: ", importer.GetErrorString(), " path=", path, "\n");
        return nullptr;
    }

    if (!scene->mRootNode) {
        ENGINE_PRINT("[Anim] No root node\n");
        return nullptr;
    }
    if (scene->mNumAnimations == 0) {
        ENGINE_PRINT("[Anim] File has NO animations: ", path, "\n");
        return nullptr;
    }

    aiAnimation* aiAnim = scene->mAnimations[0];
    return std::make_unique<Animation>(aiAnim, scene->mRootNode, boneInfoMap, boneCount);
}

void AnimationComponent::AddClipFromFile(const std::string& path, const std::map<std::string, BoneInfo>& boneInfoMap, int boneCount)
{
    auto anim = LoadClipFromPath(path, boneInfoMap, boneCount);
    if (!anim) return;

    clips.emplace_back(std::move(anim));

    clipPaths.push_back(path);
    clipGUIDs.push_back({0, 0});
    clipCount = static_cast<int>(clipPaths.size());

    if(clips.size() == 1)
    {
        activeClip = 0;
        EnsureAnimator();
        animator->PlayAnimation(clips[0].get());
    }
}


void AnimationComponent::Play() 
{
    isPlay = true; 
    if (!clips.empty())
    {
		EnsureAnimator();
		animator->PlayAnimation(clips[activeClip].get());
    }
}
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
    animator->PlayAnimation(clip);
}

void AnimationComponent::SetClipCount(size_t count)
{
    clipCount = static_cast<int>(count);
    clipPaths.resize(count);
    clipGUIDs.resize(count);
}

void AnimationComponent::LoadClipsFromPaths(const std::map<std::string, BoneInfo>& boneInfoMap, int boneCount)
{
    clips.clear();

    for (const auto& path : clipPaths) {
        if (path.empty()) {
            continue;
        }
        auto anim = LoadClipFromPath(path, boneInfoMap, boneCount);
        if (anim) {
            clips.emplace_back(std::move(anim));
        }
    }

    if (!clips.empty() && activeClip >= clips.size()) {
        activeClip = 0;
    }

    if (!clips.empty()) {
        EnsureAnimator();
        SyncAnimatorToActiveClip();
    }
}

void AnimationComponent::PlayClip(std::size_t clipIndex, bool loop) {
	activeClip = clipIndex;
	isLoop = loop;
	isPlay = true;
}

void AnimationComponent::PlayOnce(std::size_t clipIndex) {
	PlayClip(clipIndex, false);
}

bool AnimationComponent::IsPlaying() const {
	return isPlay;
}