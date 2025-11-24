#pragma once
#include "pch.h"
#include "Animation/Animator.hpp"
#include "Animation/AnimationSystem.hpp"
#include "Animation/AnimationStateMachine.hpp"
#include "Reflection/ReflectionBase.hpp"
#include "Utilities/GUID.hpp"

class ENGINE_API AnimationComponent
{
public:
	REFL_SERIALIZABLE

	AnimationComponent();

    ~AnimationComponent() = default;

    AnimationComponent(const AnimationComponent& other);

    AnimationComponent& operator=(AnimationComponent other) noexcept;

    friend void swap(AnimationComponent& a, AnimationComponent& b) noexcept;

    // also keep move ops (optional)
    AnimationComponent(AnimationComponent&&) noexcept = default;
    AnimationComponent& operator=(AnimationComponent&&) noexcept = default;

    // Per-frame from your engine/editor
    void Update(float dt);           // advances Animator if playing

    // Editor-facing controls
    void Play();
    void Pause();
    void Stop();                     // reset to start
    void SetLooping(bool v);
    void SetSpeed(float s);
    void SetClip(size_t index);      // choose a different clip

	// Load Animation from file and add to clips
	void AddClipFromFile(const std::string& path, const std::map<std::string, BoneInfo>& boneInfoMap, int boneCount);

    // Access for systems that need it
    Animator& GetAnimator();
    const Animator& GetAnimator() const;
	Animator* GetAnimatorPtr();
	const Animator* GetAnimatorPtr() const;
	Animator* EnsureAnimator(); // create if missing

    Animation& GetClip(size_t i);
    const Animation& GetClip(size_t i) const;
    const std::vector<std::unique_ptr<Animation>>& GetClips() const;
    size_t GetActiveClipIndex() const;

    void LoadClipsFromPaths(const std::map<std::string, BoneInfo>& boneInfoMap, int boneCount);
    void SetClipCount(size_t count);

    void PlayClip(std::size_t clipIndex, bool loop);
	void PlayOnce(std::size_t clipIndex);
    bool IsPlaying() const;

    bool  enabled = true;
    bool  isPlay = false;
    bool  isLoop = true;
    float speed = 1.0f;
    int clipCount = 0;
    std::vector<std::string> clipPaths;
    std::vector<GUID_128> clipGUIDs;

    // Editor preview state (NOT serialized - only for inspector preview)
    float editorPreviewTime = 0.0f;  // Separate time for inspector preview

    // Runtime control
    void ResetForPlay();  // Reset animator to 0 for fresh game start
    void ResetPreview();  // Reset preview time to 0


	// StateMachine
    AnimationStateMachine*          GetStateMachine()       { return stateMachine.get(); }
    const AnimationStateMachine*    GetStateMachine() const { return stateMachine.get(); }

    // Helper to lazily allocate FSM:
    AnimationStateMachine* EnsureStateMachine()
    {
        if (!stateMachine)
        {
            stateMachine = std::make_unique<AnimationStateMachine>();
            stateMachine->SetOwner(this);
        }
        return stateMachine.get();
    }

private:
    std::vector<std::unique_ptr<Animation>> clips;
    size_t activeClip = 0;

    std::unique_ptr<Animator> animator;

	std::unique_ptr<AnimationStateMachine> stateMachine;

    void SyncAnimatorToActiveClip();
    std::unique_ptr<Animation> LoadClipFromPath(const std::string& path, const std::map<std::string, BoneInfo>& boneInfoMap, int boneCount);
};