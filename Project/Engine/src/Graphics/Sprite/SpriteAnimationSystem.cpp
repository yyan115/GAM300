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
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    // Initialize all sprites with their first frame
    for (const auto& entity : entities)
    {
        auto& anim = ecsManager.GetComponent<SpriteAnimationComponent>(entity);
        auto& sprite = ecsManager.GetComponent<SpriteRenderComponent>(entity);

        // If we have a valid clip with frames, set the initial frame
        if (anim.currentClipIndex >= 0 && anim.currentClipIndex < (int)anim.clips.size())
        {
            SpriteAnimationClip& clip = anim.clips[anim.currentClipIndex];
            if (!clip.frames.empty())
            {
                const SpriteFrame& frame = clip.frames[0];

                // Set initial texture
                if (frame.textureGUID != GUID_128{})
                {
                    sprite.textureGUID = frame.textureGUID;
                    sprite.texturePath = frame.texturePath;

                    // Load the texture
                    try {
                        std::string texturePath = AssetManager::GetInstance().GetAssetPathFromGUID(frame.textureGUID);
                        if (!texturePath.empty()) {
                            sprite.texture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(frame.textureGUID, texturePath);
                        } else if (!frame.texturePath.empty()) {
                            sprite.texture = ResourceManager::GetInstance().GetResource<Texture>(frame.texturePath);
                        }
                    } catch (...) {
                        if (!frame.texturePath.empty()) {
                            sprite.texture = ResourceManager::GetInstance().GetResource<Texture>(frame.texturePath);
                        }
                    }
                }

                // Set initial UV coordinates
                sprite.uvOffset = frame.uvOffset;
                sprite.uvScale = frame.uvScale;

                // Ensure sprite has a shader
                if (sprite.shaderGUID == GUID_128{}) {
                    GUID_128 spriteShaderGUID = MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("sprite"));
                    sprite.shaderGUID = spriteShaderGUID;
                    std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(spriteShaderGUID);
                    sprite.shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(spriteShaderGUID, shaderPath);
                }
            }
        }
    }

	return true;
}

void SpriteAnimationSystem::Update()
{
    bool isEditMode = Engine::IsEditMode();

    // Check if we just entered play mode
    if (wasInEditMode && !isEditMode) {
        // Just transitioned from edit to play mode
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        // Start all autoPlay animations
        for (const auto& entity : entities) {
            auto& anim = ecsManager.GetComponent<SpriteAnimationComponent>(entity);

            // Start playing if autoPlay is true and we have a valid clip
            if (anim.enabled && anim.autoPlay && anim.currentClipIndex >= 0) {
                anim.playing = true;
                anim.currentFrameIndex = 0;
                anim.timeInCurrentFrame = 0.0f;
            }
        }
    }

    wasInEditMode = isEditMode;

    if(isEditMode) {
        return;
	}

    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    float dt = static_cast<float>(TimeManager::GetDeltaTime());

    for (const auto& entity : entities)
    {
        // Skip entities that are inactive in hierarchy (checks parents too)
        if (!ecsManager.IsEntityActiveInHierarchy(entity)) {
            continue;
        }

        auto& anim = ecsManager.GetComponent<SpriteAnimationComponent>(entity);
		auto& sprite = ecsManager.GetComponent<SpriteRenderComponent>(entity);

        // Skip if component is disabled or not playing
        if (!anim.enabled || !anim.playing || anim.currentClipIndex < 0)
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

        // Ensure sprite has a shader if it doesn't have one
        if (sprite.shaderGUID == GUID_128{}) {
            // Set default sprite shader
            GUID_128 spriteShaderGUID = MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("sprite"));
            sprite.shaderGUID = spriteShaderGUID;
            std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(spriteShaderGUID);
            sprite.shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(spriteShaderGUID, shaderPath);
        }

        // Update texture if different
        if (frame.textureGUID != GUID_128{})
        {
            if (frame.textureGUID != sprite.textureGUID) {
                sprite.textureGUID = frame.textureGUID;
                sprite.texturePath = frame.texturePath; // Use the stored path directly

                // Try to load texture from GUID first, then from path
                try {
                    std::string texturePath = AssetManager::GetInstance().GetAssetPathFromGUID(frame.textureGUID);
                    if (!texturePath.empty()) {
                        sprite.texture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(frame.textureGUID, texturePath);
                    } else if (!frame.texturePath.empty()) {
                        // Fall back to using the stored path
                        sprite.texture = ResourceManager::GetInstance().GetResource<Texture>(frame.texturePath);
                    }
                } catch (...) {
                    // If GUID lookup fails, try direct path
                    if (!frame.texturePath.empty()) {
                        sprite.texture = ResourceManager::GetInstance().GetResource<Texture>(frame.texturePath);
                    }
                }

                // Verify texture was loaded
                if (!sprite.texture) {
                    ENGINE_LOG_ERROR("Failed to load texture for sprite animation frame: GUID=" + GUIDUtilities::ConvertGUID128ToString(frame.textureGUID) + ", Path=" + frame.texturePath);
                }
            }
		}

        // Always update UV coordinates
        sprite.uvOffset = frame.uvOffset;
        sprite.uvScale = frame.uvScale;
    }
}