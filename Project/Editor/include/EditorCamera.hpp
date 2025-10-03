#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

/**
 * @brief Unity-style editor camera for scene editing.
 * 
 * Features:
 * - Alt+LMB: Rotate camera around target
 * - Scroll: Zoom in/out
 * - No WASD movement
 * - No mouse look (only when Alt is held)
 */
class EditorCamera {
public:
    // Camera Attributes
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;
    
    // Target point for orbiting
    glm::vec3 Target;
    
    // Orbit parameters
    float Distance;        // Distance from target
    float Yaw;            // Horizontal angle around target  
    float Pitch;          // Vertical angle around target
    
    // Camera options
    float Zoom;           // Field of view
    float MinDistance;    // Minimum zoom distance
    float MaxDistance;    // Maximum zoom distance
    
    // Input sensitivity
    float OrbitSensitivity;
    float ZoomSensitivity;
    float PanSensitivity;

    /**
     * @brief Constructor - creates editor camera looking at origin
     */
    EditorCamera(glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f), 
                 float distance = 5.0f);

    /**
     * @brief Returns the view matrix
     */
    glm::mat4 GetViewMatrix() const;
    
    /**
     * @brief Returns the projection matrix
     */
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

    /**
     * @brief Process Unity-style editor input
     * @param deltaTime Time since last frame
     * @param isWindowHovered Whether the scene panel is hovered
     * @param isAltPressed Whether Alt key is pressed
     * @param isLeftMousePressed Whether left mouse button is pressed
     * @param isMiddleMousePressed Whether middle mouse button is pressed
     * @param mouseDeltaX Mouse X movement
     * @param mouseDeltaY Mouse Y movement
     * @param scrollDelta Mouse scroll delta
     */
    void ProcessInput(float deltaTime, bool isWindowHovered,
                     bool isAltPressed, bool isLeftMousePressed, bool isMiddleMousePressed,
                     float mouseDeltaX, float mouseDeltaY, float scrollDelta);

    /**
     * @brief Set the target point to orbit around
     */
    void SetTarget(const glm::vec3& target);
    
    /**
     * @brief Frame the target (like Unity's Frame Selected)
     */
    void FrameTarget(const glm::vec3& target, float distance = 5.0f);

private:
    /**
     * @brief Update camera position based on orbit parameters
     */
    void UpdateCameraVectors();
};