#pragma once

#include "pch.h"
#include "ECS/ECSManager.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Video/VideoSystem.hpp"
#include "Video/VideoComponent.hpp" 
#include "Performance/PerformanceProfiler.hpp"
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Input/InputManager.h"
#include "ECS/TagManager.hpp"
#include "ECS/TagComponent.hpp"
#include "Video/cutscenelayer.hpp"
#include "Scene/SceneManager.hpp"

//HELPER FUNCTIONS
float lerp(float start, float end, float time)
{
    return start + time * (end - start);
}




void VideoSystem::Initialise(ECSManager& ecsManager) {
     std::cout << "VideoSystem Initialised" << std::endl;
     m_ecs = &ecsManager;
}

void VideoSystem::Update(float dt) {
    PROFILE_FUNCTION();

    int dialogueTagIndex = TagManager::GetInstance().GetTagIndex("DialogueText");
    int dialogueBoxTagIndex = TagManager::GetInstance().GetTagIndex("DialogueBox");
    int blackScreenIndex = TagManager::GetInstance().GetTagIndex("BlackScreen");
    int skipButtonIndex = TagManager::GetInstance().GetTagIndex("SkipButton");

    //GET ENTITY VIA TAG
    for (const auto& entity : m_ecs->GetAllEntities())
    {
        if (foundBlackScreen && foundDialogueBox && foundDialogueText)
        {
            break;
        }

        //FIND DialogueText Entity
        if (!foundDialogueText && 
            m_ecs->HasComponent<TextRenderComponent>(entity) &&
            m_ecs->GetComponent<TagComponent>(entity).tagIndex == dialogueTagIndex)
        
        {
                foundDialogueText = true;
                dialogueText_Entity = entity;
        }

        //FIND DIALOGUEBOX ENTITY
        if (!foundDialogueBox &&
            m_ecs->HasComponent<TextRenderComponent>(entity) &&
            m_ecs->GetComponent<TagComponent>(entity).tagIndex == dialogueBoxTagIndex)

        {
            foundDialogueBox = true;
            dialogueBox_Entity = entity;
        }


        //FIND BLACKSCREEN ENTITY FOR TRANSITION 

        if (!foundBlackScreen &&
            m_ecs->HasComponent<SpriteRenderComponent>(entity) &&
            m_ecs->GetComponent<TagComponent>(entity).tagIndex == blackScreenIndex)
        {
            foundBlackScreen = true;
            blackScreen_Entity = entity;
        }

        if (!foundSkipButton &&
            m_ecs->HasComponent<SpriteRenderComponent>(entity) &&
            m_ecs->GetComponent<TagComponent>(entity).tagIndex == skipButtonIndex)
        {
            foundSkipButton = true;
            skipButton_Entity = entity;
        }


    }
    //ITS FINE IF CANNOT FIND, BUT NEED MAKE SURE NOT TO ACCESS IT (DIALOGUE BOX + DIALOGUE TEXT)
   //but for now we just assume its found.

    if (m_ecs->HasComponent<TextRenderComponent>(dialogueText_Entity) == false)
        return;

    //GET RESPECTIVE COMPONENT
    auto& textComp      = m_ecs->GetComponent<TextRenderComponent>(dialogueText_Entity);
    auto& textTransform = m_ecs->GetComponent<Transform>(dialogueText_Entity);
    auto& blackScreenSprite = m_ecs->GetComponent<SpriteRenderComponent>(blackScreen_Entity);
    auto& skipButtonSprite = m_ecs->GetComponent<SpriteRenderComponent>(skipButton_Entity);


    //GET THE VIDEO COMP AND SPRITE COMPONENT FROM THE ENTITY
    for (const auto& entity : entities)
    {

        //SKIP ENTITIES THAT DOES NOT HAVE VIDEO COMPONENT
        if (!m_ecs->HasComponent<VideoComponent>(entity) || !m_ecs->HasComponent<SpriteRenderComponent>(entity))
            continue;
        auto& videoComp = m_ecs->GetComponent<VideoComponent>(entity);
        auto& spriteComp = m_ecs->GetComponent<SpriteRenderComponent>(entity);

        //IF CHANGE ASSET, SET ASSET AS DIRTY. HOWEVER IF NEVER CHANGE ASSET, (CLICK PLAY) MUST ALSO SET TO ASSET AS DIRTY

        //SET UP THE FIRST CUTSCENE
        if (videoComp.asset_dirty)
        {
            videoComp.activeFrame = videoComp.frameStart;
            videoComp.currentTime = 0;
            videoComp.asset_dirty = false;
            std::string firstPath = ConstructNewPath(videoComp);
            m_dialogueManager.Reset();
            SwapCutscene(spriteComp, firstPath);
            isTransitioning = true;
            videoComp.cutsceneEnded = false;
            internalCutsceneEnded = false;
        }
        videoComp.currentTime += dt;

//LOGIC TO CHANGE SCENE
#ifdef autoPlay
        //if (videoComp.currentTime > renderTime)
        //{
        //    videoComp.currentTime = 0;  
        //    
        //    if (videoComp.activeFrame < videoComp.frameEnd)
        //    {
        //        videoComp.activeFrame += 1;
        //        std::string newCutScenePath = ConstructNewPath(videoComp);

        //        SwapCutscene(spriteComp, newCutScenePath);
        //    }
        //}
#endif 

        //HANDLE TRANSITION
        if (isTransitioning)
        {
            if (internalCutsceneEnded)
            {
                FadeOutTransition(blackScreenSprite, dt, videoComp.preTime);
                if (blackScreenSprite.alpha >= 0.99f)
                {
                    std::cout << "am i seein this right? " << blackScreenSprite.alpha << std::endl;
                    blackScreenSprite.alpha = 1.0f;
                    isTransitioning = false;
                    videoComp.cutsceneEnded = true;     //set this to true, handle in lua script.
                    //SceneManager::GetInstance().LoadScene(sceneToLoad, true);
                }
            }
            else
            {
                FadeInTransition(blackScreenSprite, dt, videoComp.postTime);
                if (blackScreenSprite.alpha <= 0.01)
                {
                    blackScreenSprite.alpha = 0;
                    isTransitioning = false;
                }
            }
        }

        //SKIP BUTTON INPUT
        //if (skipButtonSprite.isVisible && InputManager::GetMouseButtonDown(Input::MouseButton::LEFT))
        if (skipButtonSprite.isVisible && g_inputManager->IsPointerJustPressed())
        {
            internalCutsceneEnded = true;
            //videoComp.cutsceneEnded = true; //USE THIS INSTEAD, REPLACE ALL CUT SCENE ENDED
            isTransitioning = true;
        }


        //BLOCK INPUT IF SCENE IS OVER / TRANSITITION STATE
        if (internalCutsceneEnded)
            return;
        if(g_inputManager->IsPointerJustPressed())
        {
            //IF CURRENT SCENE HAS YET TO FINISH RENDERING ALL THE DIALOGUE, FINISH IT AND CONTINUE; DO NOT SWAP CUTSCENE
            //This only applies to the start transition since end transition is blocked
            if (isTransitioning)
            {
                blackScreenSprite.alpha = 0;        //make it 0 immediately
                isTransitioning = false;
            }



            bool goNext = m_dialogueManager.IsTextFinished(textComp,videoComp.activeFrame);
            if (goNext)
            {
                videoComp.activeFrame += 1;
                if (videoComp.activeFrame > videoComp.frameEnd)
                {
                    videoComp.activeFrame = videoComp.frameEnd; //just to fix it in place
                    internalCutsceneEnded = true;
                    //videoComp.cutsceneEnded = true;
                    isTransitioning = true;     //for fade out transition
                }
                else
                {
                    std::string newCutScenePath = ConstructNewPath(videoComp);
                    SwapCutscene(spriteComp, newCutScenePath);
                }
            }
            else //Render text immediately
            {
                m_dialogueManager.HandleTextRender(dt, textComp, textTransform, videoComp.activeFrame, true);
            }
        }

        //TEXT HANDLING
        m_dialogueManager.dialogueMap = videoComp.dialogueMap;  //pass the dialogueMap
        m_dialogueManager.HandleTextRender(dt, textComp, textTransform, videoComp.activeFrame);

        // MAYBE CAN HAVE A AUTO UI SO IF CLICKED ON -> JUST AUTO LET IT PLAY OUT

    }

}

void VideoSystem::SwapCutscene(SpriteRenderComponent& comp, std::string newCutscenePath)
{    
    comp.texture = ResourceManager::GetInstance().GetResource<Texture>(newCutscenePath); //Updating the actual texture
    //comp.textureGUID = targetGUID;      //for saving
    comp.texturePath = newCutscenePath;     //for display purpose
}

std::string VideoSystem::ConstructNewPath(VideoComponent& videoComp)
{
    std::string numResult = "_" + videoComp.PadNumber(videoComp.activeFrame);
    std::string fileName = videoComp.cutSceneName + numResult + ".png";

    std::string newCutscenePath = rootDirectory + fileName;
    return newCutscenePath;
}


void VideoSystem::FadeInTransition(SpriteRenderComponent& blackScreen, float dt, float preTime)
{
    if (preTime == 0)
        return;

    float lerpSpeed = 1.0f / preTime;
    int endTransparency = 0;

    blackScreen.alpha = lerp(blackScreen.alpha, endTransparency, lerpSpeed * dt);
}

void VideoSystem::FadeOutTransition(SpriteRenderComponent& blackScreen, float dt, float postTime)
{
    if (postTime == 0)
        return;
    float lerpSpeed = 1.0f / postTime;
    int endTransparency = 1;
    blackScreen.alpha = lerp(blackScreen.alpha,endTransparency, lerpSpeed* dt);
}

/*
THINGS TO BE FIXED:
TEXT DIALOGUE (ENSURE TEXT FITS IN DIALOGUE BOX)
PROPER LOADING OF ASSETS (SOMETIMES NEED TO GO INTO THE FOLDER OPEN UP THEN LOAD PROPERLY)
*/