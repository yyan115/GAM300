#include "Panels/MaterialInspector.hpp"
#include "imgui.h"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include <Graphics/Texture.h>
#include <Asset Manager/ResourceManager.hpp>
#include <Asset Manager/AssetManager.hpp>
#include <ECS/ECSRegistry.hpp>
#include <Graphics/Model/ModelRenderComponent.hpp>
#include <iostream>
#include <vector>
#include <filesystem>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

void MaterialInspector::DrawMaterialAsset(std::shared_ptr<Material> material, const std::string& assetPath) {
    if (!material) return;

    bool materialChanged = false;

    // Colors section - Unity style
    if (ImGui::CollapsingHeader("Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

        // Ambient color row
        glm::vec3 ambient = material->GetAmbient();
        float ambientColor[3] = { ambient.r, ambient.g, ambient.b };

        ImGui::Text("Ambient");
        ImGui::SameLine();
        ImGui::Text("R: %.0f", ambientColor[0] * 255);
        ImGui::SameLine();
        if (ImGui::ColorButton("##ambient_r", ImVec4(ambientColor[0], 0, 0, 1), ImGuiColorEditFlags_NoTooltip, ImVec2(30, 20))) {
            ImGui::OpenPopup("ambient_color_picker");
        }
        ImGui::SameLine();
        ImGui::Text("G: %.0f", ambientColor[1] * 255);
        ImGui::SameLine();
        if (ImGui::ColorButton("##ambient_g", ImVec4(0, ambientColor[1], 0, 1), ImGuiColorEditFlags_NoTooltip, ImVec2(30, 20))) {
            ImGui::OpenPopup("ambient_color_picker");
        }
        ImGui::SameLine();
        ImGui::Text("B: %.0f", ambientColor[2] * 255);
        ImGui::SameLine();
        if (ImGui::ColorButton("##ambient_b", ImVec4(0, 0, ambientColor[2], 1), ImGuiColorEditFlags_NoTooltip, ImVec2(30, 20))) {
            ImGui::OpenPopup("ambient_color_picker");
        }
        ImGui::SameLine();
        if (ImGui::ColorButton("##ambient_full", ImVec4(ambientColor[0], ambientColor[1], ambientColor[2], 1), ImGuiColorEditFlags_NoTooltip, ImVec2(30, 20))) {
            ImGui::OpenPopup("ambient_color_picker");
        }

        if (ImGui::BeginPopup("ambient_color_picker")) {
            if (ImGui::ColorPicker3("Ambient Color", ambientColor)) {
                material->SetAmbient(glm::vec3(ambientColor[0], ambientColor[1], ambientColor[2]));
                materialChanged = true;
            }
            ImGui::EndPopup();
        }

        // Diffuse color row
        glm::vec3 diffuse = material->GetDiffuse();
        float diffuseColor[3] = { diffuse.r, diffuse.g, diffuse.b };

        ImGui::Text("Diffuse");
        ImGui::SameLine();
        ImGui::Text("R: %.0f", diffuseColor[0] * 255);
        ImGui::SameLine();
        if (ImGui::ColorButton("##diffuse_r", ImVec4(diffuseColor[0], 0, 0, 1), ImGuiColorEditFlags_NoTooltip, ImVec2(30, 20))) {
            ImGui::OpenPopup("diffuse_color_picker");
        }
        ImGui::SameLine();
        ImGui::Text("G: %.0f", diffuseColor[1] * 255);
        ImGui::SameLine();
        if (ImGui::ColorButton("##diffuse_g", ImVec4(0, diffuseColor[1], 0, 1), ImGuiColorEditFlags_NoTooltip, ImVec2(30, 20))) {
            ImGui::OpenPopup("diffuse_color_picker");
        }
        ImGui::SameLine();
        ImGui::Text("B: %.0f", diffuseColor[2] * 255);
        ImGui::SameLine();
        if (ImGui::ColorButton("##diffuse_b", ImVec4(0, 0, diffuseColor[2], 1), ImGuiColorEditFlags_NoTooltip, ImVec2(30, 20))) {
            ImGui::OpenPopup("diffuse_color_picker");
        }
        ImGui::SameLine();
        if (ImGui::ColorButton("##diffuse_full", ImVec4(diffuseColor[0], diffuseColor[1], diffuseColor[2], 1), ImGuiColorEditFlags_NoTooltip, ImVec2(30, 20))) {
            ImGui::OpenPopup("diffuse_color_picker");
        }

        if (ImGui::BeginPopup("diffuse_color_picker")) {
            if (ImGui::ColorPicker3("Diffuse Color", diffuseColor)) {
                material->SetDiffuse(glm::vec3(diffuseColor[0], diffuseColor[1], diffuseColor[2]));
                materialChanged = true;
            }
            ImGui::EndPopup();
        }

        // Specular color row
        glm::vec3 specular = material->GetSpecular();
        float specularColor[3] = { specular.r, specular.g, specular.b };

        ImGui::Text("Specular");
        ImGui::SameLine();
        ImGui::Text("R: %.0f", specularColor[0] * 255);
        ImGui::SameLine();
        if (ImGui::ColorButton("##specular_r", ImVec4(specularColor[0], 0, 0, 1), ImGuiColorEditFlags_NoTooltip, ImVec2(30, 20))) {
            ImGui::OpenPopup("specular_color_picker");
        }
        ImGui::SameLine();
        ImGui::Text("G: %.0f", specularColor[1] * 255);
        ImGui::SameLine();
        if (ImGui::ColorButton("##specular_g", ImVec4(0, specularColor[1], 0, 1), ImGuiColorEditFlags_NoTooltip, ImVec2(30, 20))) {
            ImGui::OpenPopup("specular_color_picker");
        }
        ImGui::SameLine();
        ImGui::Text("B: %.0f", specularColor[2] * 255);
        ImGui::SameLine();
        if (ImGui::ColorButton("##specular_b", ImVec4(0, 0, specularColor[2], 1), ImGuiColorEditFlags_NoTooltip, ImVec2(30, 20))) {
            ImGui::OpenPopup("specular_color_picker");
        }
        ImGui::SameLine();
        if (ImGui::ColorButton("##specular_full", ImVec4(specularColor[0], specularColor[1], specularColor[2], 1), ImGuiColorEditFlags_NoTooltip, ImVec2(30, 20))) {
            ImGui::OpenPopup("specular_color_picker");
        }

        if (ImGui::BeginPopup("specular_color_picker")) {
            if (ImGui::ColorPicker3("Specular Color", specularColor)) {
                material->SetSpecular(glm::vec3(specularColor[0], specularColor[1], specularColor[2]));
                materialChanged = true;
            }
            ImGui::EndPopup();
        }

        // Shininess row
        float shininess = material->GetShininess();
        float normalizedShininess = shininess / 256.0f;

        ImGui::Text("Shininess");
        ImGui::SameLine();
        ImGui::Text("%.3f", normalizedShininess);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::SliderFloat("##shininess_slider", &normalizedShininess, 0.0f, 1.0f, "")) {
            material->SetShininess(normalizedShininess * 256.0f);
            materialChanged = true;
        }

        ImGui::PopStyleVar(2);
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

            // Unity-style texture slot layout
            ImGui::Text("%s:", name.c_str());
            ImGui::SameLine();

            // Calculate sizes for Unity-like layout
            float availableWidth = ImGui::GetContentRegionAvail().x;
            float removeButtonWidth = 20.0f;
            float selectButtonWidth = 20.0f;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float textureFieldWidth = availableWidth - removeButtonWidth - selectButtonWidth - (spacing * 2);

            // Texture display field (drag-drop target)
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

                    // Create a TextureInfo with the path - the texture will be loaded when actually needed by the renderer
                    auto textureInfo = std::make_unique<TextureInfo>(assetPathStr, nullptr);
                    material->SetTexture(type, std::move(textureInfo));
                    materialChanged = true;
                    std::cout << "[MaterialInspector] Successfully set texture path on material: " << assetPathStr << std::endl;
                }
                ImGui::EndDragDropTarget();
            }

            // Remove button (X)
            ImGui::SameLine();
            std::string removeButtonLabel = std::string(ICON_FA_XMARK) + "##remove_" + name;
            if (ImGui::Button(removeButtonLabel.c_str(), ImVec2(removeButtonWidth, 0))) {
                // Remove the texture
                material->RemoveTexture(type);
                materialChanged = true;
                std::cout << "[MaterialInspector] Removed " << name << " texture" << std::endl;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Remove texture");
            }

            // Select button (folder icon)
            ImGui::SameLine();
            std::string selectButtonLabel = std::string(ICON_FA_FOLDER_OPEN) + "##select_" + name;
            if (ImGui::Button(selectButtonLabel.c_str(), ImVec2(selectButtonWidth, 0))) {
                // Open file dialog
                #ifdef _WIN32
                    // Store current working directory to restore it later
                    std::filesystem::path originalWorkingDir = std::filesystem::current_path();

                    // Proper filter format with embedded nulls
                    char filter[] = "Image Files (*.png;*.jpg;*.jpeg;*.bmp;*.tga)\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files (*.*)\0*.*\0";
                    char filename[260] = {0};
                    std::string title = "Select " + name + " Texture";

                    OPENFILENAMEA ofn;
                    ZeroMemory(&filename, sizeof(filename));
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.lpstrFilter = filter;
                    ofn.lpstrFile = filename;
                    ofn.nMaxFile = sizeof(filename);
                    ofn.lpstrTitle = title.c_str();
                    ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

                    if (GetOpenFileNameA(&ofn)) {
                        std::string selectedPath = filename;

                        // Convert to relative path if possible (use original working dir as reference)
                        std::filesystem::path absolutePath = std::filesystem::absolute(selectedPath);
                        std::filesystem::path relativePath = std::filesystem::relative(absolutePath, originalWorkingDir);

                        std::string finalPath = relativePath.string();
                        std::replace(finalPath.begin(), finalPath.end(), '\\', '/');

                        // Create a TextureInfo with the path
                        auto textureInfo = std::make_unique<TextureInfo>(finalPath, nullptr);
                        material->SetTexture(type, std::move(textureInfo));
                        materialChanged = true;
                        std::cout << "[MaterialInspector] Selected texture: " << finalPath << std::endl;
                    }

                    // Restore the original working directory to ensure asset browser isn't affected
                    std::filesystem::current_path(originalWorkingDir);
                    std::cout << "[MaterialInspector] Restored working directory to: " << originalWorkingDir << std::endl;
                #else
                    // For non-Windows platforms, show a message
                    std::cout << "[MaterialInspector] File dialog not implemented for this platform" << std::endl;
                #endif
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Select texture file");
            }

            // Pop the unique ID
            ImGui::PopID();
        }
    }

    // Save button - always show it for material editing (outside collapsing headers)
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

void MaterialInspector::ApplyMaterialToModel(Entity entity, const GUID_128& materialGuid) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        if (!ecsManager.HasComponent<ModelRenderComponent>(entity)) {
            std::cerr << "[MaterialInspector] Entity does not have ModelRenderComponent" << std::endl;
            return;
        }

        ModelRenderComponent& modelRenderer = ecsManager.GetComponent<ModelRenderComponent>(entity);

        if (!modelRenderer.model) {
            std::cerr << "[MaterialInspector] Model is not loaded" << std::endl;
            return;
        }

        // Get the material asset metadata
        std::shared_ptr<AssetMeta> materialMeta = AssetManager::GetInstance().GetAssetMeta(materialGuid);
        if (!materialMeta) {
            std::cerr << "[MaterialInspector] Material asset not found" << std::endl;
            return;
        }

        // Load the material
        std::shared_ptr<Material> material = ResourceManager::GetInstance().GetResource<Material>(materialMeta->sourceFilePath);
        if (!material) {
            std::cerr << "[MaterialInspector] Failed to load material: " << materialMeta->sourceFilePath << std::endl;
            return;
        }

        // If material doesn't have a name, set it from the filename
        if (material->GetName().empty() || material->GetName() == "DefaultMaterial") {
            std::filesystem::path path(materialMeta->sourceFilePath);
            std::string name = path.stem().string(); // Get filename without extension
            material->SetName(name);
            std::cout << "[MaterialInspector] Set material name to: " << name << std::endl;
        }

        // Apply the material to the entire entity (like Unity)
        modelRenderer.SetMaterial(material);
        std::cout << "[MaterialInspector] Applied material '" << material->GetName() << "' to entity" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "[MaterialInspector] Error applying material to model: " << e.what() << std::endl;
    }
}

void MaterialInspector::ApplyMaterialToModelByPath(Entity entity, const std::string& materialPath) {
    try {
        ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

        if (!ecsManager.HasComponent<ModelRenderComponent>(entity)) {
            std::cerr << "[MaterialInspector] Entity does not have ModelRenderComponent" << std::endl;
            return;
        }

        ModelRenderComponent& modelRenderer = ecsManager.GetComponent<ModelRenderComponent>(entity);

        if (!modelRenderer.model) {
            std::cerr << "[MaterialInspector] Model is not loaded" << std::endl;
            return;
        }

        // Load the material directly by path
        std::shared_ptr<Material> material = ResourceManager::GetInstance().GetResource<Material>(materialPath);
        if (!material) {
            std::cerr << "[MaterialInspector] Failed to load material: " << materialPath << std::endl;
            return;
        }

        // If material doesn't have a name, set it from the filename
        if (material->GetName().empty() || material->GetName() == "DefaultMaterial") {
            std::filesystem::path path(materialPath);
            std::string name = path.stem().string(); // Get filename without extension
            material->SetName(name);
            std::cout << "[MaterialInspector] Set material name to: " << name << std::endl;
        }

        // Apply the material to the entire entity (like Unity)
        modelRenderer.SetMaterial(material);
        std::cout << "[MaterialInspector] Applied material '" << material->GetName() << "' to entity (by path)" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "[MaterialInspector] Error applying material to model by path: " << e.what() << std::endl;
    }
}