#include "pch.h"
#include "Graphics/Sprite/SpriteAnimationSystem.hpp"
#include "Graphics/Sprite/SpriteAnimationComponent.hpp"
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ActiveComponent.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "TimeManager.hpp"
//#include "Engine.h"

bool SpriteAnimationSystem::Initialise()
{
	return true;
}

void SpriteAnimationSystem::Update()
{
    if(Engine::IsEditMode()) {
        return;
	}

    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    float dt = static_cast<float>(TimeManager::GetDeltaTime());   // replace with your engine's call

    for (const auto& entity : entities)
    {
        auto& anim = ecsManager.GetComponent<SpriteAnimationComponent>(entity);
		auto& sprite = ecsManager.GetComponent<SpriteRenderComponent>(entity);

        if (!anim.playing || anim.currentClipIndex < 0)
            continue;

        if (anim.currentClipIndex >= (int)anim.clips.size())
            continue;

        SpriteAnimationClip& clip = anim.clips[anim.currentClipIndex];
        if (clip.frames.empty())
            continue;

        anim.timeInCurrentFrame += dt * anim.playbackSpeed;


        while (anim.timeInCurrentFrame >= clip.frames[anim.currentFrameIndex].duration)
        {
            anim.timeInCurrentFrame -= clip.frames[anim.currentFrameIndex].duration;
            anim.currentFrameIndex++;

            if (anim.currentFrameIndex >= (int)clip.frames.size())
            {
                if (clip.loop)
                {
                    anim.currentFrameIndex = 0;
                }
                else
                {
                    anim.currentFrameIndex = (int)clip.frames.size() - 1;
                    anim.playing = false; // stop at last frame
                    break;
                }
            }
        }

        const SpriteFrame& frame = clip.frames[anim.currentFrameIndex];

        if (frame.textureGUID != GUID_128{} && frame.textureGUID != sprite.textureGUID)
        {
			sprite.textureGUID = frame.textureGUID;
			std::string texturePath = AssetManager::GetInstance().GetAssetPathFromGUID(frame.textureGUID);
            sprite.texturePath = texturePath;
            sprite.texture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(frame.textureGUID, texturePath);
		}

        sprite.uvOffset = frame.uvOffset;
        sprite.uvScale = frame.uvScale;
    }
}