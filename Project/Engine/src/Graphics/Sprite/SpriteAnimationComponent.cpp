#include "pch.h"
#include "Graphics/Sprite/SpriteAnimationComponent.hpp"

//#pragma region Reflection
//REFL_REGISTER_START(SpriteAnimationComponent)
//REFL_REGISTER_PROPERTY(clips)
//REFL_REGISTER_PROPERTY(currentClipIndex)
//REFL_REGISTER_PROPERTY(currentFrameIndex)
//REFL_REGISTER_PROPERTY(timeInCurrentFrame)
//REFL_REGISTER_PROPERTY(playbackSpeed)
//REFL_REGISTER_PROPERTY(playing)
//REFL_REGISTER_END
//#pragma endregion

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