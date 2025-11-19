#pragma once
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Reflection/ReflectionBase.hpp"
#include "Utilities/GUID.hpp"

struct SpriteFrame
{
	GUID_128 textureGUID; // Texture for this frame
	std::string texturePath; // Path to the texture (for reference)
	glm::vec2 uvOffset;   // bottom-left or top-left depending on your convention
	glm::vec2 uvScale;    // size of the frame in UV space
	float duration = 0.1f; // seconds this frame is shown
};

struct SpriteAnimationClip
{
	std::string name;
	std::vector<SpriteFrame> frames;
	bool loop = true;
};


class SpriteAnimationComponent
{
public:
	//REFL_SERIALIZABLE
    SpriteAnimationComponent() = default;

    std::vector<SpriteAnimationClip> clips;

    int currentClipIndex = -1;
    int currentFrameIndex = 0;

    float timeInCurrentFrame = 0.0f;  // timer for current frame
    float playbackSpeed = 1.0f;       // 1.0 = normal speed

    bool playing = true;

    void Play(const std::string& clipName, bool restartIfSame = false);
    void Stop() { playing = false; }
    void Resume() { playing = true; }
};