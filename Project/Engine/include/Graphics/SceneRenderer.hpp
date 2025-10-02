#pragma once

#include "Graphics/OpenGL.h"
#include <glm/glm.hpp>

#ifdef _WIN32
#ifdef ENGINE_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
#else
// Linux/GCC
#ifdef ENGINE_EXPORTS
#define ENGINE_API __attribute__((visibility("default")))
#else
#define ENGINE_API
#endif
#endif

class Camera;

/**
 * @brief Handles scene rendering operations for editor and runtime
 *
 * This class manages framebuffer operations and scene rendering
 * specifically for editor viewports and scene previews.
 */
class ENGINE_API SceneRenderer {
public:
    /**
     * @brief Create or resize scene framebuffer
     * @param width Width of the framebuffer
     * @param height Height of the framebuffer
     * @return Framebuffer ID
     */
    static unsigned int CreateSceneFramebuffer(int width, int height);

    /**
     * @brief Delete the scene framebuffer and associated textures
     */
    static void DeleteSceneFramebuffer();

    /**
     * @brief Get the scene color texture for display
     * @return OpenGL texture ID
     */
    static unsigned int GetSceneTexture();

    /**
     * @brief Begin rendering to scene framebuffer
     * @param width Framebuffer width
     * @param height Framebuffer height
     */
    static void BeginSceneRender(int width, int height);

    /**
     * @brief End scene rendering (unbind framebuffer)
     */
    static void EndSceneRender();

    /**
     * @brief Render the scene using the current game camera
     */
    static void RenderScene();

    /**
     * @brief Render scene for editor with default camera
     */
    static void RenderSceneForEditor();

    /**
     * @brief Render scene for editor with custom camera parameters
     * @param cameraPos Camera position
     * @param cameraFront Camera front vector
     * @param cameraUp Camera up vector
     * @param cameraZoom Camera zoom/FOV
     * @param orthoZoomLevel Orthographic zoom level for 2D mode
     */
    static void RenderSceneForEditor(const glm::vec3& cameraPos,
                                   const glm::vec3& cameraFront,
                                   const glm::vec3& cameraUp,
                                   float cameraZoom,
                                   float orthoZoomLevel = 1.0f);

private:
    // Static framebuffer data
    static unsigned int sceneFrameBuffer;
    static unsigned int sceneColorTexture;
    static unsigned int sceneDepthTexture;
    static int sceneWidth;
    static int sceneHeight;

    // Static editor camera for rendering
    static Camera* editorCamera;

    // Private constructor - static class
    SceneRenderer() = delete;
};