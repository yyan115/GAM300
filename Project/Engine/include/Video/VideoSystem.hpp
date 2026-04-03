#pragma once

#include "ECS/System.hpp"

class ECSManager;
struct VideoComponent;

class VideoSystem : public System {
public:
    VideoSystem() = default;
    ~VideoSystem() = default;

    void Initialise(ECSManager& ecsManager);
    void Update(float dt);

private:
    ECSManager* m_ecs = nullptr;

    // Core operations
    void ResolveEntityReferences(VideoComponent& vc);
    void BeginCutscene(VideoComponent& vc);
    void AdvanceToBoard(VideoComponent& vc, int boardIndex);
    void BeginEndingFade(VideoComponent& vc);
    void FinishCutscene(VideoComponent& vc);

    // Per-frame helpers
    void UpdateTypewriter(VideoComponent& vc, float dt);
    void CompleteTypewriter(VideoComponent& vc);
    bool IsTypewriterFinished(const VideoComponent& vc) const;
    void ApplyBlur(VideoComponent& vc, float dt);
    void SwapBoardImage(VideoComponent& vc, int boardIndex);
    void PlayBoardSFX(VideoComponent& vc, int boardIndex);
    void StopBoardSFX(VideoComponent& vc);
    void SetBlackScreenAlpha(VideoComponent& vc, float alpha);
    void ClearText(VideoComponent& vc);

    // Easing
    static float Smoothstep(float t);
};
