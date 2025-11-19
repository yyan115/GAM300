#pragma once
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Reflection/ReflectionBase.hpp"
#include "Utilities/GUID.hpp"

struct SpriteFrame
{
	REFL_SERIALIZABLE
	GUID_128 textureGUID; // Texture for this frame
	std::string texturePath; // Path to the texture (for reference)
	glm::vec2 uvOffset;   // bottom-left or top-left depending on your convention
	glm::vec2 uvScale;    // size of the frame in UV space
	float duration = 0.1f; // seconds this frame is shown
};

struct SpriteAnimationClip
{
	REFL_SERIALIZABLE
	std::string name;
	std::vector<SpriteFrame> frames;
	bool loop = true;
};


class ENGINE_API SpriteAnimationComponent
{
public:
	REFL_SERIALIZABLE
    SpriteAnimationComponent() = default;

    std::vector<SpriteAnimationClip> clips;

    int currentClipIndex = -1;
    int currentFrameIndex = 0;

    float timeInCurrentFrame = 0.0f;  // timer for current frame
    float playbackSpeed = 1.0f;       // 1.0 = normal speed

    bool playing = false;  // Start with animation not playing by default
    bool enabled = true;  // Enable/disable the component
    bool autoPlay = true;  // Automatically play on scene start

    // Editor preview state (NOT serialized - only for inspector preview)
    float editorPreviewTime = 0.0f;  // Separate time for inspector preview
    int editorPreviewFrameIndex = 0;  // Current frame in editor preview

    void Play(const std::string& clipName, bool restartIfSame = false);
    void Stop() { playing = false; }
    void Resume() { playing = true; }
};