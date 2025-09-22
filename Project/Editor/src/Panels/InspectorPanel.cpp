#include "Panels/InspectorPanel.hpp"
#include "imgui.h"
#include "GUIManager.hpp"
#include <Graphics/Model/ModelRenderComponent.hpp>
#include <cstring>
#include <string>
#include <cstddef>
#include <unordered_map>
#include <vector>
#include <algorithm>

InspectorPanel::InspectorPanel() 
    : EditorPanel("Inspector", true) {
}

void InspectorPanel::OnImGuiRender() {
    if (ImGui::Begin(name.c_str(), &isOpen)) {
        Entity selectedEntity = GUIManager::GetSelectedEntity();

        if (selectedEntity == static_cast<Entity>(-1)) {
            ImGui::Text("No object selected");
            ImGui::Text("Select an object in the Scene Hierarchy to view its properties");
        } else {
            try {
                // Get the active ECS manager
                ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

                ImGui::Text("Entity ID: %u", selectedEntity);
                ImGui::Separator();

                // Draw NameComponent if it exists
                if (ecsManager.HasComponent<NameComponent>(selectedEntity)) {
                    DrawNameComponent(selectedEntity);
                    ImGui::Separator();
                }

                // Draw Transform component if it exists
                if (ecsManager.HasComponent<Transform>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                        DrawTransformComponent(selectedEntity);
                    }
                }

                // Draw ModelRenderComponent if it exists
                if (ecsManager.HasComponent<ModelRenderComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Model Renderer")) {
                        DrawModelRenderComponent(selectedEntity);
                    }
                }

                //ImGui::Separator();

                //// Add Component button
                //if (ImGui::Button("Add Component")) {
                //    ImGui::OpenPopup("AddComponentPopup");
                //}

                //if (ImGui::BeginPopup("AddComponentPopup")) {
                //    if (ImGui::MenuItem("Transform")) {
                //        if (!ecsManager.HasComponent<Transform>(selectedEntity)) {
                //            ecsManager.AddComponent<Transform>(selectedEntity, Transform{});
                //        }
                //    }
                //    if (ImGui::MenuItem("Name Component")) {
                //        if (!ecsManager.HasComponent<NameComponent>(selectedEntity)) {
                //            ecsManager.AddComponent<NameComponent>(selectedEntity, NameComponent{"Entity " + std::to_string(selectedEntity)});
                //        }
                //    }
                //    ImGui::EndPopup();
                //}

            } catch (const std::exception& e) {
                ImGui::Text("Error accessing entity: %s", e.what());
            }
        }
    }
    ImGui::End();
}

void InspectorPanel::DrawNameComponent(Entity entity) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        NameComponent& nameComponent = ecsManager.GetComponent<NameComponent>(entity);

        ImGui::PushID("NameComponent");

        // Use a static map to maintain state per entity
        static std::unordered_map<Entity, std::vector<char>> nameBuffers;

        // Get or create buffer for this entity
        auto& nameBuffer = nameBuffers[entity];

        // Initialize buffer if empty or different from component
        std::string currentName = nameComponent.name;
        if (nameBuffer.empty() || std::string(nameBuffer.data()) != currentName) {
            nameBuffer.clear();
            nameBuffer.resize(256, '\0'); // Create 256-char buffer filled with null terminators
            if (!currentName.empty() && currentName.length() < 255) {
                std::copy(currentName.begin(), currentName.end(), nameBuffer.begin());
            }
        }

        // Use InputText with char buffer
        ImGui::Text("Name");
        ImGui::SameLine();
        if (ImGui::InputText("##Name", nameBuffer.data(), nameBuffer.size())) {
            // Update the actual component
            nameComponent.name = std::string(nameBuffer.data());
        }

        ImGui::PopID();
    } catch (const std::exception& e) {
        ImGui::Text("Error accessing NameComponent: %s", e.what());
    }
}

void InspectorPanel::DrawTransformComponent(Entity entity) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        Transform& transform = ecsManager.GetComponent<Transform>(entity);

        ImGui::PushID("Transform");

        // Position
        float position[3] = { transform.localPosition.x, transform.localPosition.y, transform.localPosition.z };
        ImGui::Text("Position");
        ImGui::SameLine();
        if (ImGui::DragFloat3("##Position", position, 0.1f, -FLT_MAX, FLT_MAX, "%.3f")) {
            ecsManager.transformSystem->SetLocalPosition(entity, { position[0], position[1], position[2] });
        }

        // Rotation
        float rotation[3] = { transform.localRotation.x, transform.localRotation.y, transform.localRotation.z };
        ImGui::Text("Rotation");
        ImGui::SameLine();
        if (ImGui::DragFloat3("##Rotation", rotation, 1.0f, -180.0f, 180.0f, "%.1f")) {
            ecsManager.transformSystem->SetLocalRotation(entity, { rotation[0], rotation[1], rotation[2] });
        }

        // Scale
        float scale[3] = { transform.localScale.x, transform.localScale.y, transform.localScale.z };
        ImGui::Text("Scale");
        ImGui::SameLine();
        if (ImGui::DragFloat3("##Scale", scale, 0.1f, 0.001f, FLT_MAX, "%.3f")) {
            ecsManager.transformSystem->SetLocalScale(entity, { scale[0], scale[1], scale[2] });
        }


        ImGui::PopID();
    } catch (const std::exception& e) {
        ImGui::Text("Error accessing Transform: %s", e.what());
    }
}

void InspectorPanel::DrawModelRenderComponent(Entity entity) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        ModelRenderComponent& modelRenderer = ecsManager.GetComponent<ModelRenderComponent>(entity);

        ImGui::PushID("ModelRenderComponent");

        // Display model info (read-only for now)
        ImGui::Text("Model Renderer Component");
        if (modelRenderer.model) {
            ImGui::Text("Model: Loaded");
        } else {
            ImGui::Text("Model: None");
        }

        if (modelRenderer.shader) {
            ImGui::Text("Shader: Loaded");
        } else {
            ImGui::Text("Shader: None");
        }

        ImGui::PopID();
    } catch (const std::exception& e) {
        ImGui::Text("Error accessing ModelRenderComponent: %s", e.what());
    }
}