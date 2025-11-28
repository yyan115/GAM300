/* Start Header ************************************************************************/
/*!
\file       TagsLayersPanel.cpp
\author     Muhammad Zikry
\date       2025
\brief      Panel for managing tags and layers within the editor.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "Panels/TagsLayersPanel.hpp"
#include "ECS/TagManager.hpp"
#include "ECS/LayerManager.hpp"
#include "ECS/SortingLayerManager.hpp"
#include "ECS/TagsLayersSettings.hpp"
#include <imgui.h>

TagsLayersPanel::TagsLayersPanel()
    : EditorPanel("Tags & Layers", false) {
}

void TagsLayersPanel::OnImGuiRender() {
    if (!isOpen) return;

    ImGui::Begin("Tags & Layers", &isOpen);

    // Push custom colors for tabs to make them more visible
    ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));                    // Inactive tab - darker gray
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.35f, 0.5f, 0.7f, 1.0f));              // Hovered tab - blue tint
    ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.3f, 0.45f, 0.65f, 1.0f));             // Active tab - blue
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(0.25f, 0.35f, 0.5f, 1.0f));   // Unfocused active - dimmer blue

    if (ImGui::BeginTabBar("TagsLayersTabBar")) {
        if (ImGui::BeginTabItem("Tags")) {
            RenderTagsSection();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Layers")) {
            RenderLayersSection();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Sorting Layers")) {
            RenderSortingLayersSection();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // Pop the custom tab colors
    ImGui::PopStyleColor(4);

    ImGui::End();
}

void TagsLayersPanel::RenderTagsSection() {
    ImGui::Text("Manage Tags");
    ImGui::Separator();

    // Display current tags
    ImGui::Text("Current Tags:");
    // Use flexible height - take available space minus space for add section
    float availableHeight = ImGui::GetContentRegionAvail().y - 100.0f;
    ImGui::BeginChild("TagsList", ImVec2(0, availableHeight), true);

    if (ImGui::BeginTable("TagsTable", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 60.0f);

        const auto& tags = TagManager::GetInstance().GetAllTags();
        for (size_t i = 0; i < tags.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", tags[i].c_str());

            ImGui::TableNextColumn();
            // Remove button (don't allow removing the default "Untagged" tag at index 0)
            if (i > 0) {
                if (ImGui::Button("Remove")) {
                    selectedTagForRemoval = static_cast<int>(i);
                }
            } else {
                ImGui::TextDisabled("Default");
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();

    // Handle tag removal
    if (selectedTagForRemoval >= 0) {
        // Note: In a real implementation, you'd want to check if the tag is in use
        // and warn the user or prevent removal. For now, we'll just remove it.
        TagManager::GetInstance().RemoveTag(selectedTagForRemoval);
        selectedTagForRemoval = -1;
        // Save project settings after modification
        TagsLayersSettings::GetInstance().SaveSettings();
    }

    // Add new tag
    ImGui::Separator();
    ImGui::Text("Add New Tag:");
    ImGui::InputText("##TagName", newTagBuffer, sizeof(newTagBuffer));

    if (ImGui::Button("Add Tag##AddTagButton") && strlen(newTagBuffer) > 0) {
        AddTag(newTagBuffer);
        memset(newTagBuffer, 0, sizeof(newTagBuffer));
    }
}

void TagsLayersPanel::RenderLayersSection() {
    ImGui::Text("Manage Layers");
    ImGui::Separator();

    // Display current layers
    ImGui::Text("Current Layers:");
    // Use flexible height - take available space minus space for add section
    float availableHeight = ImGui::GetContentRegionAvail().y - 100.0f;
    ImGui::BeginChild("LayersList", ImVec2(0, availableHeight), true);

    if (ImGui::BeginTable("LayersTable", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 60.0f);

        const auto& layers = LayerManager::GetInstance().GetAllLayers();
        for (size_t i = 0; i < layers.size(); ++i) {
            // Skip empty layers
            if (layers[i].empty()) continue;

            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", layers[i].c_str());

            ImGui::TableNextColumn();
            // Remove button (don't allow removing the default "Default" layer at index 0)
            if (i > 0) {
                if (ImGui::Button("Remove")) {
                    selectedLayerForRemoval = static_cast<int>(i);
                }
            } else {
                ImGui::TextDisabled("Default");
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();

    // Handle layer removal
    if (selectedLayerForRemoval >= 0) {
        // Note: In a real implementation, you'd want to check if the layer is in use
        LayerManager::GetInstance().RemoveLayer(selectedLayerForRemoval);
        selectedLayerForRemoval = -1;
    }

    // Add new layer
    ImGui::Separator();
    ImGui::Text("Add New Layer:");
    ImGui::InputText("##LayerName", newLayerBuffer, sizeof(newLayerBuffer));

    if (ImGui::Button("Add Layer##AddLayerButton") && strlen(newLayerBuffer) > 0) {
        AddLayer(newLayerBuffer);
        memset(newLayerBuffer, 0, sizeof(newLayerBuffer));
    }
}

void TagsLayersPanel::AddTag(const std::string& tagName) {
    TagManager::GetInstance().AddTag(tagName);
    // Save project settings after modification
    TagsLayersSettings::GetInstance().SaveSettings();
}

void TagsLayersPanel::RemoveTag(int tagIndex) {
    TagManager::GetInstance().RemoveTag(tagIndex);
}

void TagsLayersPanel::AddLayer(const std::string& layerName) {
    LayerManager::GetInstance().AddLayer(layerName);
    // Save project settings after modification
    TagsLayersSettings::GetInstance().SaveSettings();
}

void TagsLayersPanel::RemoveLayer(int layerIndex) {
    LayerManager::GetInstance().RemoveLayer(layerIndex);
    // Save project settings after modification
    TagsLayersSettings::GetInstance().SaveSettings();
}

void TagsLayersPanel::RenderSortingLayersSection() {
    ImGui::Text("Manage Sorting Layers");
    ImGui::Separator();
    ImGui::TextWrapped("Sorting layers control the order in which 2D sprites and text are rendered. Higher order = rendered on top.");
    ImGui::Spacing();

    // Display current sorting layers
    ImGui::Text("Current Sorting Layers (in rendering order):");
    // Use flexible height - take available space minus space for add section
    float availableHeight = ImGui::GetContentRegionAvail().y - 150.0f;
    ImGui::BeginChild("SortingLayersList", ImVec2(0, availableHeight), true);

    if (ImGui::BeginTable("SortingLayersTable", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Order", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        const auto& sortingLayers = SortingLayerManager::GetInstance().GetAllLayers();
        for (size_t i = 0; i < sortingLayers.size(); ++i) {
            const auto& layer = sortingLayers[i];

            ImGui::TableNextRow();

            // Order column
            ImGui::TableNextColumn();
            ImGui::Text("%d", layer.order);

            // Name column
            ImGui::TableNextColumn();
            ImGui::Text("%s", layer.name.c_str());

            // Action column
            ImGui::TableNextColumn();

            // Can't remove Default sorting layer
            if (layer.id == 0) {
                ImGui::TextDisabled("Default");
            } else {
                ImGui::PushID(layer.id);
                if (ImGui::Button("Remove")) {
                    selectedSortingLayerForRemoval = layer.id;
                }
                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();

    // Handle sorting layer removal
    if (selectedSortingLayerForRemoval >= 0) {
        SortingLayerManager::GetInstance().RemoveLayer(selectedSortingLayerForRemoval);
        selectedSortingLayerForRemoval = -1;
        // Save project settings after modification
        TagsLayersSettings::GetInstance().SaveSettings();
    }

    // Add new sorting layer
    ImGui::Separator();
    ImGui::Text("Add New Sorting Layer:");
    ImGui::InputText("##SortingLayerName", newSortingLayerBuffer, sizeof(newSortingLayerBuffer));
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("New layers are added at the end (rendered on top)");
    }

    if (ImGui::Button("Add Sorting Layer##AddSortingLayerButton") && strlen(newSortingLayerBuffer) > 0) {
        int newID = SortingLayerManager::GetInstance().AddLayer(newSortingLayerBuffer);
        if (newID >= 0) {
            memset(newSortingLayerBuffer, 0, sizeof(newSortingLayerBuffer));
            // Save project settings after modification
            TagsLayersSettings::GetInstance().SaveSettings();
        } else {
            // Layer already exists or too many layers
            ImGui::OpenPopup("AddSortingLayerError");
        }
    }

    // Error popup
    if (ImGui::BeginPopup("AddSortingLayerError")) {
        ImGui::Text("Failed to add sorting layer!");
        ImGui::Text("Layer name may already exist or max limit reached.");
        if (ImGui::Button("OK")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}