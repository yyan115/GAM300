#pragma once
#include "pch.h"
#include "Animation/Animator.hpp"

class AnimationComponent
{
public:
    // Construction: pass the rigged Model so Animation can resolve bones
    AnimationComponent(Model* model);

    // Per-frame from your engine/editor
    void Update(float dt);           // advances Animator if playing

    // Editor-facing controls
    void Play();
    void Pause();
    void Stop();                     // reset to start
    void SetLooping(bool v);
    void SetSpeed(float s);
    void SetClip(size_t index);      // choose a different clip

    // Access for systems that need it
    Animator& GetAnimator();
    const Animator& GetAnimator() const;
    Animation& GetClip(size_t i);
    const Animation& GetClip(size_t i) const;
    const std::vector<std::unique_ptr<Animation>>& GetClips() const;
    size_t GetActiveClipIndex() const;

    // UI state (can be private with getters if you prefer)
    bool  isPlay = false;
    bool  isLoop = true;
    float speed = 1.0f;

private:
    // Data
    std::vector<std::unique_ptr<Animation>> clips; // idle/walk/run…
    size_t activeClip = 0;

    // Player
    std::unique_ptr<Animator> animator;
    Model* model = nullptr;

    // Helpers
    void SyncAnimatorToActiveClip(); // call when clip changes
};