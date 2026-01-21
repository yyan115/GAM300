#pragma once

#include "pch.h"
#include "ECS/System.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Video/cutscene.hpp"

//#include "Video/VideoComponent.hpp" 

struct VideoComponent;
class VideoSystem : public System{
public:
    VideoSystem() = default;
    ~VideoSystem() = default;


    // 1. System-level Init 
    void Initialise(ECSManager& ecsManager);

    //GET DT ONLY THE REST GET FROM INSIDE THE FUNCTION ITSELF
    //void Update(VideoComponent& component, const Asset::CutsceneInfo& info, float dt);
    void Update(float dt);


    /**
     * @brief Initializes a VideoComponent with data from a CutsceneInfo asset.
     */
    ////void InitializeCutscene(VideoComponent& component, const Asset::CutsceneInfo& info);
    //void InitializeCutscene();

    void SwapCutscene(SpriteRenderComponent& comp, std::string newCutscenePath);


    /**
     * @brief Checks if the cutscene has finished all phases (including post-time).
     */
    bool IsFinished(const VideoComponent& component, const Asset::CutsceneInfo& info) const;

private:
    // Internal helper to handle the transitions between Pre-time -> Duration -> Post-time
    void AdvanceCutsceneState(VideoComponent& component, const Asset::CutsceneInfo& info, float dt);
    ECSManager* m_ecs = nullptr;
};