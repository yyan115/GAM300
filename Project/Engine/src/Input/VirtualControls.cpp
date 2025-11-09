/* Start Header ************************************************************************/
/*!
\file       VirtualControls.cpp
\author     Yan Yu
\date       Oct 8, 2025
\brief      Implementation of virtual on-screen joystick for Android, rendering
            a circular joystick with outer and inner circles for touch-based movement

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "pch.h"

#ifdef ANDROID

#include "Input/VirtualControls.hpp"
#include "Input/InputManager.hpp"
#include "Logging.hpp"
#include <android/log.h>
#include "Asset Manager/ResourceManager.hpp"
#include <glm/glm.hpp>
#include <cmath>

// Static member definitions
bool VirtualControls::s_initialized = false;
bool VirtualControls::s_enabled = true;
VirtualJoystick VirtualControls::s_joystick = {};
GLuint VirtualControls::s_vao = 0;
GLuint VirtualControls::s_vbo = 0;
std::shared_ptr<Shader> VirtualControls::s_shader = nullptr;

void VirtualControls::Initialize() {
    if (s_initialized) {
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "VirtualControls already initialized");
        return;
    }

    __android_log_print(ANDROID_LOG_INFO, "GAM300", "Starting VirtualControls initialization...");

    InitializeJoystick();
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "Joystick initialized");

    InitializeRenderResources();
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "Render resources initialized");

    s_initialized = true;
    s_enabled = true;
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "VirtualControls initialization complete - initialized=%s, enabled=%s",
                       s_initialized ? "true" : "false", s_enabled ? "true" : "false");
}

void VirtualControls::Shutdown() {
    if (!s_initialized) return;

    // Cleanup OpenGL resources
    if (s_vao != 0) {
        glDeleteVertexArrays(1, &s_vao);
        s_vao = 0;
    }
    if (s_vbo != 0) {
        glDeleteBuffers(1, &s_vbo);
        s_vbo = 0;
    }

    // Shader is managed by ResourceManager, don't manually delete
    s_shader = nullptr;

    s_initialized = false;
}

void VirtualControls::InitializeJoystick() {
    // Position joystick in bottom-left corner
    const float baseX = 0.15f;       // 15% from left edge
    const float baseY = 0.85f;       // 85% from top (near bottom)
    const float outerRadius = 0.12f; // 12% of screen width for outer circle
    const float innerRadius = 0.05f; // 5% of screen width for inner circle

    s_joystick.baseX = baseX;
    s_joystick.baseY = baseY;
    s_joystick.stickX = baseX;  // Start centered
    s_joystick.stickY = baseY;  // Start centered
    s_joystick.outerRadius = outerRadius;
    s_joystick.innerRadius = innerRadius;
    s_joystick.isActive = false;
    s_joystick.alpha = 0.6f;
}

void VirtualControls::InitializeRenderResources() {
    // Load shader using the engine's resource manager
    std::string shaderPath = ResourceManager::GetPlatformShaderPath("virtualcontrols");
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "Attempting to load shader at path: %s", shaderPath.c_str());

    s_shader = ResourceManager::GetInstance().GetResource<Shader>(shaderPath);
    if (!s_shader) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Failed to load virtualcontrolsandroid shader at path: %s", shaderPath.c_str());
        return;
    }

    __android_log_print(ANDROID_LOG_INFO, "GAM300", "VirtualControls shader loaded successfully");

    // Create a simple quad for rendering
    float vertices[] = {
        // positions   // texture coords
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f
    };

    // Generate VAO and VBO
    glGenVertexArrays(1, &s_vao);
    glGenBuffers(1, &s_vbo);

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    __android_log_print(ANDROID_LOG_INFO, "GAM300", "VirtualControls VAO and VBO created successfully");
}

void VirtualControls::Render(int screenWidth, int screenHeight) {
    if (!s_initialized || !s_enabled) {
        return;
    }

    // Check if shader is valid
    if (!s_shader) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "VirtualControls shader is null!");
        return;
    }

    // Save current OpenGL state
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);

    GLint currentVAO = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);

    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);

    // Set our state
    glDisable(GL_DEPTH_TEST); // Disable depth test so draw order matters
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    s_shader->Activate();
    glBindVertexArray(s_vao);

    // Render joystick
    RenderJoystick(screenWidth, screenHeight);

    // Restore OpenGL state
    glBindVertexArray(currentVAO);
    glUseProgram(currentProgram);
    if (!blendEnabled) {
        glDisable(GL_BLEND);
    }
    if (depthTestEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
}

void VirtualControls::RenderJoystick(int screenWidth, int screenHeight) {
    // Render outer circle (base)
    float outerScreenX = s_joystick.baseX * screenWidth;
    float outerScreenY = s_joystick.baseY * screenHeight;
    float outerScreenR = s_joystick.outerRadius * screenWidth;

    // Convert to NDC
    float outerNdcX = (outerScreenX / screenWidth) * 2.0f - 1.0f;
    float outerNdcY = 1.0f - (outerScreenY / screenHeight) * 2.0f;
    float outerNdcR = (outerScreenR / screenWidth) * 2.0f;

    // Transform matrix for outer circle
    glm::mat4 outerTransform = glm::mat4(
        outerNdcR, 0,          0, 0,
        0,         outerNdcR,  0, 0,
        0,         0,          1, 0,
        outerNdcX, outerNdcY,  0, 1
    );

    s_shader->setMat4("uTransform", outerTransform);
    s_shader->setVec4("uColor", 0.3f, 0.3f, 0.3f, 1.0f);
    s_shader->setFloat("uAlpha", s_joystick.alpha * 0.5f);
    s_shader->setInt("uIsInnerCircle", 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Render inner circle (stick)
    float innerScreenX = s_joystick.stickX * screenWidth;
    float innerScreenY = s_joystick.stickY * screenHeight;
    float innerScreenR = s_joystick.innerRadius * screenWidth;

    // Convert to NDC
    float innerNdcX = (innerScreenX / screenWidth) * 2.0f - 1.0f;
    float innerNdcY = 1.0f - (innerScreenY / screenHeight) * 2.0f;
    float innerNdcR = (innerScreenR / screenWidth) * 2.0f;

    // Transform matrix for inner circle
    glm::mat4 innerTransform = glm::mat4(
        innerNdcR, 0,          0, 0,
        0,         innerNdcR,  0, 0,
        0,         0,          1, 0,
        innerNdcX, innerNdcY,  0, 1
    );

    s_shader->setMat4("uTransform", innerTransform);
    s_shader->setVec4("uColor", 0.5f, 0.5f, 0.5f, 1.0f);
    s_shader->setFloat("uAlpha", s_joystick.alpha);
    s_shader->setInt("uIsInnerCircle", 1);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

bool VirtualControls::HandleTouch(float x, float y, bool isPressed) {
    if (!s_initialized || !s_enabled) return false;

    UpdateJoystickState(x, y, isPressed);
    return s_joystick.isActive;
}

void VirtualControls::UpdateJoystickState(float touchX, float touchY, bool isPressed) {
    if (isPressed) {
        // First touch - check if we're starting inside the joystick
        if (!s_joystick.isActive) {
            if (IsPointInCircle(touchX, touchY, s_joystick.baseX, s_joystick.baseY, s_joystick.outerRadius)) {
                s_joystick.isActive = true;
            }
        }

        // If joystick is active, update position (even if finger moved outside)
        if (s_joystick.isActive) {
            // Calculate offset from center
            float dx = touchX - s_joystick.baseX;
            float dy = touchY - s_joystick.baseY;
            float distance = std::sqrt(dx * dx + dy * dy);

            // Clamp stick position to outer circle
            float maxDist = s_joystick.outerRadius - s_joystick.innerRadius;
            if (distance > maxDist) {
                float angle = std::atan2(dy, dx);
                dx = std::cos(angle) * maxDist;
                dy = std::sin(angle) * maxDist;
            }

            s_joystick.stickX = s_joystick.baseX + dx;
            s_joystick.stickY = s_joystick.baseY + dy;

            UpdateKeysFromJoystick();
        }
    } else {
        // Release - reset stick to center
        if (s_joystick.isActive) {
            s_joystick.isActive = false;
            s_joystick.stickX = s_joystick.baseX;
            s_joystick.stickY = s_joystick.baseY;

            // Release all movement keys
            InputManager::OnKeyEvent(Input::Key::W, Input::KeyAction::RELEASE);
            InputManager::OnKeyEvent(Input::Key::A, Input::KeyAction::RELEASE);
            InputManager::OnKeyEvent(Input::Key::S, Input::KeyAction::RELEASE);
            InputManager::OnKeyEvent(Input::Key::D, Input::KeyAction::RELEASE);
        }
    }
}

void VirtualControls::UpdateKeysFromJoystick() {
    // Calculate joystick direction
    float dx = s_joystick.stickX - s_joystick.baseX;
    float dy = s_joystick.stickY - s_joystick.baseY;

    // Dead zone threshold
    const float deadZone = 0.02f;

    // Update WASD keys based on joystick position
    static bool wPressed = false;
    static bool aPressed = false;
    static bool sPressed = false;
    static bool dPressed = false;

    // W key (up - negative dy because Y increases downward)
    bool shouldPressW = (dy < -deadZone);
    if (shouldPressW != wPressed) {
        InputManager::OnKeyEvent(Input::Key::W, shouldPressW ? Input::KeyAction::PRESS : Input::KeyAction::RELEASE);
        wPressed = shouldPressW;
    }

    // S key (down - positive dy)
    bool shouldPressS = (dy > deadZone);
    if (shouldPressS != sPressed) {
        InputManager::OnKeyEvent(Input::Key::S, shouldPressS ? Input::KeyAction::PRESS : Input::KeyAction::RELEASE);
        sPressed = shouldPressS;
    }

    // A key (left - negative dx)
    bool shouldPressA = (dx < -deadZone);
    if (shouldPressA != aPressed) {
        InputManager::OnKeyEvent(Input::Key::A, shouldPressA ? Input::KeyAction::PRESS : Input::KeyAction::RELEASE);
        aPressed = shouldPressA;
    }

    // D key (right - positive dx)
    bool shouldPressD = (dx > deadZone);
    if (shouldPressD != dPressed) {
        InputManager::OnKeyEvent(Input::Key::D, shouldPressD ? Input::KeyAction::PRESS : Input::KeyAction::RELEASE);
        dPressed = shouldPressD;
    }
}

bool VirtualControls::IsPointInCircle(float px, float py, float cx, float cy, float radius) {
    float dx = px - cx;
    float dy = py - cy;
    return (dx * dx + dy * dy) <= (radius * radius);
}

#endif // ANDROID
