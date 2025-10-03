#pragma once

#include "EditorPanel.hpp"
#include <vector>
#include <string>

/**
 * @brief Panel for displaying the game/runtime view.
 *
 * This panel shows the game as it would appear to the end user,
 * without editor gizmos or tools. It's essentially a preview window.
 */
class GamePanel : public EditorPanel {
public:
    GamePanel();
    virtual ~GamePanel() = default;

    /**
     * @brief Render the game panel's ImGui content.
     */
    void OnImGuiRender() override;

private:
    /**
     * @brief Render the resolution panel toolbar
     */
    void RenderResolutionPanel();

    /**
     * @brief Calculate viewport dimensions with aspect ratio preservation
     * @param availableWidth Available panel width
     * @param availableHeight Available panel height
     * @param viewportWidth Output viewport width
     * @param viewportHeight Output viewport height
     * @param offsetX Output X offset for centering
     * @param offsetY Output Y offset for centering
     */
    void CalculateViewportDimensions(int availableWidth, int availableHeight,
                                   int& viewportWidth, int& viewportHeight,
                                   float& offsetX, float& offsetY);

    // Resolution settings
    struct Resolution {
        int width;
        int height;
        std::string name;
        Resolution(int w, int h, const std::string& n) : width(w), height(h), name(n) {}
    };

    std::vector<Resolution> resolutions;
    int selectedResolutionIndex;
    bool useCustomAspectRatio;
    float customAspectRatio;
    bool freeAspect;
};