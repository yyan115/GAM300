#include "EditorCamera.hpp"
#include <algorithm>

EditorCamera::EditorCamera(glm::vec3 target, float distance)
    : Target(target), Distance(distance), Yaw(0.0f), Pitch(20.0f),
      Zoom(45.0f), MinDistance(1.0f), MaxDistance(50.0f),
      OrbitSensitivity(0.5f), ZoomSensitivity(2.0f), PanSensitivity(0.01f)
{
    WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    
    // Initialize camera position based on orbit parameters
    UpdateCameraVectors();
}

glm::mat4 EditorCamera::GetViewMatrix() const {
    return glm::lookAt(Position, Target, Up);
}

glm::mat4 EditorCamera::GetProjectionMatrix(float aspectRatio) const {
    // Clamp aspect ratio to reasonable bounds to prevent assertion errors
    float safeAspectRatio = aspectRatio;
    if (safeAspectRatio < 0.001f) safeAspectRatio = 0.001f;
    if (safeAspectRatio > 1000.0f) safeAspectRatio = 1000.0f;

    return glm::perspective(glm::radians(Zoom), safeAspectRatio, 0.1f, 100.0f);
}

void EditorCamera::ProcessInput(float deltaTime, bool isWindowHovered,
                               bool isAltPressed, bool isLeftMousePressed, bool isMiddleMousePressed,
                               float mouseDeltaX, float mouseDeltaY, float scrollDelta) {

    if (!isWindowHovered) return;

    if (isAltPressed && isLeftMousePressed) {
        Yaw -= mouseDeltaX * OrbitSensitivity;
        Pitch -= mouseDeltaY * OrbitSensitivity;

        // Constrain pitch to prevent flipping
        Pitch = std::clamp(Pitch, -89.0f, 89.0f);

        UpdateCameraVectors();
    }

    // Unity-style panning: Middle mouse button to pan target
    if (isMiddleMousePressed) {
        // Calculate right and up vectors relative to the camera
        glm::vec3 right = Right;
        glm::vec3 up = Up;

        // Pan the target point based on mouse movement
        // Scale pan speed based on distance to maintain consistent feel
        float panScale = Distance * PanSensitivity;
        Target -= right * mouseDeltaX * panScale;  // X-axis inverted: drag right moves world left
        Target -= up * mouseDeltaY * panScale;     // Y-axis: drag up moves world up (unchanged)

        UpdateCameraVectors();
    }

    // Zoom with scroll wheel
    if (scrollDelta != 0.0f) {
        Distance -= scrollDelta * ZoomSensitivity;
        Distance = std::clamp(Distance, MinDistance, MaxDistance);

        UpdateCameraVectors();
    }
}

void EditorCamera::SetTarget(const glm::vec3& target) {
    Target = target;
    UpdateCameraVectors();
}

void EditorCamera::FrameTarget(const glm::vec3& target, float distance) {
    Target = target;
    Distance = distance;
    
    // Reset to a nice viewing angle
    Yaw = 0.0f;
    Pitch = 20.0f;
    
    UpdateCameraVectors();
}

void EditorCamera::UpdateCameraVectors() {
    // Calculate position based on spherical coordinates around target
    glm::vec3 offset;
    offset.x = Distance * cos(glm::radians(Pitch)) * sin(glm::radians(Yaw));
    offset.y = Distance * sin(glm::radians(Pitch));
    offset.z = Distance * cos(glm::radians(Pitch)) * cos(glm::radians(Yaw));
    
    Position = Target + offset;
    
    // Calculate camera vectors
    Front = glm::normalize(Target - Position);
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));
}