#pragma once
#include "Engine.h"
#include <string>
#include <memory>
#include <iostream>
#include "Sound/AudioSystem.hpp"
#include "Math/Vector3D.hpp"
#include "Asset Manager/ResourceManager.hpp" // load Audio assets via ResourceManager

// Forward declare Audio class to avoid including the header
class Audio;

struct ENGINE_API AudioComponent {
    // Public editable properties used by inspector
    std::string AudioAssetPath; // original asset path under Resources
    float Volume{ 1.0f };
    bool Loop{ false };
    bool PlayOnAwake{ false };
    bool Spatialize{ false };
    float Attenuation{ 1.0f };

    // Runtime data
    std::shared_ptr<Audio> audioAsset{ nullptr };
    ChannelHandle Channel{ 0 };

    // Optional position - when spatialize is enabled inspector / transform system should update this
    Vector3D Position{ 0.0f, 0.0f, 0.0f };

    AudioComponent();
    ~AudioComponent();

    // Called when inspector sets a new asset path
    void SetAudioAssetPath(const std::string& path);

    void Play();
    void Pause();
    void Stop();

    // Called every frame (or when transform updates) to keep 3D channel position updated
    void UpdatePosition(const Vector3D& pos);

    bool IsPlaying();

    void SetVolume(float newVolume);
    void SetPitch(float pitch);
};