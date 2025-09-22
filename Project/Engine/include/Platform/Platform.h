#pragma once

// Include OpenGL abstraction first so GLFW doesn't include OpenGL headers
#include "../Graphics/OpenGL.h"

// Platform detection and abstraction
#ifdef ANDROID
    // Android platform headers
    #include <android/native_activity.h>
    #include <android/asset_manager.h>
    #include <android/log.h>
    #include <EGL/egl.h>
    
    // Android window handle
    typedef ANativeWindow* PlatformWindow;
    typedef EGLDisplay PlatformDisplay;
    typedef EGLContext PlatformContext;
    typedef EGLSurface PlatformSurface;
    
    // Android callback types (different signature but same concept)
    typedef void (*PlatformErrorCallback)(int error, const char* description);
    typedef void (*PlatformFramebufferSizeCallback)(PlatformWindow window, int width, int height);
    typedef void (*PlatformFocusCallback)(PlatformWindow window, int focused);
    
#else
    // Desktop platform headers
    #include <GLFW/glfw3.h>
    #include <GLFW/glfw3native.h>
    
    // Desktop window handle
    typedef GLFWwindow* PlatformWindow;
    typedef void* PlatformDisplay;  // Not used on desktop
    typedef void* PlatformContext;   // Not used on desktop
    typedef void* PlatformSurface;   // Not used on desktop
    
    // Desktop callback types
    typedef void (*PlatformErrorCallback)(int error, const char* description);
    typedef void (*PlatformFramebufferSizeCallback)(PlatformWindow window, int width, int height);
    typedef void (*PlatformFocusCallback)(PlatformWindow window, int focused);
    
#endif

// Common platform-independent types
// OpenGL.h already included at the top