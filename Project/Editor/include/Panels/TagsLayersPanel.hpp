/* Start Header ************************************************************************/
/*!
\file       TagsLayersPanel.hpp
\author     Muhammad Zikry
\date       2025
\brief      Panel for managing tags and layers within the editor.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include "EditorPanel.hpp"
#include "pch.h"

/**
 * @brief Tags & Layers management panel for the editor.
 * 
 * This panel provides a centralized location for managing tags and layers,
 * similar to Unity's Tags & Layers window. It allows adding, removing, and
 * editing tags and layers that are used throughout the project.
 */
class TagsLayersPanel : public EditorPanel {
public:
    TagsLayersPanel();
    virtual ~TagsLayersPanel() = default;

    /**
     * @brief Render the tags & layers panel's ImGui content.
     */
    void OnImGuiRender() override;

private:
    /**
     * @brief Render the tags management section.
     */
    void RenderTagsSection();

    /**
     * @brief Render the layers management section.
     */
    void RenderLayersSection();

    /**
     * @brief Render the sorting layers management section.
     */
    void RenderSortingLayersSection();

    /**
     * @brief Add a new tag.
     * @param tagName The name of the tag to add.
     */
    void AddTag(const std::string& tagName);

    /**
     * @brief Remove a tag by index.
     * @param tagIndex The index of the tag to remove.
     */
    void RemoveTag(int tagIndex);

    /**
     * @brief Add a new layer.
     * @param layerName The name of the layer to add.
     */
    void AddLayer(const std::string& layerName);

    /**
     * @brief Remove a layer by index.
     * @param layerIndex The index of the layer to remove.
     */
    void RemoveLayer(int layerIndex);

    // UI state
    char newTagBuffer[64] = {0};
    char newLayerBuffer[64] = {0};
    char newSortingLayerBuffer[64] = {0};
    int selectedTagForRemoval = -1;
    int selectedLayerForRemoval = -1;
    int selectedSortingLayerForRemoval = -1;
};