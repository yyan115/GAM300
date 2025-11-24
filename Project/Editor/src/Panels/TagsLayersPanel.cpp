#include "Panels/TagsLayersPanel.hpp"
#include "ECS/TagManager.hpp"
#include "ECS/LayerManager.hpp"
#include <imgui.h>

TagsLayersPanel::TagsLayersPanel()
    : EditorPanel("Tags & Layers", false) {
}

void TagsLayersPanel::OnImGuiRender() {
    if (!isOpen) return;

    ImGui::Begin("Tags & Layers", &isOpen);

    if (ImGui::BeginTabBar("TagsLayersTabBar")) {
        if (ImGui::BeginTabItem("Tags")) {
            RenderTagsSection();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Layers")) {
            RenderLayersSection();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void TagsLayersPanel::RenderTagsSection() {
    ImGui::Text("Manage Tags");
    ImGui::Separator();

    // Display current tags
    ImGui::Text("Current Tags:");
    ImGui::BeginChild("TagsList", ImVec2(0, 150), true);

    if (ImGui::BeginTable("TagsTable", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 60.0f);

        const auto& tags = TagManager::GetInstance().GetAllTags();
        for (size_t i = 0; i < tags.size(); ++i) {
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
    }

    // Add new tag
    ImGui::Separator();
    ImGui::Text("Add New Tag:");
    ImGui::InputText("Tag Name", newTagBuffer, sizeof(newTagBuffer));

    if (ImGui::Button("Add Tag") && strlen(newTagBuffer) > 0) {
        AddTag(newTagBuffer);
        memset(newTagBuffer, 0, sizeof(newTagBuffer));
    }
}

void TagsLayersPanel::RenderLayersSection() {
    ImGui::Text("Manage Layers");
    ImGui::Separator();

    // Display current layers
    ImGui::Text("Current Layers:");
    ImGui::BeginChild("LayersList", ImVec2(0, 150), true);

    if (ImGui::BeginTable("LayersTable", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 60.0f);

        const auto& layers = LayerManager::GetInstance().GetAllLayers();
        for (size_t i = 0; i < layers.size(); ++i) {
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
    ImGui::InputText("Layer Name", newLayerBuffer, sizeof(newLayerBuffer));

    if (ImGui::Button("Add Layer") && strlen(newLayerBuffer) > 0) {
        AddLayer(newLayerBuffer);
        memset(newLayerBuffer, 0, sizeof(newLayerBuffer));
    }
}

void TagsLayersPanel::AddTag(const std::string& tagName) {
    TagManager::GetInstance().AddTag(tagName);
}

void TagsLayersPanel::RemoveTag(int tagIndex) {
    TagManager::GetInstance().RemoveTag(tagIndex);
}

void TagsLayersPanel::AddLayer(const std::string& layerName) {
    LayerManager::GetInstance().AddLayer(layerName);
}

void TagsLayersPanel::RemoveLayer(int layerIndex) {
    LayerManager::GetInstance().RemoveLayer(layerIndex);
}