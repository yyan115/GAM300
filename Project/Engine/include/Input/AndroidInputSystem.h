#pragma once

#include "IInputSystem.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <glm/glm.hpp>

/**
 * @brief Android implementation of IInputSystem
 *
 * Maps touch inputs, gestures, and virtual controls to logical actions.
 * Supports:
 * - Touch zones (virtual buttons on screen)
 * - Virtual joysticks (on-screen analog stick)
 * - Gesture detection (swipes, double-tap, pinch)
 * - Touch drag for camera look
 *
 * Loads configuration from JSON defining positions, sizes, and visual properties
 * of all virtual controls.
 */
class AndroidInputSystem : public IInputSystem {
public:
    AndroidInputSystem();
    ~AndroidInputSystem() override = default;

    // IInputSystem interface implementation
    bool IsActionPressed(const std::string& action) override;
    bool IsActionJustPressed(const std::string& action) override;
    bool IsActionJustReleased(const std::string& action) override;
    glm::vec2 GetAxis(const std::string& axisName) override;

    bool IsPointerPressed() override;
    bool IsPointerJustPressed() override;
    glm::vec2 GetPointerPosition() override;

    int GetTouchCount() override;
    glm::vec2 GetTouchPosition(int index) override;

    void Update(float deltaTime) override;
    bool LoadConfig(const std::string& path) override;

    std::unordered_map<std::string, bool> GetAllActionStates() override;
    std::unordered_map<std::string, glm::vec2> GetAllAxisStates() override;

    void RenderOverlay(int screenWidth, int screenHeight) override;

    // ========== Android-Specific Methods ==========

    /**
     * @brief Called by AndroidPlatform when touch starts
     * @param pointerId Unique touch ID
     * @param x Normalized X coordinate (0-1)
     * @param y Normalized Y coordinate (0-1)
     */
    void OnTouchDown(int pointerId, float x, float y);

    /**
     * @brief Called by AndroidPlatform when touch moves
     */
    void OnTouchMove(int pointerId, float x, float y);

    /**
     * @brief Called by AndroidPlatform when touch ends
     */
    void OnTouchUp(int pointerId, float x, float y);

private:
    // ========== Internal Types ==========

    /**
     * @brief Virtual button (touch zone on screen)
     */
    struct TouchZone {
        std::string action;           // Action name this zone triggers
        glm::vec2 position;          // Normalized position (0-1)
        float radius;                // Circle radius (normalized)
        bool isCircle;               // Circle or rectangle
        glm::vec2 rectSize;          // Rectangle size if not circle

        // Visual properties
        std::string normalImage;
        std::string pressedImage;
        float alpha;

        // State
        bool isPressed = false;
        int activeTouchId = -1;      // Which touch is pressing this zone
    };

    /**
     * @brief Virtual joystick (on-screen analog stick)
     */
    struct VirtualJoystick {
        std::string axisName;        // Axis name (e.g., "Movement")
        glm::vec2 basePosition;      // Center position (normalized)
        float outerRadius;           // Outer circle radius
        float innerRadius;           // Inner stick radius
        float deadZone;              // Dead zone threshold

        // Visual properties
        std::string outerImage;
        std::string innerImage;
        float alpha;

        // State
        bool isActive = false;
        int activeTouchId = -1;
        glm::vec2 stickOffset;       // Current stick displacement from center
        glm::vec2 normalizedValue;   // Output value (-1 to 1)
    };

    /**
     * @brief Touch drag zone for camera look
     */
    struct TouchDragZone {
        std::string axisName;        // Axis name (e.g., "Look")
        glm::vec2 zonePosition;      // Zone top-left (normalized)
        glm::vec2 zoneSize;          // Zone dimensions (normalized)
        float sensitivity;

        // State
        bool isActive = false;
        int activeTouchId = -1;
        glm::vec2 previousPosition;
        glm::vec2 delta;             // Current frame delta
    };

    /**
     * @brief Gesture type
     */
    enum class GestureType {
        SWIPE,
        DOUBLE_TAP,
        PINCH
    };

    /**
     * @brief Gesture detector binding
     */
    struct GestureBinding {
        std::string action;          // Action triggered by gesture
        GestureType type;

        // For swipes
        glm::vec2 direction;         // Normalized direction (for directional swipes)
        float minDistance;           // Minimum swipe distance
        float maxTime;               // Maximum swipe duration

        // For double-tap
        float maxTimeBetweenTaps;
        float maxTapDistance;        // Max movement during tap

        // Zone (optional - can limit gesture to screen region)
        bool hasZone;
        glm::vec2 zonePosition;
        glm::vec2 zoneSize;
    };

    /**
     * @brief Active touch point
     */
    struct TouchPoint {
        int id;
        glm::vec2 position;
        glm::vec2 startPosition;
        float startTime;
        bool consumed;               // If handled by touch zone/joystick
    };

    // ========== State ==========

    // Virtual controls (loaded from config)
    std::vector<TouchZone> m_touchZones;
    std::vector<VirtualJoystick> m_joysticks;
    std::vector<TouchDragZone> m_dragZones;
    std::vector<GestureBinding> m_gestures;

    // Active touches
    std::unordered_map<int, TouchPoint> m_activeTouches;

    // Action state tracking
    std::unordered_set<std::string> m_currentActions;
    std::unordered_set<std::string> m_previousActions;

    // Gesture tracking
    float m_lastTapTime = 0.0f;
    glm::vec2 m_lastTapPosition;
    int m_tapCount = 0;

    // Current frame time (for gesture timing)
    float m_currentTime = 0.0f;

    // ========== Helper Methods ==========

    /**
     * @brief Check if a point is inside a touch zone
     */
    bool IsTouchInsideZone(const TouchZone& zone, glm::vec2 touchPos);

    /**
     * @brief Check if a point is inside a rectangle
     */
    bool IsPointInRect(glm::vec2 point, glm::vec2 rectPos, glm::vec2 rectSize);

    /**
     * @brief Update touch zone states
     */
    void UpdateTouchZones();

    /**
     * @brief Update virtual joystick states
     */
    void UpdateJoysticks();

    /**
     * @brief Update touch drag zones (for camera look)
     */
    void UpdateDragZones();

    /**
     * @brief Detect and trigger gestures
     */
    void DetectGestures(float deltaTime);

    /**
     * @brief Check for swipe gestures
     */
    void DetectSwipes();

    /**
     * @brief Check for double-tap gestures
     */
    void DetectDoubleTap();

    /**
     * @brief Get current time in seconds (for gesture timing)
     */
    float GetCurrentTime();
};
