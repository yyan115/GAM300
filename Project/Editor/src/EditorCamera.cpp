#include "EditorCamera.hpp"
#include <algorithm>

EditorCamera::EditorCamera(glm::vec3 target, float distance)
    : Target(target), Distance(distance), Yaw(0.0f), Pitch(20.0f),
      Zoom(45.0f), MinDistance(1.0f), MaxDistance(50.0f),
      OrbitSensitivity(0.5f), ZoomSensitivity(2.0f), PanSensitivity(0.03f),
      OrthoZoomLevel(1.0f)
{
    WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);

    // Initialize camera position based on orbit parameters
    UpdateCameraVectors();
}

glm::mat4 EditorCamera::GetViewMatrix() const {
    return glm::lookAt(Position, Target, Up);
}

glm::mat4 EditorCamera::Get2DViewMatrix() const {
    // For 2D mode, use a simple identity view matrix
    // The camera is looking straight down at the XY plane
    // Position doesn't matter since we're using orthographic projection centered on Target
    return glm::mat4(1.0f);
}

glm::mat4 EditorCamera::GetProjectionMatrix(float aspectRatio) const {
    // Clamp aspect ratio to reasonable bounds to prevent assertion errors
    float safeAspectRatio = aspectRatio;
    if (safeAspectRatio < 0.001f) safeAspectRatio = 0.001f;
    if (safeAspectRatio > 1000.0f) safeAspectRatio = 1000.0f;

    return glm::perspective(glm::radians(Zoom), safeAspectRatio, 0.1f, 100.0f);
}

glm::mat4 EditorCamera::GetOrthographicProjectionMatrix(float aspectRatio, float viewportWidth, float viewportHeight) const {
    // For 2D mode, create an orthographic projection with zoom support
    // OrthoZoomLevel: 1.0 = normal (1:1 pixel mapping), 0.5 = zoomed in 2x, 2.0 = zoomed out 2x

    // Apply zoom to viewport size
    float viewWidth = viewportWidth * OrthoZoomLevel;
    float viewHeight = viewportHeight * OrthoZoomLevel;

    // Center the view around the target point in pixel space
    // Target.x and Target.y represent the center pixel coordinate you're looking at
    float left = Target.x - viewWidth * 0.5f;
    float right = Target.x + viewWidth * 0.5f;
    float bottom = Target.y - viewHeight * 0.5f;
    float top = Target.y + viewHeight * 0.5f;

    return glm::ortho(left, right, bottom, top, -1000.0f, 1000.0f);
}

void EditorCamera::ProcessInput(float deltaTime, bool isWindowHovered,
                               bool isAltPressed, bool isLeftMousePressed, bool isMiddleMousePressed,
                               float mouseDeltaX, float mouseDeltaY, float scrollDelta, bool is2DMode) {

    if (!isWindowHovered) return;

    // Only allow camera rotation in 3D mode
    if (isAltPressed && isLeftMousePressed && !is2DMode) {
        Yaw -= mouseDeltaX * OrbitSensitivity;
        Pitch -= mouseDeltaY * OrbitSensitivity;

        // Constrain pitch to prevent flipping
        Pitch = std::clamp(Pitch, -89.0f, 89.0f);

        UpdateCameraVectors();
    }

    
    if (isMiddleMousePressed) {
        // Calculate right and up vectors relative to the camera
        glm::vec3 right = Right;
        glm::vec3 up = Up;

        // Pan the target point based on mouse movement
        // In 2D mode, scale by OrthoZoomLevel; in 3D mode, scale by Distance
        float panScale;
        if (is2DMode) {
            // In 2D mode, use zoom-scaled sensitivity
            // Reference zoom of 2.5 where base sensitivity feels good
            float referenceZoom = 2.5f;
            panScale = PanSensitivity * (referenceZoom / OrthoZoomLevel);
        } else {
            // In 3D mode, scale by distance
            panScale = Distance * PanSensitivity;
        }

        Target -= right * mouseDeltaX * panScale;  // X-axis inverted: drag right moves world left
        Target -= up * mouseDeltaY * panScale;     // Y-axis: drag up moves world up (unchanged)

        // Also update Position directly for proper 2D camera panning
        // This ensures Position.xy matches Target.xy for orthographic projection
        Position -= right * mouseDeltaX * panScale;
        Position -= up * mouseDeltaY * panScale;

        UpdateCameraVectors();
    }

    // Zoom with scroll wheel
    if (scrollDelta != 0.0f) {
        // In 2D mode, zoom affects orthographic scale instead of distance
        // We could check EditorState here, but for simplicity, always update both
        Distance -= scrollDelta * ZoomSensitivity;
        Distance = std::clamp(Distance, MinDistance, MaxDistance);

        // For 2D orthographic zoom: scroll up = zoom in (smaller zoom level)
        OrthoZoomLevel -= scrollDelta * 0.1f;
        OrthoZoomLevel = std::clamp(OrthoZoomLevel, 0.1f, 5.0f);

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