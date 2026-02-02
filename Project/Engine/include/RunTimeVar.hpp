#pragma once
#include "pch.h"
#include "Graphics/OpenGL.h"
#include <unordered_map>
#include "Engine.h"
#include "Input/Keys.h"

namespace RunTimeVar {
    struct Window {
        GLint width = 0;
        GLint height = 0;
        GLint viewportWidth = 0;
        GLint viewportHeight = 0;
        const char* title = "Untitled";

        GLint windowedWidth = 1600; // Default windowed size
        GLint windowedHeight = 900;  // Default windowed size
        GLint windowedPosX = 0;    // Default window position
        GLint windowedPosY = 0;    // Default window position

        GLint gameResolutionWidth = 1600;
        GLint gameResolutionHeight = 900;
        GLint gameViewportWidth = 1600;
        GLint gameViewportHeight = 900;

        bool isFullscreen = false;
        bool isFocused = true;
    };

    struct Input {
        //to be port over
        double mouseX = 0.0;
        double mouseY = 0.0;
        double scrollOffsetX = 0.0;
        double scrollOffsetY = 0.0;

    };

    extern ENGINE_API Window window;
    extern ENGINE_API Input input;
    //TimeManager
    extern ENGINE_API double deltaTime;
    extern ENGINE_API double lastFrameTime;
    extern ENGINE_API double unscaledDeltaTime;
    //extern class IPlatform* platform;
    //extern PlatformWindow ptrWindow;
}