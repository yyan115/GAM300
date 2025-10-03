#include "Panels/MaterialInspector.hpp"
#include "Panels/AssetBrowserPanel.hpp"
#include "EditorComponents.hpp"
#include "imgui.h"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include <Graphics/Texture.h>
#include <Graphics/Material.hpp>
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

// Helper function to draw color component with input fields and color picker
static bool DrawColorComponent(const char* label, float color[3], const char* popupId) {
    bool changed = false;

    ImGui::Text("%s", label);
    ImGui::SameLine();

    // Create unique IDs using the label
    std::string rId = std::string("##r_") + label;
    std::string gId = std::string("##g_") + label;
    std::string bId = std::string("##b_") + label;
    std::string colorId = std::string("##color_") + label;

    // R input field
    float r = color[0] * 255.0f;
    ImGui::PushItemWidth(50);
    if (ImGui::DragFloat(rId.c_str(), &r, 1.0f, 0.0f, 255.0f, "%.0f")) {
        color[0] = r / 255.0f;
        changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("G:");
    ImGui::SameLine();

    // G input field
    float g = color[1] * 255.0f;
    ImGui::PushItemWidth(50);
    if (ImGui::DragFloat(gId.c_str(), &g, 1.0f, 0.0f, 255.0f, "%.0f")) {
        color[1] = g / 255.0f;
        changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("B:");
    ImGui::SameLine();

    // B input field
    float b = color[2] * 255.0f;
    ImGui::PushItemWidth(50);
    if (ImGui::DragFloat(bId.c_str(), &b, 1.0f, 0.0f, 255.0f, "%.0f")) {
        color[2] = b / 255.0f;
        changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Color picker button
    if (ImGui::ColorButton(colorId.c_str(), ImVec4(color[0], color[1], color[2], 1), ImGuiColorEditFlags_NoTooltip, ImVec2(30, 20))) {
        ImGui::OpenPopup(popupId);
    }

    // Color picker popup
    if (ImGui::BeginPopup(popupId)) {
        if (ImGui::ColorPicker3("Color", color)) {
            changed = true;
        }
        ImGui::EndPopup();
    }

    return changed;
}

void MaterialInspector::DrawMaterialAsset(std::shared_ptr<Material> material, const std::string& assetPath, bool showLockButton, bool* isLocked, std::function<void()> lockCallback) {
    if (!material) return;

    bool materialChanged = false;

    // Colors section
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.24f, 0.24f, 0.24f, 1.0f));        // Neutral grey for collapsing headers
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.30f, 0.30f, 1.0f)); // Hover
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));  // Active
    bool colorsOpen = ImGui::CollapsingHeader("Colors", ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::PopStyleColor(3);

    // Add lock button on the same line as Colors header if requested
    if (showLockButton && isLocked && lockCallback) {
        ImGui::SameLine(ImGui::GetWindowWidth() - 35);
        if (ImGui::Button(*isLocked ? ICON_FA_LOCK : ICON_FA_UNLOCK, ImVec2(30, 0))) {
            lockCallback();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(*isLocked ? "Unlock Inspector" : "Lock Inspector");
        }
    }

    if (colorsOpen) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

        // Ambient color row
        glm::vec3 ambient = material->GetAmbient();
        float ambientColor[3] = { ambient.r, ambient.g, ambient.b };

        if (DrawColorComponent("Ambient", ambientColor, "ambient_color_picker")) {
            material->SetAmbient(glm::vec3(ambientColor[0], ambientColor[1], ambientColor[2]));
            materialChanged = true;
        }

        // Diffuse color row
        glm::vec3 diffuse = material->GetDiffuse();
        float diffuseColor[3] = { diffuse.r, diffuse.g, diffuse.b };

        if (DrawColorComponent("Diffuse", diffuseColor, "diffuse_color_picker")) {
            material->SetDiffuse(glm::vec3(diffuseColor[0], diffuseColor[1], diffuseColor[2]));
            materialChanged = true;
        }

        // Specular color row
        glm::vec3 specular = material->GetSpecular();
        float specularColor[3] = { specular.r, specular.g, specular.b };

        if (DrawColorComponent("Specular", specularColor, "specular_color_picker")) {
            material->SetSpecular(glm::vec3(specularColor[0], specularColor[1], specularColor[2]));
            materialChanged = true;
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
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.24f, 0.24f, 0.24f, 1.0f));        // Neutral grey for collapsing headers
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.30f, 0.30f, 1.0f)); // Hover
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));  // Active
    if (ImGui::CollapsingHeader("Textures")) {
        ImGui::PopStyleColor(3);
        // Texture type mappings
        static const std::vector<std::pair<Material::TextureType, std::string>> textureTypes = {
            {Material::TextureType::DIFFUSE, "Diffuse"},
            {Material::TextureType::SPECULAR, "Specular"},
            {Material::TextureType::AMBIENT_OCCLUSION, "Ambient Occlusion"},
            {Material::TextureType::HEIGHT, "Height"},
            {Material::TextureType::NORMAL, "Normal"},
            {Material::TextureType::METALLIC, "Metallic"},
            {Material::TextureType::ROUGHNESS, "Roughness"}
        };

        for (const auto& textureType : textureTypes) {
            const auto& type = textureType.first;
            const auto& name = textureType.second;
            // Push unique ID for this texture slot
            ImGui::PushID(static_cast<int>(type));

            // Get current texture path
            std::string currentPath;
            if (auto textureInfo = material->GetTextureInfo(type)) {
                currentPath = textureInfo->get().filePath;
            }

            
            ImGui::Text("%s:", name.c_str());
            ImGui::SameLine();

            // Calculate sizes for Unity-like layout
            float availableWidth = ImGui::GetContentRegionAvail().x;
            float removeButtonWidth = 35.0f;
            float selectButtonWidth = 35.0f;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float textureFieldWidth = availableWidth - removeButtonWidth - selectButtonWidth - (spacing * 2);

            // Texture display field (drag-drop target)
            std::string textureDisplay;
            if (currentPath.empty()) {
                textureDisplay = "None (Texture)";
            } else {
                // Show just the filename for cleaner display
                std::filesystem::path pathObj(currentPath);
                textureDisplay = pathObj.filename().string();
            }

            
            EditorComponents::DrawDragDropButton(textureDisplay.c_str(), textureFieldWidth);

            // Drag-drop target for textures with visual feedback
            if (EditorComponents::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_PAYLOAD")) {
                    // Extract texture path from payload
                    const char* texturePath = (const char*)payload->Data;
                    std::string pathStr(texturePath, payload->DataSize);
                    // strip trailing nulls
                    pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

                    // Use the original path as-is since it already has the correct relative path
                    std::string assetPathStr = pathStr;

                    std::cout << "[MaterialInspector] Using original path: " << assetPathStr << std::endl;

                    // Create a TextureInfo with the path - the texture will be loaded when actually needed by the renderer
                    auto textureInfo = std::make_unique<TextureInfo>(assetPathStr, nullptr);
                    material->SetTexture(type, std::move(textureInfo));
                    materialChanged = true;
                    std::cout << "[MaterialInspector] Successfully set texture path on material: " << assetPathStr << std::endl;
                }
                EditorComponents::EndDragDropTarget();
            }

            // Remove button (X) - centered
            ImGui::SameLine();
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
            std::string removeButtonLabel = std::string(ICON_FA_XMARK) + "##remove_" + name;
            if (ImGui::Button(removeButtonLabel.c_str(), ImVec2(removeButtonWidth, ImGui::GetTextLineHeightWithSpacing()))) {
                // Remove the texture
                material->RemoveTexture(type);
                materialChanged = true;
                std::cout << "[MaterialInspector] Removed " << name << " texture" << std::endl;
            }
            ImGui::PopStyleVar();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Remove texture");
            }

            // Select button (folder icon) - centered
            ImGui::SameLine();
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f)); // Center text in button
            std::string selectButtonLabel = std::string(ICON_FA_FOLDER_OPEN) + "##select_" + name;
            if (ImGui::Button(selectButtonLabel.c_str(), ImVec2(selectButtonWidth, ImGui::GetTextLineHeightWithSpacing()))) {
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
            ImGui::PopStyleVar(); // Pop ButtonTextAlign style
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Select texture file");
            }

            // Pop the unique ID
            ImGui::PopID();
        }
    } else {
        ImGui::PopStyleColor(3); // Pop header colors if not open
    }

    // Save button - always show it for material editing (outside collapsing headers)
    ImGui::Separator();
    if (ImGui::Button("Save Material")) {
        // Convert to absolute path for reliable saving
        std::filesystem::path absoluteSavePath = std::filesystem::absolute(assetPath);
        std::string absoluteSavePathStr = absoluteSavePath.string();
        
        std::cout << "[MaterialInspector] Attempting to save material to: " << absoluteSavePathStr << std::endl;
        material->CompileUpdatedAssetToResource(assetPath);
        //AssetManager::GetInstance().AddToEventQueue(AssetManager::Event::modified, assetPath);
        
        // Use the Material's CompileToResource method
        //std::string compiledPath = material->CompileToResource(absoluteSavePathStr);
        //if (!compiledPath.empty()) {
        //    std::cout << "[MaterialInspector] Material saved successfully: " << compiledPath << std::endl;

        //    // Force ResourceManager to reload the material from disk to update cache
        //    std::cout << "[MaterialInspector] Forcing ResourceManager cache reload for: " << absoluteSavePathStr << std::endl;
        //    auto reloadedMaterial = ResourceManager::GetInstance().GetResource<Material>(absoluteSavePathStr, true);
        //    if (reloadedMaterial) {
        //        std::cout << "[MaterialInspector] Successfully reloaded material in ResourceManager cache: " << reloadedMaterial->GetName() << " with " << reloadedMaterial->GetAllTextureInfo().size() << " textures" << std::endl;
        //    } else {
        //        std::cout << "[MaterialInspector] Failed to reload material in ResourceManager cache" << std::endl;
        //    }

        //    materialChanged = false; // Reset change flag after save
        //}
        //else {
        //    std::cout << "[MaterialInspector] Failed to save material to: " << absoluteSavePathStr << std::endl;
        //}
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

        // Get the material asset metadata or fallback path
        std::shared_ptr<AssetMeta> materialMeta = AssetManager::GetInstance().GetAssetMeta(materialGuid);
        std::string sourceFilePath;
        
        if (!materialMeta) {
            // Try fallback GUID lookup
            std::cout << "[MaterialInspector] AssetMeta not found, trying fallback path lookup" << std::endl;
            sourceFilePath = AssetBrowserPanel::GetFallbackGuidFilePath(materialGuid);
            if (sourceFilePath.empty()) {
                std::cerr << "[MaterialInspector] Material asset not found and no fallback path available" << std::endl;
                return;
            }
            std::cout << "[MaterialInspector] Found fallback path: " << sourceFilePath << std::endl;
        } else {
            sourceFilePath = materialMeta->sourceFilePath;
        }

        // Convert to absolute path to avoid path resolution issues
        std::filesystem::path absolutePath = std::filesystem::absolute(sourceFilePath);
        std::string absolutePathStr = absolutePath.string();
        
        // Load the material
        std::shared_ptr<Material> material = ResourceManager::GetInstance().GetResource<Material>(absolutePathStr);
        if (!material) {
            std::cerr << "[MaterialInspector] Failed to load material: " << absolutePathStr << std::endl;
            return;
        }

        // If material doesn't have a name, set it from the filename
        if (material->GetName().empty() || material->GetName() == "DefaultMaterial") {
            std::filesystem::path path(sourceFilePath);
            std::string name = path.stem().string(); // Get filename without extension
            material->SetName(name);
            std::cout << "[MaterialInspector] Set material name to: " << name << std::endl;
        }

        // Apply the material to the entire entity (like Unity)
        modelRenderer.SetMaterial(material);
        modelRenderer.materialGUID = materialGuid;
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

        // Convert to absolute path to avoid path resolution issues  
        std::filesystem::path absolutePath = std::filesystem::absolute(materialPath);
        std::string absolutePathStr = absolutePath.string();
        
        // Load the material directly by path
        std::shared_ptr<Material> material = ResourceManager::GetInstance().GetResource<Material>(absolutePathStr);
        if (!material) {
            std::cerr << "[MaterialInspector] Failed to load material: " << absolutePathStr << std::endl;
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