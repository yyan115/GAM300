#pragma once

#ifdef ANDROID

#include <Input/Keys.h>
#include <Graphics/OpenGL.h>
#include <Graphics/ShaderClass.h>
#include <memory>

struct VirtualButton {
    float x, y;          // Center position (0-1 normalized screen coords)
    float width, height; // Size (0-1 normalized screen coords)
    Input::Key key;      // Key this button represents
    bool isPressed;      // Current state
    float alpha;         // Transparency (for visual feedback)
};

class VirtualControls {
public:
    static void Initialize();
    static void InitializeButtons();
    static void InitializeRenderResources();
    static void Shutdown();
    
    // Render the virtual controls overlay
    static void Render(int screenWidth, int screenHeight);
    
    // Check if a touch position hits any virtual button
    static bool HandleTouch(float x, float y, bool isPressed);
    
    // Enable/disable virtual controls
    static void SetEnabled(bool enabled) { s_enabled = enabled; }
    static bool IsEnabled() { return s_enabled; }

private:
    static bool s_initialized;
    static bool s_enabled;
    
    // Virtual buttons for WASD movement
    static VirtualButton s_buttons[4]; // W, A, S, D
    
    // Rendering resources
    static GLuint s_vao;
    static GLuint s_vbo;
    static std::shared_ptr<Shader> s_shader;
    
    // Helper methods
    static bool IsPointInButton(const VirtualButton& button, float x, float y);
    static void RenderButton(const VirtualButton& button, int screenWidth, int screenHeight);
    static void UpdateButtonState(VirtualButton& button, bool pressed);
};

#endif // ANDROID