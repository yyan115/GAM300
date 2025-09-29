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
#include <Sound/AudioComponent.hpp>

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

                // Draw AudioComponent if present
                if (ecsManager.HasComponent<AudioComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
                        DrawAudioComponent(selectedEntity);
                    }
                }

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
        Vector3D rotationEuler = transform.localRotation.ToEulerDegrees();
        float rotation[3] = { rotationEuler.x, rotationEuler.y, rotationEuler.z };
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

void InspectorPanel::DrawAudioComponent(Entity entity) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
        AudioComponent& audio = ecsManager.GetComponent<AudioComponent>(entity);

        ImGui::PushID("AudioComponent");

        // Asset path input
        char buffer[512] = {0};
        if (!audio.AudioAssetPath.empty()) {
            strncpy_s(buffer, audio.AudioAssetPath.c_str(), sizeof(buffer)-1);
        }
        ImGui::Text("Audio Asset Path");
        ImGui::SameLine();
        if (ImGui::InputText("##AudioPath", buffer, sizeof(buffer))) {
            audio.SetAudioAssetPath(std::string(buffer));
        }

        // Volume slider
        float vol = audio.Volume;
        if (ImGui::SliderFloat("Volume", &vol, 0.0f, 1.0f)) {
            audio.Volume = vol;
            //if (audio.Channel) {
            //    FMOD_Channel_SetVolume(reinterpret_cast<FMOD_CHANNEL*>(0), audio.Volume); // placeholder if needed
            //}
        }

        // Loop checkbox
        if (ImGui::Checkbox("Loop", &audio.Loop)) {
            // no immediate action; applied at play time
        }

        // Play on Awake
        ImGui::Checkbox("Play On Start", &audio.PlayOnStart);

        // Spatialize
        if (ImGui::Checkbox("Spatialize", &audio.Spatialize)) {
            // toggled
        }

        // Attenuation
        float att = audio.Attenuation;
        if (ImGui::SliderFloat("Attenuation", &att, 0.0f, 10.0f)) {
            audio.Attenuation = att;
        }

        // Position (if spatialized)
        if (audio.Spatialize) {
            float pos[3] = { audio.Position.x, audio.Position.y, audio.Position.z };
            if (ImGui::DragFloat3("Position", pos, 0.1f)) {
                /*audio.UpdatePosition(Vector3D(pos[0], pos[1], pos[2]));*/
                // Also update Transform if present
                if (ecsManager.HasComponent<Transform>(entity)) {
                    ecsManager.transformSystem->SetLocalPosition(entity, { pos[0], pos[1], pos[2] });
                }
            }
        }

        // Play/Stop buttons
        if (ImGui::Button("Play")) {
            audio.Play();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            audio.Stop();
        }

		ImGui::PopID();
	}
	catch (const std::exception& e)
	{
		ImGui::Text("Error accessing AudioComponent: %s", e.what());
	}
}
