#pragma once

#include "pch.h"
#include "ECS/ECSManager.hpp"
#include "ECS/ECSRegistry.hpp"
#include "Video/VideoSystem.hpp"
#include "Video/VideoComponent.hpp" 
#include "Performance/PerformanceProfiler.hpp"
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Asset Manager/AssetManager.hpp"

void VideoSystem::Initialise(ECSManager& ecsManager) {
     std::cout << "VideoSystem Initialised" << std::endl;
     m_ecs = &ecsManager;
}

void VideoSystem::Update(float dt) {
    PROFILE_FUNCTION();

    for (const auto& entity : entities)
    {
        //SKIP ENTITIES THAT DOES NOT HAVE VIDEO COMPONENT
        if (!m_ecs->HasComponent<VideoComponent>(entity) || !m_ecs->HasComponent<SpriteRenderComponent>(entity))
            continue;
        auto& videoComp = m_ecs->GetComponent<VideoComponent>(entity);
        auto& spriteComp = m_ecs->GetComponent<SpriteRenderComponent>(entity);


        //std::cout << "\n========== CUTSCENE INITIALIZED ==========" << std::endl;
        //std::cout << "Name       : " << videoComp.cutSceneName << std::endl;
        //std::cout << "Frames     : " << videoComp.frameStart << " -> " << videoComp.frameEnd << std::endl;
        //std::cout << "Pre-Time   : " << videoComp.preTime << "s" << std::endl;
        //std::cout << "Duration   : " << videoComp.duration << "s" << std::endl;
        //std::cout << "Post-Time  : " << videoComp.postTime << "s" << std::endl;
        //std::cout << "Path       : " << videoComp.videoPath << std::endl;
        //std::cout << "==========================================\n" << std::endl;


        float renderTime = 3.00f;

        //increment activeFrame -> set up the new string -> pass it into spriteComp (can call swap cutscene)

        //IF CHANGE ASSET, SET ASSET AS DIRTY. HOWEVER IF NEVER CHANGE ASSET, (CLICK PLAY) MUST ALSO SET TO ASSET AS DIRTY

        //SET UP THE FIRST CUTSCENE
        if (videoComp.asset_dirty)
        {
            videoComp.activeFrame = videoComp.frameStart;
            videoComp.currentTime = 0;
            videoComp.asset_dirty = false;
            std::string firstPath = ConstructNewPath(videoComp);
            SwapCutscene(spriteComp, firstPath);
        }

        videoComp.currentTime += dt;


        if (videoComp.currentTime > renderTime)
        {
            videoComp.currentTime = 0;  
            std::cout << "NEXT" << std::endl;
            
            if (videoComp.activeFrame < videoComp.frameEnd)
            {
                videoComp.activeFrame += 1;
                std::string newCutScenePath = ConstructNewPath(videoComp);
                SwapCutscene(spriteComp, newCutScenePath);
            }

           
        }





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

