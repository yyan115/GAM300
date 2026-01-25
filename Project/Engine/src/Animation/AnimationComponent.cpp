#include "pch.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Animation/AnimationComponent.hpp"
#include <Platform/IPlatform.h>
#include <WindowManager.hpp>
#include "Graphics/Model/Model.h"
#include "Asset Manager/AssetManager.hpp"

#pragma region Reflection
REFL_REGISTER_START(AnimationComponent)
	REFL_REGISTER_PROPERTY(enabled)
	REFL_REGISTER_PROPERTY(isPlay)
	REFL_REGISTER_PROPERTY(isLoop)
	REFL_REGISTER_PROPERTY(speed)
	REFL_REGISTER_PROPERTY(clipCount)
	REFL_REGISTER_PROPERTY(clipPaths)
	REFL_REGISTER_PROPERTY(clipGUIDs)
	REFL_REGISTER_PROPERTY(controllerPath)
REFL_REGISTER_END
#pragma endregion

AnimationComponent::AnimationComponent()
{
    animator = std::make_unique<Animator>(nullptr); // set clip later
}

AnimationComponent::~AnimationComponent()
{
    // Clear animator's reference to prevent dangling pointer access
    if (animator) {
        animator->ClearAnimation();
    }
    clips.clear();
}

void AnimationComponent::ClearClips()
{
    // Clear animator's reference first
    if (animator) {
        animator->ClearAnimation();
    }
    clips.clear();
    activeClip = 0;
    isPlay = false;
}


AnimationComponent::AnimationComponent(const AnimationComponent& other)
    : enabled(other.enabled)
    , isPlay(other.isPlay)
    , isLoop(other.isLoop)
    , speed(other.speed)
    , clipCount(other.clipCount)
    , clipPaths(other.clipPaths)
    , clipGUIDs(other.clipGUIDs)
    , controllerPath(other.controllerPath)
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
    swap(a.controllerPath, b.controllerPath);
    swap(a.activeClip, b.activeClip);
    swap(a.clips, b.clips);
    swap(a.animator, b.animator);
}

void AnimationComponent::Update(float dt, Entity entity)
{
	if (!animator) return;

    if (isPlay && !clips.empty() && activeClip < clips.size())
    {
        animator->UpdateAnimation(dt * speed, isLoop, entity);
        if (!isLoop)
        {
            if (clips[activeClip]) {
                const float durTicks = clips[activeClip]->GetDuration();
                if (animator->GetCurrentTime() >= durTicks) isPlay = false;
            }
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

    unsigned int postProcessFlags = aiProcess_Triangulate | aiProcess_FlipUVs;

    const aiScene* scene = importer.ReadFileFromMemory(buffer.data(), buffer.size(), postProcessFlags, "fbx");

    if (!scene) {
        ENGINE_PRINT("[Anim] Buffer size: ", buffer.size());
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

    float scaleFactor = Model::CalculateAutoScale(scene);

    // 3. MANUAL FIX: Iterate and multiply translation keys
    if (std::abs(scaleFactor - 1.0f) > 0.001f)
    {
        ENGINE_LOG_INFO("[AnimationComponent] Auto-scaling animation by " + std::to_string(scaleFactor));

        // Free the current scene
        importer.FreeScene();

        // Re-import with GlobalScale
        importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, scaleFactor);
        postProcessFlags |= aiProcess_GlobalScale;

        scene = importer.ReadFileFromMemory(buffer.data(), buffer.size(), postProcessFlags, "fbx");

        if (!scene || !scene->mRootNode || scene->mNumAnimations == 0) {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AnimationComponent] Re-import with scaling failed\n");
            return nullptr;
        }
    }

    aiAnimation* aiAnim = scene->mAnimations[0];
    return std::make_unique<Animation>(aiAnim, scene->mRootNode, boneInfoMap, boneCount);
}

void AnimationComponent::AddClipFromFile(const std::string& path, const std::map<std::string, BoneInfo>& boneInfoMap, int boneCount, Entity entity)
{
    auto anim = LoadClipFromPath(path, boneInfoMap, boneCount);
    if (!anim) return;

    clips.emplace_back(std::move(anim));

    clipPaths.push_back(path);
    // Store the GUID for cross-machine compatibility
    GUID_128 guid = AssetManager::GetInstance().GetGUID128FromAssetMeta(path);
    clipGUIDs.push_back(guid);
    clipCount = static_cast<int>(clipPaths.size());

    if(clips.size() == 1)
    {
        activeClip = 0;
        EnsureAnimator();
        animator->PlayAnimation(clips[0].get(), entity);
    }
}


void AnimationComponent::Play(Entity entity)
{
    isPlay = true;
    if (!clips.empty() && activeClip < clips.size())
    {
		EnsureAnimator();
		animator->PlayAnimation(clips[activeClip].get(), entity);
    }
}
void AnimationComponent::Pause() { isPlay = false; }

void AnimationComponent::Stop(Entity entity)
{
    isPlay = false;
    if(!clips.empty() && activeClip < clips.size() && animator)
        animator->PlayAnimation(clips[activeClip].get(), entity);
}

void AnimationComponent::SetLooping(bool v) { isLoop = v; }
void AnimationComponent::SetSpeed(float s) { speed = std::max(0.0f, s); }


void AnimationComponent::SetClip(size_t index, Entity entity)
{
    if (index >= clips.size() || index == activeClip) return;
    activeClip = index;
    SyncAnimatorToActiveClip(entity);
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

Animation& AnimationComponent::GetClip(size_t i) {
    if (i >= clips.size()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AnimationComponent] GetClip index ", i, " out of bounds (size=", clips.size(), ")\n");
        static Animation dummyAnim;
        return dummyAnim;
    }
    return *clips[i];
}
const Animation& AnimationComponent::GetClip(size_t i) const {
    if (i >= clips.size()) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AnimationComponent] GetClip const index ", i, " out of bounds (size=", clips.size(), ")\n");
        static Animation dummyAnim;
        return dummyAnim;
    }
    return *clips[i];
}
const std::vector<std::unique_ptr<Animation>>& AnimationComponent::GetClips() const { return clips; }
size_t AnimationComponent::GetActiveClipIndex() const
{
    if (clips.empty())
        return 0;
    if (activeClip >= clips.size())
        return clips.size() - 1;
    return activeClip;
}


void AnimationComponent::SyncAnimatorToActiveClip(Entity entity)
{
    if (clips.empty() || activeClip >= clips.size() || !clips[activeClip] || !animator) {
        return;
    }
    Animation* clip = clips[activeClip].get();
    animator->PlayAnimation(clip, entity);
}

void AnimationComponent::SetClipCount(size_t count)
{
    clipCount = static_cast<int>(count);
    clipPaths.resize(count);
    clipGUIDs.resize(count);
}

void AnimationComponent::LoadClipsFromPaths(const std::map<std::string, BoneInfo>& boneInfoMap, int boneCount, Entity entity)
{
    ENGINE_PRINT("[AnimationComponent] LoadClipsFromPaths: Loading ", clipPaths.size(), " clips for entity ", entity, "\n");

    // Clear animator's reference before clearing clips to prevent dangling pointer
    if (animator) {
        animator->ClearAnimation();
    }
    clips.clear();

    for (size_t i = 0; i < clipPaths.size(); ++i) {
        const auto& path = clipPaths[i];
        std::string pathToLoad{};

        // First try to use GUID to get the correct local path (handles cross-machine scenarios)
        if (i < clipGUIDs.size() && (clipGUIDs[i].high != 0 || clipGUIDs[i].low != 0)) {
            std::string guidPath = AssetManager::GetInstance().GetAssetPathFromGUID(clipGUIDs[i]);
            if (!guidPath.empty()) {
                pathToLoad = guidPath;
                ENGINE_PRINT("[AnimationComponent] Resolved path from GUID: ", pathToLoad, "\n");
            }
        }

        // Fall back to path if GUID lookup failed
        if (pathToLoad.empty() && !path.empty()) {
#ifndef EDITOR
            // Safely extract path from "Resources" onwards for game build
            size_t resPos = path.find("Resources");
            if (resPos != std::string::npos) {
                pathToLoad = path.substr(resPos);
            } else {
                pathToLoad = path;  // Use as-is if "Resources" not found
            }
#else
            pathToLoad = path;
#endif
        }

        if (pathToLoad.empty()) {
            ENGINE_PRINT("[AnimationComponent] Skipping empty path\n");
            continue;
        }

        ENGINE_PRINT("[AnimationComponent] Loading clip from: ", pathToLoad, "\n");
        auto anim = LoadClipFromPath(pathToLoad, boneInfoMap, boneCount);
        if (anim) {
            clips.emplace_back(std::move(anim));
            ENGINE_PRINT("[AnimationComponent] Successfully loaded clip, total: ", clips.size(), "\n");
        } else {
            ENGINE_PRINT(EngineLogging::LogLevel::Error, "[AnimationComponent] Failed to load clip from: ", pathToLoad, "\n");
        }
    }

    ENGINE_PRINT("[AnimationComponent] Finished loading clips, count: ", clips.size(), "\n");

    if (!clips.empty() && activeClip >= clips.size()) {
        activeClip = 0;
    }

    if (!clips.empty()) {
        EnsureAnimator();
        SyncAnimatorToActiveClip(entity);
    }
}

void AnimationComponent::PlayClip(std::size_t clipIndex, bool loop, Entity entity) {
	activeClip = clipIndex;
	isLoop = loop;
	isPlay = true;

	// Actually start playing the animation on the animator
	if (!clips.empty() && clipIndex < clips.size()) {
		EnsureAnimator();
		animator->PlayAnimation(clips[clipIndex].get(), entity);
		ENGINE_PRINT("[AnimationComponent] PlayClip: Playing clip ", clipIndex, " for entity ", entity, "\n");
	} else {
		ENGINE_PRINT(EngineLogging::LogLevel::Warn, "[AnimationComponent] PlayClip: Cannot play clip ", clipIndex,
			" - clips.size()=", clips.size(), ", entity=", entity, "\n");
	}
}

void AnimationComponent::PlayOnce(std::size_t clipIndex, Entity entity) {
	PlayClip(clipIndex, false, entity);
}

bool AnimationComponent::IsPlaying() const {
	return isPlay;
}

void AnimationComponent::ResetForPlay(Entity entity) {
	// Reset animator to beginning for fresh game start
	if (!clips.empty() && animator && activeClip < clips.size() && clips[activeClip]) {
		animator->PlayAnimation(clips[activeClip].get(), entity);
	}
}

void AnimationComponent::ResetPreview(Entity entity) {
	// Reset editor preview time to 0
	editorPreviewTime = 0.0f;
	if (!clips.empty() && animator && activeClip < clips.size() && clips[activeClip]) {
		animator->PlayAnimation(clips[activeClip].get(), entity);
	}
}

AnimationStateMachine* AnimationComponent::EnsureStateMachine()
{
    if (!stateMachine)
    {
        stateMachine = std::make_unique<AnimationStateMachine>();
        stateMachine->SetOwner(this);
    }
    return stateMachine.get();
}

// Lua-friendly parameter setters
void AnimationComponent::SetBool(const std::string& name, bool value)
{
    if (stateMachine) {
        stateMachine->GetParams().SetBool(name, value);
    }
}

void AnimationComponent::SetInt(const std::string& name, int value)
{
    if (stateMachine) {
        stateMachine->GetParams().SetInt(name, value);
    }
}

void AnimationComponent::SetFloat(const std::string& name, float value)
{
    if (stateMachine) {
        stateMachine->GetParams().SetFloat(name, value);
    }
}

void AnimationComponent::SetTrigger(const std::string& name)
{
    if (stateMachine) {
        stateMachine->GetParams().SetTrigger(name);
    }
}

// Lua-friendly parameter getters
bool AnimationComponent::GetBool(const std::string& name) const
{
    if (stateMachine) {
        return stateMachine->GetParams().GetBool(name);
    }
    return false;
}

int AnimationComponent::GetInt(const std::string& name) const
{
    if (stateMachine) {
        return stateMachine->GetParams().GetInt(name);
    }
    return 0;
}

float AnimationComponent::GetFloat(const std::string& name) const
{
    if (stateMachine) {
        return stateMachine->GetParams().GetFloat(name);
    }
    return 0.0f;
}

std::string AnimationComponent::GetCurrentState() const
{
    if (stateMachine) {
        return stateMachine->GetCurrentState();
    }
    return "";
}