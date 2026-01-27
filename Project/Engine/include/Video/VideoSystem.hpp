#pragma once

#include "pch.h"
#include "ECS/System.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Video/cutscene.hpp"
#include "Video/cutscenelayer.hpp"
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

    void FadeInTransition(SpriteRenderComponent& blackScreen, float dt, float preTime);
    void FadeOutTransition(SpriteRenderComponent& blackScreen, float dt, float postTime);


private:
    ECSManager* m_ecs = nullptr;

    std::string rootDirectory   = "../../Resources/Cutscenes/Kusane_OpeningCutscene/";
    std::string sceneToLoad     = "Resources/Scenes/04_GameLevel.scene";     //PROBABLY MAKE THIS A FIELD INSTEAD, DRAG SCENE TO BE LOADED

    std::string ConstructNewPath(VideoComponent& videoComp);

    DialogueManager m_dialogueManager;

    bool foundDialogueText = false;
    bool foundDialogueBox = false;
    bool foundBlackScreen = false;
    bool foundSkipButton = false;
    bool isTransitioning = false;

    Entity dialogueText_Entity = -1;
    Entity dialogueBox_Entity = -1;
    Entity blackScreen_Entity = -1;
    Entity skipButton_Entity = -1;
};