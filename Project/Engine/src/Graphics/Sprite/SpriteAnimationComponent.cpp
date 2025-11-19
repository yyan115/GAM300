#include "pch.h"
#include "Graphics/Sprite/SpriteAnimationComponent.hpp"

#pragma region Reflection
// Register SpriteFrame
REFL_REGISTER_START(SpriteFrame)
REFL_REGISTER_PROPERTY(textureGUID)
REFL_REGISTER_PROPERTY(texturePath)
// Note: uvOffset and uvScale (glm::vec2) are not registered due to linker issues
// They will need to be handled separately if serialization is required
REFL_REGISTER_PROPERTY(duration)
REFL_REGISTER_END

// Register SpriteAnimationClip
REFL_REGISTER_START(SpriteAnimationClip)
REFL_REGISTER_PROPERTY(name)
REFL_REGISTER_PROPERTY(frames)
REFL_REGISTER_PROPERTY(loop)
REFL_REGISTER_END

// Register SpriteAnimationComponent
REFL_REGISTER_START(SpriteAnimationComponent)
REFL_REGISTER_PROPERTY(clips)
REFL_REGISTER_PROPERTY(currentClipIndex)
REFL_REGISTER_PROPERTY(currentFrameIndex)
REFL_REGISTER_PROPERTY(timeInCurrentFrame)
REFL_REGISTER_PROPERTY(playbackSpeed)
REFL_REGISTER_PROPERTY(playing)
REFL_REGISTER_PROPERTY(enabled)
REFL_REGISTER_PROPERTY(autoPlay)
REFL_REGISTER_END
#pragma endregion

void SpriteAnimationComponent::Play(const std::string& clipName, bool restartIfSame)
{
    int index = -1;
    for (int i = 0; i < (int)clips.size(); ++i)
    {
        if (clips[i].name == clipName)
        {
            index = i;
            break;
        }
    }

    if (index == -1)
        return; // not found

    if (!restartIfSame && currentClipIndex == index)
        return;

    currentClipIndex = index;
    currentFrameIndex = 0;
    timeInCurrentFrame = 0.0f;
    playing = true;
}