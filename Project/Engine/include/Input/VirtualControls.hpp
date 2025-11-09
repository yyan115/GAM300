/* Start Header ************************************************************************/
/*!
\file       VirtualControls.hpp
\author     Yan Yu
\date       Oct 8, 2025
\brief      Virtual on-screen button controls for Android touch input, providing
            WASD-style movement buttons with visual feedback and input mapping

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#ifdef ANDROID

#include <Input/Keys.h>
#include <Graphics/OpenGL.h>
#include <Graphics/ShaderClass.h>
#include <memory>

struct VirtualJoystick {
    float baseX, baseY;      // Base/outer circle center position (0-1 normalized screen coords)
    float stickX, stickY;    // Inner stick position (0-1 normalized screen coords)
    float outerRadius;       // Outer circle radius (normalized)
    float innerRadius;       // Inner circle radius (normalized)
    bool isActive;           // Is joystick being touched
    float alpha;             // Transparency
};

class VirtualControls {
public:
    static void Initialize();
    static void InitializeJoystick();
    static void InitializeRenderResources();
    static void Shutdown();

    // Render the virtual controls overlay
    static void Render(int screenWidth, int screenHeight);

    // Check if a touch position hits the joystick
    static bool HandleTouch(float x, float y, bool isPressed);

    // Enable/disable virtual controls
    static void SetEnabled(bool enabled) { s_enabled = enabled; }
    static bool IsEnabled() { return s_enabled; }

private:
    static bool s_initialized;
    static bool s_enabled;

    // Virtual joystick for movement
    static VirtualJoystick s_joystick;

    // Rendering resources
    static GLuint s_vao;
    static GLuint s_vbo;
    static std::shared_ptr<Shader> s_shader;

    // Helper methods
    static bool IsPointInCircle(float px, float py, float cx, float cy, float radius);
    static void RenderJoystick(int screenWidth, int screenHeight);
    static void UpdateJoystickState(float touchX, float touchY, bool isPressed);
    static void UpdateKeysFromJoystick();
};

#endif // ANDROID