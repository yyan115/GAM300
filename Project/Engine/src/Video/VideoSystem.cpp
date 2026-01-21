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
        if (!m_ecs->HasComponent<VideoComponent>(entity))
            continue;
        auto& videoComp = m_ecs->GetComponent<VideoComponent>(entity);

        //IF ASSET DIRY, reinitialise
        if (videoComp.asset_dirty)
        {
            Initialise(*m_ecs);
            videoComp.asset_dirty = false;
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
