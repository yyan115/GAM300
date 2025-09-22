#pragma once

// Platform abstraction for OpenGL vs OpenGL ES
#ifdef ANDROID
    // Android uses OpenGL ES
    #include <GLES3/gl3.h>
    #include <GLES3/gl3ext.h>
    #include <EGL/egl.h>

    // OpenGL ES 3.0 has VAO functions as core, no mapping needed
#else
    // Desktop uses OpenGL with GLAD
    #include <glad/glad.h>
#endif

// Common OpenGL type aliases and constants that work on both platforms
// (Both OpenGL and OpenGL ES define these the same way)