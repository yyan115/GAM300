#pragma once

#include "InputManager.h"
#include "Keys.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <glm/glm.hpp>

class IPlatform;  // Forward declaration

/**
 * @brief Desktop implementation of InputManager
 *
 * Maps keyboard, mouse, and gamepad inputs to logical actions.
 * Loads key bindings from JSON configuration file.
 *
 * Example config:
 * {
 *   "desktop": {
 *     "actions": {
 *       "Jump": { "keys": ["SPACE"], "mouseButtons": [] }
 *     },
 *     "axes": {
 *       "Movement": {
 *         "positiveX": ["D"], "negativeX": ["A"],
 *         "positiveY": ["W"], "negativeY": ["S"]
 *       }
 *     }
 *   }
 * }
 */
class DesktopInputManager : public InputManager {
public:
    /**
     * @brief Constructor
     * @param platform Pointer to platform layer for hardware input queries
     */
    explicit DesktopInputManager(IPlatform* platform);
    ~DesktopInputManager() override = default;

    // InputManager interface implementation
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

    // Editor support
    void SetGamePanelMousePos(float newX, float newY) override;

private:
    // ========== Internal Types ==========

    /**
     * @brief Binding for a discrete action (button press)
     */
    struct ActionBinding {
        std::vector<Input::Key> keys;                    // Keyboard keys
        std::vector<Input::MouseButton> mouseButtons;    // Mouse buttons
        // TODO: Add gamepad button support
    };

    /**
     * @brief Type of axis input
     */
    enum class AxisType {
        KeyboardComposite,  // WASD/Arrow keys combined into 2D vector
        MouseDelta,         // Mouse movement (for camera look)
        Gamepad             // Gamepad stick (future support)
    };

    /**
     * @brief Binding for a 2D axis (movement, look)
     */
    struct AxisBinding {
        AxisType type = AxisType::KeyboardComposite;

        // For KeyboardComposite
        std::vector<Input::Key> positiveX;  // Right
        std::vector<Input::Key> negativeX;  // Left
        std::vector<Input::Key> positiveY;  // Up
        std::vector<Input::Key> negativeY;  // Down

        // For MouseDelta
        float sensitivity = 1.0f;
    };

    // ========== State ==========

    IPlatform* m_platform;  // Platform layer for hardware queries

    // Configuration data (loaded from JSON)
    std::unordered_map<std::string, ActionBinding> m_actionBindings;
    std::unordered_map<std::string, AxisBinding> m_axisBindings;

    // State tracking for edge detection (JustPressed/JustReleased)
    std::unordered_set<std::string> m_currentActions;   // Actions active this frame
    std::unordered_set<std::string> m_previousActions;  // Actions active last frame

    // Mouse state for delta calculation
    glm::vec2 m_previousMousePos = glm::vec2(0.0f);
    glm::vec2 m_mouseDelta = glm::vec2(0.0f);
    bool m_firstMouseUpdate = true;

    // Pointer state tracking
    bool m_pointerPressed = false;
    bool m_pointerPreviouslyPressed = false;

    // Editor support
    double m_gamePanelMouseX = 0.0;
    double m_gamePanelMouseY = 0.0;

    // ========== Helper Methods ==========

    /**
     * @brief Parse key name string to enum
     * @param keyName String like "SPACE", "W", "A"
     * @return Input::Key enum value
     */
    Input::Key ParseKey(const std::string& keyName);

    /**
     * @brief Parse mouse button name string to enum
     * @param buttonName String like "LEFT", "RIGHT", "MIDDLE"
     * @return Input::MouseButton enum value
     */
    Input::MouseButton ParseMouseButton(const std::string& buttonName);

    /**
     * @brief Check if a key is currently pressed (uses IPlatform)
     */
    bool IsKeyPressed(Input::Key key);

    /**
     * @brief Check if a mouse button is currently pressed (uses IPlatform)
     */
    bool IsMouseButtonPressed(Input::MouseButton button);

    /**
     * @brief Get current mouse position in normalized coordinates (uses IPlatform)
     * @return Position in 0-1 range
     */
    glm::vec2 GetMousePositionNormalized();

    /**
     * @brief Evaluate a keyboard composite axis (WASD keys -> 2D vector)
     * @param binding Axis binding with key mappings
     * @return Normalized 2D vector
     */
    glm::vec2 EvaluateKeyboardAxis(const AxisBinding& binding);

    /**
     * @brief Update action states (called each frame)
     */
    void UpdateActionStates();

    /**
     * @brief Update axis states (called each frame)
     */
    void UpdateAxisStates(float deltaTime);
};
