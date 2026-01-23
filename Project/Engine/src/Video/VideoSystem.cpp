#pragma once

#include "pch.h"
#include "ECS/ECSManager.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Video/VideoSystem.hpp"
#include "Video/VideoComponent.hpp" 
#include "Performance/PerformanceProfiler.hpp"
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Input/InputManager.hpp"
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
    //GET ENTITY VIA TAG
    for (const auto& entity : entities)
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

    }
    //ITS FINE IF CANNOT FIND, BUT NEED MAKE SURE NOT TO ACCESS IT (DIALOGUE BOX + DIALOGUE TEXT)
   //but for now we just assume its found.


    //GET RESPECTIVE COMPONENT FOR DIALOGUE BOX AND TEXT
    auto& textComp      = m_ecs->GetComponent<TextRenderComponent>(dialogueText_Entity);
    auto& textTransform = m_ecs->GetComponent<Transform>(dialogueText_Entity);
    auto& blackScreenSprite = m_ecs->GetComponent<SpriteRenderComponent>(blackScreen_Entity);

    //HANDLE TRANSITION
    if (isTransitioning)
    {
        if (cutSceneEnded)
        {
            std::cout << "BlackScreenALpha here for fade out is " << blackScreenSprite.alpha << std::endl;
            FadeOutTransition(blackScreenSprite, dt);
            if (blackScreenSprite.alpha >= 0.99f)
            {
                blackScreenSprite.alpha = 1.0f;
                isTransitioning = false;
                // Logic for what happens after the screen is black:
                // e.g., ChangeScene("MainMenu");
                //CHANGE SCENE TO PLAY
                SceneManager::GetInstance().LoadScene(sceneToLoad);
            }
        }
        else
        {
            std::cout << "BlackScreenALpha here is " << blackScreenSprite.alpha << std::endl;
            FadeInTransition(blackScreenSprite, dt);
            if (blackScreenSprite.alpha <= 0.01)
            {
                blackScreenSprite.alpha = 0;
                isTransitioning = false;
            }
        }
    }

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
            cutSceneEnded = false;
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

        //CHANGE BACK TO MOUSE BUTTON LEFT LATER
        //if (InputManager::GetMouseButtonDown(Input::MouseButton::LEFT))
        if (InputManager::GetKeyDown(Input::Key::SPACE))
        {
            //IF CURRENT SCENE HAS YET TO FINISH RENDERING ALL THE DIALOGUE, FINISH IT AND CONTINUE; DO NOT SWAP CUTSCENE

            bool goNext = m_dialogueManager.IsTextFinished(textComp,videoComp.activeFrame);
            if (goNext)
            {
                videoComp.activeFrame += 1;
                if (videoComp.activeFrame > videoComp.frameEnd)
                {
                    videoComp.activeFrame = videoComp.frameEnd; //just to fix it in place
                    cutSceneEnded = true;
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
    auto& assetMgr = AssetManager::GetInstance();

    //GET GUID FROM PATH
    GUID_128 targetGUID = assetMgr.GetGUID128FromAssetMeta(newCutscenePath);


    comp.texture = assetMgr.LoadByGUID<Texture>(targetGUID);        //Updating the actual texture
    comp.textureGUID = targetGUID;      //for saving
    comp.texturePath = newCutscenePath;     //for display purpose
}

std::string VideoSystem::ConstructNewPath(VideoComponent& videoComp)
{
    std::string numResult = "_" + videoComp.PadNumber(videoComp.activeFrame);
    std::string fileName = videoComp.cutSceneName + numResult + ".png";

    std::string newCutscenePath = rootDirectory + fileName;
    return newCutscenePath;
}


void VideoSystem::FadeInTransition(SpriteRenderComponent& blackScreen, float dt)
{
    //SET BLACK SCREEN SORTING ORDER TO HIGHEST (MAYBE SET LIKE 10)
    // SET TRANSPARENCY 255.
    //LERP TRANSPARENCY FROM 255 TO 0 
    float lerpSpeed = 1.0f;
    int endTransparency = 0;

    blackScreen.alpha = lerp(blackScreen.alpha, endTransparency, lerpSpeed * dt);
    std::cout << "FADE IN TRANSITION HAPPENING" << std::endl;
}

void VideoSystem::FadeOutTransition(SpriteRenderComponent& blackScreen, float dt)
{
    //SET BLACK SCREEN SORTING ORDER TO HIGHEST (MAYBE SET LIKE 10)
    // SET TRANSPARENCY 255.
    //LERP TRANSPARENCY FROM 255 TO 0 
    float lerpSpeed = 1.0f;
    int endTransparency = 1;
    blackScreen.alpha = lerp(blackScreen.alpha,endTransparency, lerpSpeed* dt);
}




//LERP SPRITE COMPONENT ALPHA, -> TRANSITION
/*
TODO: MAKE TEXT FILE SAVEABLE
TEXT CONSTRAINT INTO THE DIALOGUE BOX
FADE IN/ FADE OUT
SKIP BUTTON + INSTANT TEXT RENDER
DELAY SWITCH?
*/