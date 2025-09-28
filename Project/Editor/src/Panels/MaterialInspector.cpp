#include "Panels/MaterialInspector.hpp"
#include "imgui.h"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include <Graphics/Texture.h>
#include <Asset Manager/ResourceManager.hpp>
#include <iostream>
#include <vector>
#include <filesystem>

void MaterialInspector::DrawMaterialAsset(std::shared_ptr<Material> material, const std::string& assetPath) {
    if (!material) return;

    bool materialChanged = false;

    // Colors section
    if (ImGui::CollapsingHeader("Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Ambient Color
        glm::vec3 ambient = material->GetAmbient();
        float ambientColor[3] = { ambient.r, ambient.g, ambient.b };
        if (ImGui::ColorEdit3("Ambient", ambientColor)) {
            material->SetAmbient(glm::vec3(ambientColor[0], ambientColor[1], ambientColor[2]));
            materialChanged = true;
        }

        // Diffuse Color
        glm::vec3 diffuse = material->GetDiffuse();
        float diffuseColor[3] = { diffuse.r, diffuse.g, diffuse.b };
        if (ImGui::ColorEdit3("Diffuse", diffuseColor)) {
            material->SetDiffuse(glm::vec3(diffuseColor[0], diffuseColor[1], diffuseColor[2]));
            materialChanged = true;
        }

        // Specular Color
        glm::vec3 specular = material->GetSpecular();
        float specularColor[3] = { specular.r, specular.g, specular.b };
        if (ImGui::ColorEdit3("Specular", specularColor)) {
            material->SetSpecular(glm::vec3(specularColor[0], specularColor[1], specularColor[2]));
            materialChanged = true;
        }

        // Shininess
        float shininess = material->GetShininess();
        if (ImGui::SliderFloat("Shininess", &shininess, 1.0f, 256.0f)) {
            material->SetShininess(shininess);
            materialChanged = true;
        }
    }

    // Textures section
    if (ImGui::CollapsingHeader("Textures")) {
        // Texture type mappings
        static const std::vector<std::pair<TextureType, std::string>> textureTypes = {
            {TextureType::DIFFUSE, "Diffuse"},
            {TextureType::SPECULAR, "Specular"},
            {TextureType::AMBIENT_OCCLUSION, "Ambient Occlusion"},
            {TextureType::HEIGHT, "Height"},
            {TextureType::NORMAL, "Normal"},
            {TextureType::METALLIC, "Metallic"},
            {TextureType::ROUGHNESS, "Roughness"}
        };

        for (const auto& [type, name] : textureTypes) {
            // Push unique ID for this texture slot
            ImGui::PushID(static_cast<int>(type));

            // Get current texture path
            std::string currentPath;
            if (auto textureInfo = material->GetTextureInfo(type)) {
                currentPath = textureInfo->get().filePath;
            }

            // Texture slot (drag-drop target + display)
            ImGui::Text("%s:", name.c_str());
            ImGui::SameLine();

            // Calculate sizes for layout
            float availableWidth = ImGui::GetContentRegionAvail().x;
            float buttonWidth = 60.0f;
            float textureFieldWidth = availableWidth - buttonWidth - ImGui::GetStyle().ItemSpacing.x;

            // Texture display field (also drag-drop target)
            std::string textureDisplay;
            if (currentPath.empty()) {
                textureDisplay = "None (Drag texture here)";
            } else {
                // Show just the filename for cleaner display
                std::filesystem::path pathObj(currentPath);
                textureDisplay = pathObj.filename().string();
            }
            ImGui::Button(textureDisplay.c_str(), ImVec2(textureFieldWidth, 0));

            // Drag-drop target for textures
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_PAYLOAD")) {
                    // Extract texture path from payload
                    const char* texturePath = (const char*)payload->Data;
                    std::string pathStr(texturePath, payload->DataSize);

                    // Use the original path as-is since it already has the correct relative path
                    std::string assetPathStr = pathStr;

                    std::cout << "[MaterialInspector] Using original path: " << assetPathStr << std::endl;

                    // Convert TextureType to string for AssetManager
                    std::string typeName;
                    switch (type) {
                    case TextureType::DIFFUSE: typeName = "diffuse"; break;
                    case TextureType::SPECULAR: typeName = "specular"; break;
                    case TextureType::AMBIENT_OCCLUSION: typeName = "ambient_occlusion"; break;
                    case TextureType::HEIGHT: typeName = "height"; break;
                    case TextureType::NORMAL: typeName = "normal"; break;
                    case TextureType::METALLIC: typeName = "metallic"; break;
                    case TextureType::ROUGHNESS: typeName = "roughness"; break;
                    default: typeName = "diffuse"; break;
                    }

                    // Create texture info with path - the material system will handle actual loading
                    std::cout << "[MaterialInspector] Setting texture path: " << assetPathStr << std::endl;

                    // Create a TextureInfo with the path - the texture will be loaded when actually needed by the renderer
                    auto textureInfo = std::make_unique<TextureInfo>(assetPathStr, nullptr);
                    material->SetTexture(type, std::move(textureInfo));
                    materialChanged = true;
                    std::cout << "[MaterialInspector] Successfully set texture path on material: " << assetPathStr << std::endl;

                }
                ImGui::EndDragDropTarget();
            }

            // Select button
            ImGui::SameLine();
            std::string selectLabel = "...##" + name;
            if (ImGui::Button(selectLabel.c_str(), ImVec2(buttonWidth, 0))) {
                // TODO: Open file dialog here
                std::cout << "[MaterialInspector] File dialog for " << name << " texture would open here" << std::endl;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Select texture file");
            }

            // Pop the unique ID
            ImGui::PopID();
        }

        // Save button - always show it for material editing
        ImGui::Separator();
        if (ImGui::Button("Save Material")) {
            // Use the Material's CompileToResource method
            std::string compiledPath = material->CompileToResource(assetPath);
            if (!compiledPath.empty()) {
                std::cout << "[MaterialInspector] Material saved: " << assetPath << std::endl;

                // Force ResourceManager to reload the material from disk to update cache
                std::cout << "[MaterialInspector] Forcing ResourceManager cache reload for: " << assetPath << std::endl;
                auto reloadedMaterial = ResourceManager::GetInstance().GetResource<Material>(assetPath, true);
                if (reloadedMaterial) {
                    std::cout << "[MaterialInspector] Successfully reloaded material in ResourceManager cache: " << reloadedMaterial->GetName() << " with " << reloadedMaterial->GetAllTextureInfo().size() << " textures" << std::endl;
                } else {
                    std::cout << "[MaterialInspector] Failed to reload material in ResourceManager cache" << std::endl;
                }

                materialChanged = false; // Reset change flag after save
            }
            else {
                std::cout << "[MaterialInspector] Failed to save material: " << assetPath << std::endl;
            }
        }
        if (materialChanged) {
            ImGui::SameLine();
            ImGui::Text("(Material has unsaved changes)");
        }
    }
}