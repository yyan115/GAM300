#pragma once

#include "InputManager.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <glm/glm.hpp>

/**
 * @brief Android implementation of InputManager
 *
 * Uses entity-based touch detection: config references entity names,
 * engine looks up their transforms to determine hit areas.
 *
 * Supports:
 * - Entity-based touch zones (buttons bound to scene sprites)
 * - Gesture detection (swipes, double-tap)
 * - Unhandled touch drag (for camera rotation)
 *
 * Game code flow:
 * 1. Check if action is pressed: Input.IsActionPressed("Attack")
 * 2. For joysticks, get touch position: Input.GetActionTouchPosition("Movement")
 * 3. For camera, check unhandled drag: Input.IsDragging(), Input.GetDragDelta()
 */
class AndroidInputManager : public InputManager {
public:
    AndroidInputManager();
    ~AndroidInputManager() override = default;

    // InputManager interface implementation
    bool IsActionPressed(const std::string& action) override;
    bool IsActionJustPressed(const std::string& action) override;
    bool IsActionJustReleased(const std::string& action) override;
    glm::vec2 GetActionTouchPosition(const std::string& action) override;
    glm::vec2 GetAxis(const std::string& axisName) override;

    bool IsDragging() override;
    glm::vec2 GetDragDelta() override;

    bool IsPointerPressed() override;
    bool IsPointerJustPressed() override;
    glm::vec2 GetPointerPosition() override;

    int GetTouchCount() override;
    glm::vec2 GetTouchPosition(int index) override;

    std::vector<Touch> GetTouches() override;
    Touch GetTouchById(int touchId) override;

    void Update(float deltaTime) override;
    bool LoadConfig(const std::string& path) override;

    std::unordered_map<std::string, bool> GetAllActionStates() override;
    std::unordered_map<std::string, glm::vec2> GetAllAxisStates() override;

    void RenderOverlay(int screenWidth, int screenHeight) override;
    void SetGamePanelMousePos(float newX, float newY) override;

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
     * @brief Action bound to an entity (button/joystick)
     */
    struct EntityAction {
        std::string actionName;      // Action name (e.g., "Attack", "Movement")
        std::string entityName;      // Entity name to look up (e.g., "(ANDROID)AttackButton")

        // Cached entity data (updated each frame)
        bool entityFound = false;
        glm::vec2 entityCenter;      // Screen position (normalized 0-1)
        glm::vec2 entitySize;        // Size (normalized)

        // Touch state
        bool isPressed = false;
        int activeTouchId = -1;
        glm::vec2 touchPositionRelative;  // Touch position relative to entity center
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
        float maxTimeBetweenTaps = 0.3f;
        float maxTapDistance = 50.0f;        // Max movement during tap
    };

    /**
     * @brief Active touch point with full tracking
     */
    struct TouchPoint {
        int id;
        glm::vec2 position;
        glm::vec2 startPosition;
        glm::vec2 previousPosition;
        glm::vec2 delta;             // Movement this frame
        float startTime;
        float duration;              // Time since touch started
        TouchPhase phase;            // Current phase (began, moved, stationary, ended)
        std::string entityName;      // Which entity this touch is on ("" if none)
        bool isHandled;              // If handled by an entity action
        bool beganConsumed = false;  // True after Began phase has been seen for one full frame
    };

    // Touches that ended this frame (kept for one frame with Ended phase)
    std::vector<TouchPoint> m_endedTouches;

    // ========== State ==========

    // Entity-based actions (loaded from config)
    std::vector<EntityAction> m_entityActions;

    // Gesture bindings (loaded from config)
    std::vector<GestureBinding> m_gestures;

    // Active touches
    std::unordered_map<int, TouchPoint> m_activeTouches;

    // Unhandled touch for camera drag
    int m_dragTouchId = -1;
    glm::vec2 m_dragDelta;
    bool m_isDragging = false;

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
     * @brief Look up all entity transforms and cache their positions/sizes
     */
    void UpdateEntityTransforms();

    /**
     * @brief Check if a touch point is inside an entity's bounds
     */
    bool IsTouchInsideEntity(const EntityAction& entity, glm::vec2 touchPos);

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
