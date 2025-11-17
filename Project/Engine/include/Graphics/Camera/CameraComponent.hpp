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

    CameraComponent() = default;
    ~CameraComponent() = default;
};