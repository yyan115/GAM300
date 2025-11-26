#include "pch.h"

#include <chrono>
#include <format>
#include "Panels/AssetInspector.hpp"
#include "imgui.h"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include <Asset Manager/AssetManager.hpp>

GUID_128 AssetInspector::currentAssetGUID = GUID_128{};
std::shared_ptr<AssetMeta> AssetInspector::currentAssetMeta = std::make_shared<AssetMeta>();
std::shared_ptr<TextureMeta> AssetInspector::currentTextureMeta = std::make_shared<TextureMeta>();
std::shared_ptr<ModelMeta> AssetInspector::currentModelMeta = std::make_shared<ModelMeta>();

void AssetInspector::DrawAssetMetaInfo(std::shared_ptr<AssetMeta> assetMeta, const std::string& assetPath, bool showLockButton, bool* isLocked, std::function<void()> lockCallback) {
	if (!assetMeta) return;

    // If we are inspecting a different asset
    if (currentAssetGUID != assetMeta->guid) {
        // Create a new temporary asset meta to store the edited changes.
        currentAssetGUID = assetMeta->guid;
        if (assetMeta->GetType() == AssetMeta::Type::Base) {
            currentAssetMeta->PopulateAssetMeta(assetMeta->guid, assetMeta->sourceFilePath, assetMeta->compiledFilePath, assetMeta->version, assetMeta->androidCompiledFilePath);
        }
        else if (assetMeta->GetType() == AssetMeta::Type::Texture) {
            std::shared_ptr<TextureMeta> textureMeta = static_pointer_cast<TextureMeta>(assetMeta);
			currentTextureMeta->PopulateAssetMeta(assetMeta->guid, assetMeta->sourceFilePath, assetMeta->compiledFilePath, assetMeta->version, assetMeta->androidCompiledFilePath);
			currentTextureMeta->PopulateTextureMeta(textureMeta->type, textureMeta->flipUVs, textureMeta->generateMipmaps);
        }
    }

	// Base asset meta info
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.24f, 0.24f, 0.24f, 1.0f));        // Neutral grey for collapsing headers
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.30f, 0.30f, 1.0f)); // Hover
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));  // Active
    bool baseInfoOpen = ImGui::CollapsingHeader("Base Asset Info", ImGuiTreeNodeFlags_AllowItemOverlap);
    ImGui::PopStyleColor(3);

    // Add lock button on the same line as base asset info
    if (showLockButton && isLocked && lockCallback) {
        ImGui::SameLine(ImGui::GetWindowWidth() - 35);
        if (ImGui::Button(*isLocked ? ICON_FA_LOCK : ICON_FA_UNLOCK, ImVec2(30, 0))) {
            lockCallback();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(*isLocked ? "Unlock Inspector" : "Lock Inspector");
        }
    }

    if (baseInfoOpen) {
	    ImGui::Text("GUID: %s", GUIDUtilities::ConvertGUID128ToString(assetMeta->guid).c_str());
	    ImGui::Text("Source Asset Path: %s", assetMeta->sourceFilePath.c_str());
	    ImGui::Text("Compiled Resource Path: %s", assetMeta->compiledFilePath.c_str());
	    ImGui::Text("Compiled Android Resource Path: %s", assetMeta->androidCompiledFilePath.c_str());
	    //ImGui::Text("Last Compile Time: %s", std::format("{:%Y-%m-%d %H:%M:%S}", assetMeta->lastCompileTime).c_str());
    }

    ImGui::Separator();

    switch (assetMeta->GetType()) {
        // TEXTURES
        case AssetMeta::Type::Texture: {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.24f, 0.24f, 0.24f, 1.0f));        // Neutral grey for collapsing headers
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.30f, 0.30f, 1.0f)); // Hover
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));  // Active
            bool texturePropertiesOpen = ImGui::CollapsingHeader("Texture Properties", ImGuiTreeNodeFlags_DefaultOpen);
            ImGui::PopStyleColor(3);

            if (texturePropertiesOpen) {
                ImGui::Indent(ImGui::GetStyle().FramePadding.x + ImGui::GetTreeNodeToLabelSpacing());
                static int currentItem = 0;
                if (ImGui::BeginTable("Texture Properties", 2, ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Col0", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableSetupColumn("Col1", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::GetStyle().FramePadding.y = 1.0f;
                    // Texture Type
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Texture Type");

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN); // fill the whole column
                    if (ImGui::BeginCombo("##Texture Type", currentTextureMeta->type.c_str())) {
                        for (int n = 0; n < TextureMeta::textureTypes.size(); n++) {
                            bool isSelected = (currentTextureMeta->type == TextureMeta::textureTypes[n]);
                            if (ImGui::Selectable(TextureMeta::textureTypes[n].c_str(), isSelected)) {
                                currentTextureMeta->type = TextureMeta::textureTypes[n];
                            }
                            if (isSelected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    // Flip UVs
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Flip UVs");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Checkbox("##Flip UVs", &currentTextureMeta->flipUVs);

                    // Generate Mipmaps
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Generate Mipmaps");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Checkbox("##Generate Mipmaps", &currentTextureMeta->generateMipmaps);

                    ImGui::EndTable();
                }
                ImGui::GetStyle().FramePadding.y = 3.0f;
                ImGui::Unindent(ImGui::GetStyle().FramePadding.x + ImGui::GetTreeNodeToLabelSpacing());
            }

            // Save button
            ImGui::Separator();
            if (ImGui::Button("Save Properties")) {
                AssetManager::GetInstance().CompileTexture(assetPath, currentTextureMeta, true);
            }
            break;
        }
        // MODELS
        case AssetMeta::Type::Model: {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.24f, 0.24f, 0.24f, 1.0f));        // Neutral grey for collapsing headers
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.30f, 0.30f, 1.0f)); // Hover
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));  // Active
            bool texturePropertiesOpen = ImGui::CollapsingHeader("Model Properties", ImGuiTreeNodeFlags_DefaultOpen);
            ImGui::PopStyleColor(3);

            if (texturePropertiesOpen) {
                ImGui::Indent(ImGui::GetStyle().FramePadding.x + ImGui::GetTreeNodeToLabelSpacing());
                static int currentItem = 0;
                if (ImGui::BeginTable("Model Properties", 2, ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Col0", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableSetupColumn("Col1", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::GetStyle().FramePadding.y = 1.0f;

                    // Optimize Meshes
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Optimize Meshes");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Checkbox("##Optimize Meshes", &currentModelMeta->optimizeMeshes);

                    ImGui::EndTable();
                }
                ImGui::GetStyle().FramePadding.y = 3.0f;
                ImGui::Unindent(ImGui::GetStyle().FramePadding.x + ImGui::GetTreeNodeToLabelSpacing());
            }

            // Save button
            ImGui::Separator();
            if (ImGui::Button("Save Properties")) {
                AssetManager::GetInstance().CompileAsset<Model>(assetPath, true, false, currentModelMeta);
            }
            break;
		}
        default:
            break;
    }
}
