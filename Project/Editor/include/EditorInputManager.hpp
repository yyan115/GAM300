#pragma once

#include "imgui.h"
#include <glm/glm.hpp>

/**
 * @brief Editor-specific input manager that works alongside ImGui.
 *
 * This manager provides Unity-style editor input handling without
 * interfering with the engine's InputManager or ImGui's input system.
 *
 * Features:
 * - Camera controls (orbit, pan, zoom)
 * - Editor shortcuts (gizmo switching, focus, etc.)
 * - Context-aware input (respects ImGui window focus)
 */
class EditorInputManager {
public:
    struct MouseState {
        glm::vec2 position;
        glm::vec2 delta;
        glm::vec2 lastPosition;
        bool firstMouse = true;
        float scrollDelta = 0.0f;

        bool leftPressed = false;
        bool middlePressed = false;
        bool rightPressed = false;

        bool leftDown = false;
        bool middleDown = false;
        bool rightDown = false;
    };

    struct KeyboardState {
        bool altPressed = false;
        bool ctrlPressed = false;
        bool shiftPressed = false;

        // Editor shortcuts
        bool qKeyPressed = false;  // Normal/panning mode
        bool wKeyPressed = false;  // Translate gizmo
        bool eKeyPressed = false;  // Rotate gizmo
        bool rKeyPressed = false;  // Scale gizmo
    };

    /**
     * @brief Update input state from ImGui. Call this once per frame.
     */
    static void Update();

    /**
     * @brief Get current mouse state
     */
    static const MouseState& GetMouseState();

    /**
     * @brief Get current keyboard state
     */
    static const KeyboardState& GetKeyboardState();

    /**
     * @brief Check if a window is hovered and can receive input
     */
    static bool IsWindowHovered(const char* windowName);

    /**
     * @brief Get mouse delta with optional sensitivity scaling
     */
    static glm::vec2 GetMouseDelta(float sensitivity = 1.0f);

    /**
     * @brief Check for camera input (respects window hover)
     */
    static bool ShouldHandleCameraInput(const char* windowName);

    /**
     * @brief Check for gizmo shortcut inputs
     */
    static bool IsGizmoShortcutPressed(int gizmoType); // 0=normal, 1=translate, 2=rotate, 3=scale

private:
    static MouseState mouseState;
    static KeyboardState keyboardState;

    static void UpdateMouseState();
    static void UpdateKeyboardState();
};