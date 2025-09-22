#pragma once
#include "pch.h"
#include <unordered_map>
#include <WindowManager.hpp>
#ifndef ANDROID
#include <GLFW/glfw3.h>
#endif
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

    extern Window window;
    extern Input input;
    //TimeManager
    extern double deltaTime;
    extern double lastFrameTime;
    //extern class IPlatform* platform;
    //extern PlatformWindow ptrWindow;
}