#pragma once

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

/**
 * @brief Platform-agnostic input system interface
 *
 * Provides action-based input abstraction that works across desktop and Android.
 * Game code queries logical "actions" (e.g., "Jump", "Attack") instead of raw hardware inputs.
 *
 * Desktop: Maps keyboard/mouse/gamepad to actions
 * Android: Maps touch zones, gestures, and virtual controls to actions
 *
 * @example
 * // Game code (platform-agnostic)
 * if (g_inputSystem->IsActionPressed("Jump")) {
 *     player.Jump();
 * }
 *
 * glm::vec2 movement = g_inputSystem->GetAxis("Movement");
 * player.Move(movement.x, movement.y);
 */
class IInputSystem {
public:
    virtual ~IInputSystem() = default;

    // ========== Action-Based Input (Game Logic) ==========

    /**
     * @brief Check if an action is currently active
     * @param action Action name from config (e.g., "Jump", "Attack")
     * @return true if action is pressed/active this frame
     */
    virtual bool IsActionPressed(const std::string& action) = 0;

    /**
     * @brief Check if an action was just activated this frame (rising edge)
     * @param action Action name from config
     * @return true if action transitioned from inactive to active
     */
    virtual bool IsActionJustPressed(const std::string& action) = 0;

    /**
     * @brief Check if an action was just released this frame (falling edge)
     * @param action Action name from config
     * @return true if action transitioned from active to inactive
     */
    virtual bool IsActionJustReleased(const std::string& action) = 0;

    /**
     * @brief Get 2D axis value for movement/look controls
     * @param axisName Axis name from config (e.g., "Movement", "Look")
     * @return Normalized vector (-1 to 1 on each axis)
     *
     * Desktop: WASD/Arrow keys or mouse delta
     * Android: Virtual joystick or touch drag
     */
    virtual glm::vec2 GetAxis(const std::string& axisName) = 0;

    // ========== Pointer Abstraction (Scene UI Buttons) ==========

    /**
     * @brief Check if primary pointer is pressed
     * @return true if mouse left button (desktop) or primary touch (Android) is active
     *
     * Used by ButtonSystem to make scene UI work on both platforms
     */
    virtual bool IsPointerPressed() = 0;

    /**
     * @brief Check if primary pointer was just pressed this frame
     * @return true if pointer just went down
     */
    virtual bool IsPointerJustPressed() = 0;

    /**
     * @brief Get primary pointer position in normalized screen coordinates
     * @return Screen position (0-1 range, origin top-left)
     */
    virtual glm::vec2 GetPointerPosition() = 0;

    // ========== Multi-Touch Support (Android) ==========

    /**
     * @brief Get number of active touch points
     * @return Touch count (Desktop: 0-1 for mouse, Android: 0-10)
     */
    virtual int GetTouchCount() = 0;

    /**
     * @brief Get position of specific touch point
     * @param index Touch index (0 to GetTouchCount()-1)
     * @return Normalized screen position (0-1)
     */
    virtual glm::vec2 GetTouchPosition(int index) = 0;

    // ========== System Lifecycle ==========

    /**
     * @brief Update input state (call once per frame before game logic)
     * @param deltaTime Time since last frame
     */
    virtual void Update(float deltaTime) = 0;

    /**
     * @brief Load input configuration from JSON file
     * @param path Path to config file (e.g., "Resources/Configs/input_config.json")
     * @return true if loaded successfully
     */
    virtual bool LoadConfig(const std::string& path) = 0;

    // ========== Lua Optimization API ==========

    /**
     * @brief Get all action states in one call (reduces Lua/C++ boundary crossings)
     * @return Map of action names to pressed state
     *
     * Lua scripts can cache this table for the frame instead of calling
     * IsActionPressed multiple times
     */
    virtual std::unordered_map<std::string, bool> GetAllActionStates() = 0;

    /**
     * @brief Get all axis states in one call
     * @return Map of axis names to 2D values
     */
    virtual std::unordered_map<std::string, glm::vec2> GetAllAxisStates() = 0;

    // ========== Rendering (Android Virtual Controls) ==========

    /**
     * @brief Render virtual controls overlay (Android only)
     * @param screenWidth Screen width in pixels
     * @param screenHeight Screen height in pixels
     *
     * Desktop: Does nothing
     * Android: Renders joysticks, virtual buttons, etc.
     */
    virtual void RenderOverlay(int screenWidth, int screenHeight) = 0;
};

/**
 * @brief Global input system instance
 *
 * Set by Application during platform initialization.
 * Platform-specific implementation is created based on build target:
 * - Desktop: DesktopInputSystem
 * - Android: AndroidInputSystem
 */
extern IInputSystem* g_inputSystem;
