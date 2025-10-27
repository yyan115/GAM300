#pragma once
#include "pch.h"
#include "Animation/Animator.hpp"
#include "Animation/AnimationSystem.hpp"

class ENGINE_API AnimationComponent
{
public:
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

    // UI state (can be private with getters if you prefer)
    bool  isPlay = false;
    bool  isLoop = true;
    float speed = 0.1f;

private:
    // Data
    std::vector<std::unique_ptr<Animation>> clips; // idle/walk/run…
    size_t activeClip = 0;

    // Player
    std::unique_ptr<Animator> animator;

    // Helpers
    void SyncAnimatorToActiveClip(); // call when clip changes
};