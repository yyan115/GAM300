/* Start Header ************************************************************************/
/*!
\file       CameraComponent.hpp
\author     TAN SHUN ZHI, Tomy, t.shunzhitomy, 2301341, t.shunzhitomy@digipen.edu
\date
\brief

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include "Reflection/ReflectionBase.hpp"

class Texture;

enum class ProjectionType {
    PERSPECTIVE,
    ORTHOGRAPHIC
};

enum class CameraClearFlags {
    Skybox,
    SolidColor,
    DepthOnly,
    DontClear
};

class CameraComponent {
public:
    REFL_SERIALIZABLE

    bool enabled = true;      // Component enabled state (can be toggled in inspector)
    bool isActive = false;    // Which camera is currently rendering (managed by CameraSystem)
    int priority = 0;

    glm::vec3 target = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    float yaw = -90.0f;
    float pitch = 0.0f;
    bool useFreeRotation = true;

    ProjectionType projectionType = ProjectionType::PERSPECTIVE;
    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    float orthoSize = 10.0f;

    float movementSpeed = 2.5f;
    float mouseSensitivity = 0.1f;

    // Zoom settings
    float minZoom = 1.0f;     // Min FOV for perspective / min ortho size
    float maxZoom = 90.0f;    // Max FOV for perspective / max ortho size
    float zoomSpeed = 5.0f;   // How fast to zoom

    // Camera shake settings
    bool isShaking = false;
    float shakeIntensity = 0.0f;
    float shakeDuration = 0.0f;
    float shakeTimer = 0.0f;
    float shakeFrequency = 25.0f;  // How fast the shake oscillates
    glm::vec3 shakeOffset = glm::vec3(0.0f);  // Current shake offset

    CameraClearFlags clearFlags = CameraClearFlags::Skybox;
    glm::vec3 backgroundColor = glm::vec3(0.192f, 0.301f, 0.475f);

    GUID_128 skyboxTextureGUID{};
    std::shared_ptr<Texture> skyboxTexture;
    std::string skyboxTexturePath;
    bool useSkybox = true;

    // === Post-Processing: Blur (Gaussian) ===
    bool blurEnabled = false;
    float blurIntensity = 0.5f;
    float blurRadius = 2.0f;
    int blurPasses = 2;
    uint32_t blurLayerMask = 0xFFFFFFFF; // checked=blurred, all by default

    // === Post-Processing: Directional Blur ===
    bool dirBlurEnabled = false;
    float dirBlurIntensity = 0.5f;   // Blend: 0 (sharp) to 1 (fully blurred)
    float dirBlurStrength = 5.0f;    // Pixel distance of blur spread
    float dirBlurAngle = 0.0f;       // Direction in degrees (0=right, 90=up)
    int dirBlurSamples = 8;          // Quality: samples per side (4-16)

    // === Post-Processing: Bloom ===
    bool bloomEnabled = false;
    float bloomThreshold = 1.0f;
    float bloomIntensity = 1.0f;
    float bloomSpread = 0.5f;

    // === Post-Processing: Vignette ===
    bool vignetteEnabled = false;
    float vignetteIntensity = 0.5f;
    float vignetteSmoothness = 0.5f;
    glm::vec3 vignetteColor = glm::vec3(0.0f);  // Vignette tint color (black by default, like Unity)

    // === Post-Processing: Color Grading ===
    bool colorGradingEnabled = false;
    float cgBrightness = 0.0f;
    float cgContrast = 1.0f;
    float cgSaturation = 1.0f;
    glm::vec3 cgTint = glm::vec3(1.0f);

    // === Post-Processing: Chromatic Aberration ===
    bool chromaticAberrationEnabled = false;
    float chromaticAberrationIntensity = 0.5f;  // Strength of RGB separation (0-3)
    float chromaticAberrationPadding = 0.5f;    // Edge falloff: 0 = everywhere, 1 = only at very edges

    // === Post-Processing: SSAO ===
    bool ssaoEnabled = false;
    float ssaoRadius = 0.5f;
    float ssaoIntensity = 1.0f;

    // === Environment Reflections ===
    bool envReflectionEnabled = true;
    float envReflectionIntensity = 1.0f;

    // === Distance-Based Fading ===
    float fadeNear = 3.0f;    // Distance at which objects start fading in
    float fadeFar  = 5.0f;    // Distance at which objects are fully visible

    CameraComponent() = default;
    ~CameraComponent() = default;

    void SetTint(float r=1.0f, float g = 1.0f, float b = 1.0f);
};