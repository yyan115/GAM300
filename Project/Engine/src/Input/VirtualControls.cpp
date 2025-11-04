/* Start Header ************************************************************************/
/*!
\file       VirtualControls.cpp
\author     Yan Yu
\date       Oct 8, 2025
\brief      Implementation of virtual on-screen controls for Android, rendering
            transparent WASD buttons and handling touch-to-key mapping

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

// Static member definitions
bool VirtualControls::s_initialized = false;
bool VirtualControls::s_enabled = true;
VirtualButton VirtualControls::s_buttons[4];
GLuint VirtualControls::s_vao = 0;
GLuint VirtualControls::s_vbo = 0;
std::shared_ptr<Shader> VirtualControls::s_shader = nullptr;

void VirtualControls::Initialize() {
    if (s_initialized) {
        __android_log_print(ANDROID_LOG_INFO, "GAM300", "VirtualControls already initialized");
        return;
    }
    
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "Starting VirtualControls initialization...");
    
    InitializeButtons();
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "Buttons initialized");
    
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

void VirtualControls::InitializeButtons() {
    // Position buttons in bottom-left corner for WASD layout
    const float buttonSize = 0.15f; // 15% of screen (bigger for debugging)
    const float spacing = 0.03f;    // 3% spacing
    const float baseX = 0.2f;       // 20% from left edge
    const float baseY = 0.7f;       // 70% from top (higher up for visibility)
    
    // W button (top)
    s_buttons[0] = {
        baseX + buttonSize, baseY - buttonSize - spacing,  // x, y
        buttonSize, buttonSize,                            // width, height
        Input::Key::W,                                     // key
        false,                                            // isPressed
        0.7f                                              // alpha
    };
    
    // A button (left)  
    s_buttons[1] = {
        baseX, baseY,                                     // x, y
        buttonSize, buttonSize,                           // width, height
        Input::Key::A,                                    // key
        false,                                            // isPressed
        0.7f                                              // alpha
    };
    
    // S button (bottom)
    s_buttons[2] = {
        baseX + buttonSize, baseY,                        // x, y
        buttonSize, buttonSize,                           // width, height
        Input::Key::S,                                    // key
        false,                                            // isPressed
        0.7f                                              // alpha
    };
    
    // D button (right)
    s_buttons[3] = {
        baseX + buttonSize * 2 + spacing, baseY,         // x, y
        buttonSize, buttonSize,                           // width, height
        Input::Key::D,                                    // key
        false,                                            // isPressed
        0.7f                                              // alpha
    };
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
    
    // Create a simple quad for button rendering  
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
    __android_log_print(ANDROID_LOG_DEBUG, "GAM300", "VirtualControls::Render called - initialized=%s, enabled=%s, screenSize=%dx%d",
                       s_initialized ? "true" : "false", s_enabled ? "true" : "false", screenWidth, screenHeight);
    
    if (!s_initialized || !s_enabled) {
        __android_log_print(ANDROID_LOG_WARN, "GAM300", "VirtualControls::Render early return - not initialized or disabled");
        return;
    }
    
    // Check if shader is valid
    if (!s_shader) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "VirtualControls shader is null!");
        return;
    }
    
    // Clear any pending OpenGL errors first
    while (glGetError() != GL_NO_ERROR) {
        __android_log_print(ANDROID_LOG_DEBUG, "GAM300", "Clearing pending OpenGL error");
    }
    
    // Save current OpenGL state step by step
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error getting current program: %d", error);
        return;
    }
    
    GLint currentVAO = 0;  
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error getting current VAO: %d", error);
        return;
    }
    
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error checking blend enabled: %d", error);
        return;
    }
    
    // Set our state step by step
    glEnable(GL_BLEND);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error enabling blend: %d", error);
        return;
    }
    
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error setting blend func: %d", error);
        return;
    }
    
    s_shader->Activate();
    error = glGetError();
    if (error != GL_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error activating shader: error=%d", error);
        return;
    }
    
    glBindVertexArray(s_vao);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, "GAM300", "Error binding VAO %d: error=%d", s_vao, error);
        // Try to restore state
        glUseProgram(currentProgram);
        if (!blendEnabled) glDisable(GL_BLEND);
        return;
    }
    
    // Render buttons
    for (int i = 0; i < 4; i++) {
        RenderButton(s_buttons[i], screenWidth, screenHeight);
    }
    
    // Restore OpenGL state
    glBindVertexArray(currentVAO);
    glUseProgram(currentProgram);
    if (!blendEnabled) {
        glDisable(GL_BLEND);
    }
}

void VirtualControls::RenderButton(const VirtualButton& button, int screenWidth, int screenHeight) {
    // Convert normalized coordinates to screen space
    float screenX = button.x * screenWidth;
    float screenY = button.y * screenHeight;
    float screenW = button.width * screenWidth;
    float screenH = button.height * screenHeight;
    
    __android_log_print(ANDROID_LOG_INFO, "GAM300", "Rendering button key=%d at screen: %.1f,%.1f size: %.1fx%.1f", 
                       static_cast<int>(button.key), screenX, screenY, screenW, screenH);
    
    // Create transformation matrix to position and scale the button
    // Convert screen coordinates to NDC (-1 to 1)
    float ndcX = (screenX / screenWidth) * 2.0f - 1.0f;
    float ndcY = 1.0f - (screenY / screenHeight) * 2.0f; // Flip Y
    float ndcW = (screenW / screenWidth) * 2.0f;
    float ndcH = (screenH / screenHeight) * 2.0f;
    
    // Simple transform matrix (scale and translate)
    glm::mat4 transform = glm::mat4(
        ndcW/2, 0,       0,    0,
        0,      ndcH/2,  0,    0,
        0,      0,       1,    0,
        ndcX,   ndcY,    0,    1
    );
    
    // Set uniforms using engine's shader methods
    s_shader->setMat4("uTransform", transform);
    
    // Grey circles - lighter when pressed
    float color[4];
    if (button.isPressed) {
        // Lighter grey when pressed
        color[0] = 0.7f;
        color[1] = 0.7f;
        color[2] = 0.7f;
        color[3] = 1.0f;
    } else {
        // Dark grey when not pressed
        color[0] = 0.4f;
        color[1] = 0.4f;
        color[2] = 0.4f;
        color[3] = 1.0f;
    }
    
    s_shader->setVec4("uColor", color[0], color[1], color[2], color[3]);
    s_shader->setFloat("uAlpha", button.alpha);

    // Set direction for triangle: W=0(up), A=1(left), S=2(down), D=3(right)
    int direction = 0;
    switch (button.key) {
        case Input::Key::W: direction = 0; break; // Up
        case Input::Key::A: direction = 1; break; // Left
        case Input::Key::S: direction = 2; break; // Down
        case Input::Key::D: direction = 3; break; // Right
        default: direction = 0; break;
    }
    s_shader->setInt("uDirection", direction);

    // Render quad (6 vertices for 2 triangles)
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

bool VirtualControls::HandleTouch(float x, float y, bool isPressed) {
    if (!s_initialized || !s_enabled) return false;
    
    bool handled = false;
    
    for (int i = 0; i < 4; i++) {
        bool hit = IsPointInButton(s_buttons[i], x, y);
        
        if (hit) {
            UpdateButtonState(s_buttons[i], isPressed);
            handled = true;
        } else if (!isPressed) {
            // Release button if touch is not on it and this is a release event
            UpdateButtonState(s_buttons[i], false);
        }
    }
    
    return handled;
}

bool VirtualControls::IsPointInButton(const VirtualButton& button, float x, float y) {
    // Check if point is within button bounds
    float left = button.x - button.width / 2;
    float right = button.x + button.width / 2;
    float top = button.y - button.height / 2;
    float bottom = button.y + button.height / 2;
    
    return (x >= left && x <= right && y >= top && y <= bottom);
}

void VirtualControls::UpdateButtonState(VirtualButton& button, bool pressed) {
    if (button.isPressed != pressed) {
        button.isPressed = pressed;
        
        // Send input event to InputManager
        Input::KeyAction action = pressed ? Input::KeyAction::PRESS : Input::KeyAction::RELEASE;
        InputManager::OnKeyEvent(button.key, action);
        
        __android_log_print(ANDROID_LOG_DEBUG, "GAM300", "Virtual button %d %s", 
                           static_cast<int>(button.key), pressed ? "pressed" : "released");
    }
}

#endif // ANDROID