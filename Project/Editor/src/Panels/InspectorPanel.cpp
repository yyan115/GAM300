#include "Panels/InspectorPanel.hpp"
#include "Panels/AssetBrowserPanel.hpp"
#include "EditorComponents.hpp"
#include "imgui.h"
#include "GUIManager.hpp"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include <Graphics/Model/ModelRenderComponent.hpp>
#include <Graphics/Lights/LightComponent.hpp>
#include <Graphics/TextRendering/TextRenderComponent.hpp>
#include <Graphics/Particle/ParticleComponent.hpp>
#include <Physics/ColliderComponent.hpp>
#include <Physics/RigidBodyComponent.hpp>
#include <Physics/CollisionLayers.hpp>
#include <Graphics/Texture.h>
#include <Graphics/ShaderClass.h>
#include <Graphics/GraphicsManager.hpp>
#include <Asset Manager/AssetManager.hpp>
#include <Asset Manager/ResourceManager.hpp>
#include <Asset Manager/MetaFilesManager.hpp>
#include <Utilities/GUID.hpp>
#include <cstring>
#include <filesystem>
#include <thread>
#include <chrono>
#include <glm/glm.hpp>

// Global drag-drop state for cross-window material dragging
extern GUID_128 DraggedMaterialGuid;
extern std::string DraggedMaterialPath;

// Global drag-drop state for cross-window model dragging
extern GUID_128 DraggedModelGuid;
extern std::string DraggedModelPath;

// Global drag-drop state for cross-window audio dragging
extern GUID_128 DraggedAudioGuid;
extern std::string DraggedAudioPath;

// Global drag-drop state for cross-window font dragging
extern GUID_128 DraggedFontGuid;
extern std::string DraggedFontPath;
#include <cstddef>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <Sound/AudioComponent.hpp>
#include <RunTimeVar.hpp>

template <typename, typename = void> struct has_override_flag : std::false_type {};
template <typename T>
struct has_override_flag<T, std::void_t<decltype(std::declval<T&>().overrideFromPrefab)>> : std::true_type {};

template <typename T>
static inline void DrawOverrideToggleIfPresent(ECSManager& ecs, Entity e, const char* id_suffix = "")
{
    if constexpr (has_override_flag<T>::value) {
        auto& c = ecs.GetComponent<T>(e);
        bool b = c.overrideFromPrefab;
        std::string label = std::string("Override From Prefab##") + typeid(T).name() + id_suffix;
        if (ImGui::Checkbox(label.c_str(), &b)) {
            c.overrideFromPrefab = b;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Instance)");
    }
}


InspectorPanel::InspectorPanel()
	: EditorPanel("Inspector", true) {
}

void InspectorPanel::OnImGuiRender() {
	
	ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorComponents::PANEL_BG_INSPECTOR);
	ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorComponents::PANEL_BG_INSPECTOR);

	if (ImGui::Begin(name.c_str(), &isOpen)) {
		// Check for selected asset first (higher priority)
		GUID_128 selectedAsset = GUIManager::GetSelectedAsset();

		// Determine what to display based on lock state
		Entity displayEntity = static_cast<Entity>(-1);
		GUID_128 displayAsset = { 0, 0 };

		if (inspectorLocked) {
			// Show locked content
			if (lockedEntity != static_cast<Entity>(-1)) {
				displayEntity = lockedEntity;
			} else if (lockedAsset.high != 0 || lockedAsset.low != 0) {
				displayAsset = lockedAsset;
			}
		} else {
			// Show current selection
			if (selectedAsset.high != 0 || selectedAsset.low != 0) {
				displayAsset = selectedAsset;
			} else {
				displayEntity = GUIManager::GetSelectedEntity();
			}
		}

		// Validate locked content
		if (inspectorLocked) {
			if (lockedEntity != static_cast<Entity>(-1)) {
				try {
					ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
					auto activeEntities = ecsManager.GetActiveEntities();
					bool entityExists = std::find(activeEntities.begin(), activeEntities.end(), lockedEntity) != activeEntities.end();
					if (!entityExists) {
						// Locked entity no longer exists, unlock
						inspectorLocked = false;
						lockedEntity = static_cast<Entity>(-1);
						lockedAsset = {0, 0};
						displayEntity = GUIManager::GetSelectedEntity();
						displayAsset = GUIManager::GetSelectedAsset();
					}
				} catch (...) {
					// If there's any error, unlock
					inspectorLocked = false;
					lockedEntity = static_cast<Entity>(-1);
					lockedAsset = {0, 0};
					displayEntity = GUIManager::GetSelectedEntity();
					displayAsset = GUIManager::GetSelectedAsset();
				}
			}
			// Note: Asset validation could be added here if needed
		}

		// Display content
		if (displayAsset.high != 0 || displayAsset.low != 0) {
			DrawSelectedAsset(displayAsset);
		} else {
			// Clear cached material when no asset is selected
			if (cachedMaterial) {
				std::cout << "[Inspector] Clearing cached material" << std::endl;
				cachedMaterial.reset();
				cachedMaterialGuid = {0, 0};
				cachedMaterialPath.clear();
			}

			if (displayEntity == static_cast<Entity>(-1)) {
				ImGui::Text("No object selected");

				// Lock button on the same line
				ImGui::SameLine(ImGui::GetWindowWidth() - 35);
				if (ImGui::Button(inspectorLocked ? ICON_FA_LOCK : ICON_FA_UNLOCK, ImVec2(30, 0))) {
					inspectorLocked = !inspectorLocked;
					if (inspectorLocked) {
						// Lock to current content (entity or asset)
						if (selectedAsset.high != 0 || selectedAsset.low != 0) {
							lockedAsset = selectedAsset;
							lockedEntity = static_cast<Entity>(-1);
						} else {
							lockedEntity = GUIManager::GetSelectedEntity();
							lockedAsset = {0, 0};
						}
					} else {
						// Unlock
						lockedEntity = static_cast<Entity>(-1);
						lockedAsset = {0, 0};
					}
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(inspectorLocked ? "Unlock Inspector" : "Lock Inspector");
				}

				ImGui::Text("Select an object in the Scene Hierarchy or an asset in the Asset Browser to view its properties");
				if (inspectorLocked) {
					ImGui::Text("Inspector is locked but no valid content is selected.");
				}
			} else {
				try {
					// Get the active ECS manager
					ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

					ImGui::Text("Entity ID: %u", displayEntity);

					// Lock button on the same line
					ImGui::SameLine(ImGui::GetWindowWidth() - 35);
					if (ImGui::Button(inspectorLocked ? ICON_FA_LOCK : ICON_FA_UNLOCK, ImVec2(30, 0))) {
						inspectorLocked = !inspectorLocked;
						if (inspectorLocked) {
							// Lock to current content (entity or asset)
							if (selectedAsset.high != 0 || selectedAsset.low != 0) {
								lockedAsset = selectedAsset;
								lockedEntity = static_cast<Entity>(-1);
							} else {
								lockedEntity = GUIManager::GetSelectedEntity();
								lockedAsset = {0, 0};
							}
						} else {
							// Unlock
							lockedEntity = static_cast<Entity>(-1);
							lockedAsset = {0, 0};
						}
					}
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip(inspectorLocked ? "Unlock Inspector" : "Lock Inspector");
					}
					ImGui::Separator();

                    // Draw NameComponent if it exists
                    if (ecsManager.HasComponent<NameComponent>(displayEntity)) {
                        auto& nc = ecsManager.GetComponent<NameComponent>(displayEntity);

                        DrawOverrideToggleIfPresent<NameComponent>(ecsManager, displayEntity, "Name");

                        bool followPrefab = false;
                        if constexpr (has_override_flag<NameComponent>::value) followPrefab = !nc.overrideFromPrefab;

                        ImGui::BeginDisabled(followPrefab);
                        {
                            DrawNameComponent(displayEntity);
                            ImGui::Spacing();

                            // Tag dropdown
                            ImGui::Text("Tag");
                            ImGui::SameLine(80);
                            ImGui::SetNextItemWidth(-1);
                            const char* tags[] = { "Untagged", "Player", "Enemy", "UI", "Camera", "Light" };
                            static int currentTag = 0;
                            ImGui::Combo("##Tag", &currentTag, tags, IM_ARRAYSIZE(tags));

                            // Layer dropdown
                            ImGui::Text("Layer");
                            ImGui::SameLine(80);
                            ImGui::SetNextItemWidth(-1);
                            const char* layers[] = { "Default", "UI", "Water", "Ignore Raycast", "PostProcessing" };
                            static int currentLayer = 0;
                            ImGui::Combo("##Layer", &currentLayer, layers, IM_ARRAYSIZE(layers));
                        }
                        ImGui::EndDisabled();
                        ImGui::Separator();
                    }

                    // Draw Transform component if it exists
                    if (ecsManager.HasComponent<Transform>(displayEntity)) {
                        if (DrawComponentHeaderWithRemoval("Transform", displayEntity, "TransformComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
                            auto& t = ecsManager.GetComponent<Transform>(displayEntity);

                            DrawOverrideToggleIfPresent<Transform>(ecsManager, displayEntity, "Transform");

                            bool followPrefab = false;
                            if constexpr (has_override_flag<Transform>::value) followPrefab = !t.overrideFromPrefab;

                            ImGui::BeginDisabled(followPrefab);
                            {
                                DrawTransformComponent(displayEntity);
                            }
                            ImGui::EndDisabled();
                        }
                    }

					// Draw ModelRenderComponent if it exists
					if (ecsManager.HasComponent<ModelRenderComponent>(displayEntity)) {
						if (DrawComponentHeaderWithRemoval("Model Renderer", displayEntity, "ModelRenderComponent")) {
							auto& m = ecsManager.GetComponent<ModelRenderComponent>(displayEntity);

							DrawOverrideToggleIfPresent<ModelRenderComponent>(ecsManager, displayEntity, "ModelRender");

							bool followPrefab = false;
							if constexpr (has_override_flag<ModelRenderComponent>::value) followPrefab = !m.overrideFromPrefab;

							ImGui::BeginDisabled(followPrefab);
							{
								DrawModelRenderComponent(displayEntity);
							}
							ImGui::EndDisabled();
						}
					}

					// Draw SpriteRenderComponent if it exists
					if (ecsManager.HasComponent<SpriteRenderComponent>(displayEntity)) {
						if (DrawComponentHeaderWithRemoval("Sprite Renderer", displayEntity, "SpriteRenderComponent")) {
							DrawSpriteRenderComponent(displayEntity);
						}
					}

					// Draw TextRenderComponent if it exists
					if (ecsManager.HasComponent<TextRenderComponent>(displayEntity)) {
						if (DrawComponentHeaderWithRemoval("Text Renderer", displayEntity, "TextRenderComponent")) {
							DrawTextRenderComponent(displayEntity);
						}
					}

					// Draw ParticleComponent if it exists
					if (ecsManager.HasComponent<ParticleComponent>(displayEntity)) {
						if (DrawComponentHeaderWithRemoval("Particle System", displayEntity, "ParticleComponent")) {
							DrawParticleComponent(displayEntity);
						}
					}

					// Draw AudioComponent if present
					if (ecsManager.HasComponent<AudioComponent>(displayEntity)) {
						if (DrawComponentHeaderWithRemoval("Audio", displayEntity, "AudioComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
							DrawAudioComponent(displayEntity);
						}
					}

					// Draw Light Components if present
					DrawLightComponents(displayEntity);

					// Draw Collider Component if present
					if (ecsManager.HasComponent<ColliderComponent>(displayEntity)) {
						if (DrawComponentHeaderWithRemoval("Collider", displayEntity, "ColliderComponent")) {
							DrawColliderComponent(displayEntity);
						}
					}

					// Draw RigidBody Component if present
					if (ecsManager.HasComponent<RigidBodyComponent>(displayEntity)) {
						if (DrawComponentHeaderWithRemoval("RigidBody", displayEntity, "RigidBodyComponent")) {
							DrawRigidBodyComponent(displayEntity);
						}
					}

					// Add Component button
					ImGui::Separator();
					DrawAddComponentButton(displayEntity);

				} catch (const std::exception& e) {
					ImGui::Text("Error accessing entity: %s", e.what());
				}
			}
		}
	}

	// Process any pending component removals after ImGui rendering is complete
	ProcessPendingComponentRemovals();

	ImGui::End();

	ImGui::PopStyleColor(2);  // Pop WindowBg and ChildBg colors
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

		// Model drag-drop slot
		ImGui::Text("Model:");
		ImGui::SameLine();

		// Create a model slot button that shows current model
		std::string modelButtonText;
		if (modelRenderer.model) {
			// Show the model name if available
			modelButtonText = modelRenderer.model->modelName.empty() ? "Unnamed Model" : modelRenderer.model->modelName;
		} else {
			modelButtonText = "None (Model)";
		}

		
		float buttonWidth = ImGui::GetContentRegionAvail().x;
		EditorComponents::DrawDragDropButton(modelButtonText.c_str(), buttonWidth);

		// The button is now the drag-drop target for models with visual feedback
		if (EditorComponents::BeginDragDropTarget()) {
			ImGui::SetTooltip("Drop .obj, .fbx, .dae, or .3ds model here");
			// Accept the cross-window drag payload
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MODEL_DRAG")) {
				// Apply the model to the ModelRenderComponent
				ApplyModelToRenderer(entity, DraggedModelGuid, DraggedModelPath);

				// Clear the drag state
				DraggedModelGuid = {0, 0};
				DraggedModelPath.clear();
			}
			EditorComponents::EndDragDropTarget();
		}

		if (modelRenderer.shader) {
			ImGui::Text("Shader: Loaded");
		} else {
			ImGui::Text("Shader: None");
		}

		ImGui::Separator();

		// Material drag-drop slot
		ImGui::Text("Material:");
		ImGui::SameLine();

		// Create a material slot button that shows current material
		std::shared_ptr<Material> currentMaterial = modelRenderer.material;
		std::string buttonText;
		if (currentMaterial) {
			buttonText = currentMaterial->GetName();
		} else if (modelRenderer.model && !modelRenderer.model->meshes.empty()) {
			// Show default material from first mesh
			auto& defaultMaterial = modelRenderer.model->meshes[0].material;
			if (defaultMaterial) {
				buttonText = defaultMaterial->GetName() + " (default)";
			} else {
				buttonText = "None (Material)";
			}
		} else {
			buttonText = "None (Material)";
		}

		
		float materialButtonWidth = ImGui::GetContentRegionAvail().x;
		EditorComponents::DrawDragDropButton(buttonText.c_str(), materialButtonWidth);

		// The button is now the drag-drop target with visual feedback
		if (EditorComponents::BeginDragDropTarget()) {
			ImGui::SetTooltip("Drop material here to apply to model");
			// Accept the cross-window drag payload
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_DRAG")) {
				// Try GUID first, then fallback to path
				if (DraggedMaterialGuid.high != 0 || DraggedMaterialGuid.low != 0) {
					MaterialInspector::ApplyMaterialToModel(entity, DraggedMaterialGuid);
				} else {
					MaterialInspector::ApplyMaterialToModelByPath(entity, DraggedMaterialPath);
				}

				// Clear the drag state
				DraggedMaterialGuid = {0, 0};
				DraggedMaterialPath.clear();
			}
			EditorComponents::EndDragDropTarget();
		}

		ImGui::PopID();
	} catch (const std::exception& e) {
		ImGui::Text("Error accessing ModelRenderComponent: %s", e.what());
	}
}

void InspectorPanel::DrawSpriteRenderComponent(Entity entity) {
	try {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
		SpriteRenderComponent& sprite = ecsManager.GetComponent<SpriteRenderComponent>(entity);

		ImGui::PushID("SpriteRenderComponent");

		// Texture drag-drop slot
		ImGui::Text("Texture:");
		ImGui::SameLine();

		// Create texture slot button showing current texture
		std::string textureButtonText;
		if (sprite.texture) {
			// Extract filename from texture path if available
			if (!sprite.texturePath.empty()) {
				std::filesystem::path texPath(sprite.texturePath);
				textureButtonText = texPath.filename().string();
			} else {
				textureButtonText = "Loaded Texture";
			}
		} else {
			textureButtonText = "None (Texture)";
		}

		
		float textureButtonWidth = ImGui::GetContentRegionAvail().x;
		EditorComponents::DrawDragDropButton(textureButtonText.c_str(), textureButtonWidth);

		// Texture drag-drop target with visual feedback
		if (EditorComponents::BeginDragDropTarget()) {
			ImGui::SetTooltip("Drop .png, .jpg, .jpeg, .bmp, or .tga texture here");
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_PAYLOAD")) {
				// Payload contains the file path
				const char* texturePath = (const char*)payload->Data;

				// Load texture using ResourceManager
				sprite.textureGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(texturePath);
				sprite.texture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(sprite.textureGUID, texturePath);

				if (sprite.texture) {
					sprite.texturePath = texturePath; // Store the path for display
					std::cout << "[Inspector] Loaded texture: " << texturePath << std::endl;
				} else {
					std::cerr << "[Inspector] Failed to load texture: " << texturePath << std::endl;
				}
			}
			EditorComponents::EndDragDropTarget();
		}

		// Right-click to clear texture
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && sprite.texture) {
			ImGui::OpenPopup("ClearTexture");
		}

		if (ImGui::BeginPopup("ClearTexture")) {
			if (ImGui::MenuItem("Clear Texture")) {
				sprite.texture = nullptr;
				sprite.texturePath.clear();
			}
			ImGui::EndPopup();
		}

		ImGui::Separator();

		// Sprite type toggle (inverted - checkbox shows "Is 2D")
		bool is2D = !sprite.is3D;
		if (ImGui::Checkbox("Is 2D", &is2D)) {
			// Mode changed
			if (is2D && sprite.is3D) {
				// Switching from 3D to 2D
				// Sync sprite.position with Transform before saving (in case user moved via Transform)
				if (ecsManager.HasComponent<Transform>(entity)) {
					Transform& transform = ecsManager.GetComponent<Transform>(entity);
					sprite.position = Vector3D(transform.localPosition.x, transform.localPosition.y, transform.localPosition.z);
				}
				// Save current 3D position
				sprite.saved3DPosition = sprite.position;

				// Set sprite to center of viewport (using screen coordinates)
				// For 2D mode, position is in pixels from top-left
				sprite.position = Vector3D(RunTimeVar::window.width / 2.0f, RunTimeVar::window.height / 2.0f, 0.0f);
				sprite.is3D = false;
			}
			else if (!is2D && !sprite.is3D) {
				// Switching from 2D to 3D
				// Restore saved 3D position to both Transform and sprite.position
				if (ecsManager.HasComponent<Transform>(entity)) {
					ecsManager.transformSystem->SetLocalPosition(entity, Vector3D(sprite.saved3DPosition.x, sprite.saved3DPosition.y, sprite.saved3DPosition.z));
				}
				sprite.position = sprite.saved3DPosition;
				sprite.is3D = true;
				sprite.enableBillboard = true;
			}
		}

		// Follow Camera toggle (billboard effect - only for 3D sprites)
		if (sprite.is3D) {
			ImGui::Checkbox("Follow Camera", &sprite.enableBillboard);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Makes the sprite always face the camera (billboard effect)");
			}
		}

		ImGui::Checkbox("Visible", &sprite.isVisible);

		ImGui::PopID();
	} catch (const std::exception& e) {
		ImGui::Text("Error accessing SpriteRenderComponent: %s", e.what());
	}
}

void InspectorPanel::DrawTextRenderComponent(Entity entity) {
	try {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
		TextRenderComponent& textComp = ecsManager.GetComponent<TextRenderComponent>(entity);

		ImGui::PushID("TextRenderComponent");

		// Text input
		char textBuffer[256] = {};
		size_t copyLen = (std::min)(textComp.text.size(), sizeof(textBuffer) - 1);
		std::memcpy(textBuffer, textComp.text.c_str(), copyLen);
		textBuffer[copyLen] = '\0';

		ImGui::Text("Text");
		if (ImGui::InputText("##Text", textBuffer, sizeof(textBuffer))) {
			textComp.text = std::string(textBuffer);
		}

		ImGui::Separator();

		// Font drag-drop slot
		ImGui::Text("Font:");
		ImGui::SameLine();

		std::string fontButtonText = "None (Font)";
		if (textComp.font) {
			// Try to get font name from asset manager
			std::shared_ptr<AssetMeta> fontMeta = AssetManager::GetInstance().GetAssetMeta(textComp.fontGUID);
			if (fontMeta) {
				std::filesystem::path fontPath(fontMeta->sourceFilePath);
				fontButtonText = fontPath.filename().string();
			} else {
				fontButtonText = "Loaded Font";
			}
		}

		float buttonWidth = ImGui::GetContentRegionAvail().x;
		EditorComponents::DrawDragDropButton(fontButtonText.c_str(), buttonWidth);

		// Font drag-drop target
		if (EditorComponents::BeginDragDropTarget()) {
			ImGui::SetTooltip("Drop .ttf or .otf font here");
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FONT_PAYLOAD")) {
				const char* fontPath = (const char*)payload->Data;
				// Load font using ResourceManager
				textComp.font = ResourceManager::GetInstance().GetFontResource(fontPath);
				if (textComp.font) {
					textComp.fontGUID = MetaFilesManager::GetGUID128FromAssetFile(fontPath);
					std::cout << "[Inspector] Loaded font: " << fontPath << std::endl;
				} else {
					std::cerr << "[Inspector] Failed to load font: " << fontPath << std::endl;
				}
			}
			EditorComponents::EndDragDropTarget();
		}

		ImGui::Separator();

		// Font size
		int fontSize = static_cast<int>(textComp.fontSize);
		ImGui::Text("Font Size");
		ImGui::SameLine();
		if (ImGui::DragInt("##FontSize", &fontSize, 1.0f, 1, 500)) {
			textComp.fontSize = static_cast<unsigned int>(fontSize);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Font size affects glyph quality. Use Transform Scale to resize text.");
		}

		// Color picker
		float colorArray[3] = { textComp.color.x, textComp.color.y, textComp.color.z };
		ImGui::Text("Color");
		ImGui::SameLine();
		if (ImGui::ColorEdit3("##TextColor", colorArray)) {
			textComp.color = Vector3D(colorArray[0], colorArray[1], colorArray[2]);
		}

		ImGui::Separator();

		// Position (uses Transform component)
		if (!textComp.is3D && ecsManager.HasComponent<Transform>(entity)) {
			Transform& transform = ecsManager.GetComponent<Transform>(entity);
			float pos[3] = { transform.localPosition.x, transform.localPosition.y, transform.localPosition.z };
			ImGui::Text("Position (Screen)");
			if (ImGui::DragFloat3("##TextPosition", pos, 1.0f)) {
				ecsManager.transformSystem->SetLocalPosition(entity, Vector3D(pos[0], pos[1], pos[2]));
			}
		}

		// Alignment (swapped to match actual rendering behavior)
		const char* alignmentItems[] = { "Right", "Center", "Left" };
		int currentAlignment = static_cast<int>(textComp.alignment);
		ImGui::Text("Alignment");
		ImGui::SameLine();
		if (ImGui::Combo("##TextAlignment", &currentAlignment, alignmentItems, 3)) {
			textComp.alignment = static_cast<TextRenderComponent::Alignment>(currentAlignment);
			textComp.alignmentInt = currentAlignment;
		}

		// Is 3D toggle with position handling
		bool is3D = textComp.is3D;
		if (ImGui::Checkbox("Is 3D", &is3D)) {
			if (is3D && !textComp.is3D) {
				// Switching from 2D to 3D
				// Reset 2D position to origin and switch to Transform component positioning
				textComp.position = Vector3D(0.0f, 0.0f, 0.0f);
				textComp.is3D = true;
				// Position entity in front of camera by default
				if (ecsManager.HasComponent<Transform>(entity)) {
					ecsManager.transformSystem->SetLocalPosition(entity, Vector3D(0.0f, 0.0f, -5.0f));
					ecsManager.transformSystem->SetLocalScale(entity, Vector3D(1.0f, 1.0f, 1.0f));
				}
			}
			else if (!is3D && textComp.is3D) {
				// Switching from 3D to 2D
				// Set 2D screen space position to origin
				textComp.position = Vector3D(0.0f, 0.0f, 0.0f);
				textComp.is3D = false;
			}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("3D mode uses Transform component scale/position, 2D mode uses screen space position");
		}

		ImGui::Checkbox("Visible", &textComp.isVisible);

		ImGui::PopID();
	} catch (const std::exception& e) {
		ImGui::Text("Error accessing TextRenderComponent: %s", e.what());
	}
}

void InspectorPanel::DrawParticleComponent(Entity entity) {
	try {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
		ParticleComponent& particle = ecsManager.GetComponent<ParticleComponent>(entity);

		ImGui::PushID("ParticleComponent");

		// Play/Pause/Stop buttons for editor preview (shows in Scene panel)
		float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

		// Play button
		ImGui::PushStyleColor(ImGuiCol_Button, particle.isPlayingInEditor && !particle.isPausedInEditor ? ImVec4(0.2f, 0.6f, 0.2f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));

		if (ImGui::Button(ICON_FA_PLAY " Play", ImVec2(buttonWidth, 0))) {
			particle.isPlayingInEditor = true;
			particle.isPausedInEditor = false;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Play particle preview in Scene panel");
		}

		ImGui::PopStyleColor(3);
		ImGui::SameLine();

		// Pause button
		ImGui::PushStyleColor(ImGuiCol_Button, particle.isPausedInEditor ? ImVec4(0.6f, 0.5f, 0.2f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.6f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.4f, 0.1f, 1.0f));

		if (ImGui::Button(ICON_FA_PAUSE " Pause", ImVec2(buttonWidth, 0))) {
			if (particle.isPlayingInEditor) {
				particle.isPausedInEditor = !particle.isPausedInEditor;
			}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Pause particle preview (keeps existing particles)");
		}

		ImGui::PopStyleColor(3);

		// Stop button (full width)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));

		if (ImGui::Button(ICON_FA_STOP " Stop", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
			particle.isPlayingInEditor = false;
			particle.isPausedInEditor = false;
			particle.particles.clear();  // Clear all particles on stop
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Stop and clear all particles");
		}

		ImGui::PopStyleColor(3);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// Texture drag-drop slot
		ImGui::Text("Texture:");
		ImGui::SameLine();

		// Create texture slot button showing current texture
		std::string textureButtonText;
		if (particle.particleTexture) {
			// Extract filename from texture path if available
			if (!particle.texturePath.empty()) {
				std::filesystem::path texPath(particle.texturePath);
				textureButtonText = texPath.filename().string();
			} else {
				textureButtonText = "Loaded Texture";
			}
		} else {
			textureButtonText = "None (Texture)";
		}

		
		float textureButtonWidth = ImGui::GetContentRegionAvail().x;
		EditorComponents::DrawDragDropButton(textureButtonText.c_str(), textureButtonWidth);

		// Texture drag-drop target with visual feedback
		if (EditorComponents::BeginDragDropTarget()) {
			ImGui::SetTooltip("Drop .png, .jpg, .jpeg, .bmp, or .tga texture here");
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_PAYLOAD")) {
				// Payload contains the file path
				const char* texturePath = (const char*)payload->Data;

				// Load texture using ResourceManager
				particle.particleTexture = ResourceManager::GetInstance().GetResource<Texture>(texturePath);

				if (particle.particleTexture) {
					particle.texturePath = texturePath;  // Store the path for display
					particle.textureGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(texturePath);
					std::cout << "[Inspector] Loaded particle texture: " << texturePath << std::endl;
				} else {
					std::cerr << "[Inspector] Failed to load particle texture: " << texturePath << std::endl;
				}
			}
			EditorComponents::EndDragDropTarget();
		}

		// Right-click to clear texture
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && particle.particleTexture) {
			ImGui::OpenPopup("ClearParticleTexture");
		}

		if (ImGui::BeginPopup("ClearParticleTexture")) {
			if (ImGui::MenuItem("Clear Texture")) {
				particle.particleTexture = nullptr;
				particle.texturePath.clear();
			}
			ImGui::EndPopup();
		}

		ImGui::Separator();

		// Emitter Properties Section
		ImGui::Text("Emitter Properties");
		ImGui::Separator();

		// Emission Rate
		ImGui::Text("Emission Rate");
		ImGui::DragFloat("##EmissionRate", &particle.emissionRate, 0.1f, 0.0f, 1000.0f, "%.1f particles/sec");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Number of particles emitted per second");
		}

		// Max Particles
		ImGui::Text("Max Particles");
		ImGui::DragInt("##MaxParticles", &particle.maxParticles, 1, 1, 100000);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Maximum number of particles that can exist at once");
		}

		// Is Emitting
		ImGui::Checkbox("Is Emitting", &particle.isEmitting);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Whether the particle system is actively emitting new particles");
		}

		// Active Particle Count (Read-only)
		ImGui::Text("Active Particles: %zu / %d", particle.particles.size(), particle.maxParticles);

		ImGui::Spacing();
		ImGui::Text("Particle Properties");
		ImGui::Separator();

		// Particle Lifetime
		ImGui::Text("Lifetime");
		ImGui::DragFloat("##Lifetime", &particle.particleLifetime, 0.01f, 0.01f, 100.0f, "%.2f seconds");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("How long each particle lives before fading out");
		}

		// Start Size
		ImGui::Text("Start Size");
		ImGui::DragFloat("##StartSize", &particle.startSize, 0.01f, 0.0f, 100.0f, "%.2f");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Initial size of particles when spawned");
		}

		// End Size
		ImGui::Text("End Size");
		ImGui::DragFloat("##EndSize", &particle.endSize, 0.01f, 0.0f, 100.0f, "%.2f");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Final size of particles before they die (interpolated over lifetime)");
		}

		// Start Color
		ImGui::Text("Start Color");
		glm::vec4 startColor{ particle.startColor.x, particle.startColor.y, particle.startColor.z, particle.startColorAlpha };
		ImGui::ColorEdit4("##StartColor", &startColor.r);
		particle.startColor.x = startColor.x; particle.startColor.y = startColor.y; particle.startColor.z = startColor.z; particle.startColorAlpha = startColor.a;
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Initial color and alpha of particles when spawned");
		}

		// End Color
		ImGui::Text("End Color");
		glm::vec4 endColor{ particle.endColor.x, particle.endColor.y, particle.endColor.z, particle.endColorAlpha };
		ImGui::ColorEdit4("##EndColor", &endColor.r);
		particle.endColor.x = endColor.x; particle.endColor.y = endColor.y; particle.endColor.z = endColor.z; particle.endColorAlpha = endColor.a;
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Final color and alpha of particles before they die (interpolated over lifetime)");
		}

		ImGui::Spacing();
		ImGui::Text("Physics");
		ImGui::Separator();

		// Gravity
		ImGui::Text("Gravity");
		float gravity[3] = { particle.gravity.x, particle.gravity.y, particle.gravity.z };
		if (ImGui::DragFloat3("##Gravity", gravity, 0.1f, -50.0f, 50.0f, "%.2f")) {
			particle.gravity = Vector3D(gravity[0], gravity[1], gravity[2]);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Gravity force applied to particles (typically negative Y for downward)");
		}

		// Initial Velocity
		ImGui::Text("Initial Velocity");
		float velocity[3] = { particle.initialVelocity.x, particle.initialVelocity.y, particle.initialVelocity.z };
		if (ImGui::DragFloat3("##InitialVelocity", velocity, 0.1f, -100.0f, 100.0f, "%.2f")) {
			particle.initialVelocity = Vector3D(velocity[0], velocity[1], velocity[2]);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Base velocity direction for newly spawned particles");
		}

		// Velocity Randomness
		ImGui::Text("Velocity Randomness");
		ImGui::DragFloat("##VelocityRandomness", &particle.velocityRandomness, 0.01f, 0.0f, 100.0f, "%.2f");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Amount of random variation applied to particle velocities");
		}

		ImGui::PopID();
	} catch (const std::exception& e) {
		ImGui::Text("Error accessing ParticleComponent: %s", e.what());
	}
}

void InspectorPanel::DrawAudioComponent(Entity entity) {
	try {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
		AudioComponent& audio = ecsManager.GetComponent<AudioComponent>(entity);

		ImGui::PushID("AudioComponent");

		// Audio Clip drag-drop slot
		ImGui::Text("Clip:");
		ImGui::SameLine();

		// Create audio slot button showing current clip
		std::string audioButtonText;
		if (!audio.Clip.empty()) {
			std::filesystem::path clipPath(audio.Clip);
			audioButtonText = clipPath.filename().string();
		} else {
			audioButtonText = "None (Audio Clip)";
		}

		// Create the audio slot button
		float buttonWidth = ImGui::GetContentRegionAvail().x;
		ImGui::Button(audioButtonText.c_str(), ImVec2(buttonWidth, 30.0f));

		// Audio clip drag-drop target
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AUDIO_DRAG")) {
				// Use global drag data set by AssetBrowserPanel
				audio.SetClip(DraggedAudioPath);
				audio.audioGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(DraggedAudioPath);
			}
			ImGui::EndDragDropTarget();
		}

		// Right-click to clear
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !audio.Clip.empty()) {
			ImGui::OpenPopup("ClearAudioClip");
		}

		if (ImGui::BeginPopup("ClearAudioClip")) {
			if (ImGui::MenuItem("Clear Clip")) {
				audio.SetClip("");
			}
			ImGui::EndPopup();
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

		// Play on Awake (Unity naming)
		ImGui::Checkbox("Play On Awake", &audio.PlayOnAwake);

		// Spatialize
		if (ImGui::Checkbox("Spatialize", &audio.Spatialize)) {
			// toggled
		}

		// Spatial Blend (Unity naming)
		float blend = audio.SpatialBlend;
		if (ImGui::SliderFloat("Spatial Blend", &blend, 0.0f, 1.0f)) {
			audio.SetSpatialBlend(blend);
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
	} catch (const std::exception& e) {
		ImGui::Text("Error accessing AudioComponent: %s", e.what());
	}
}

void InspectorPanel::ApplyMaterialToModel(Entity entity, const GUID_128& materialGuid) {
	try {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

		if (!ecsManager.HasComponent<ModelRenderComponent>(entity)) {
			std::cerr << "[InspectorPanel] Entity does not have ModelRenderComponent" << std::endl;
			return;
		}

		ModelRenderComponent& modelRenderer = ecsManager.GetComponent<ModelRenderComponent>(entity);

		if (!modelRenderer.model) {
			std::cerr << "[InspectorPanel] Model is not loaded" << std::endl;
			return;
		}

		// Get the material asset metadata
		std::shared_ptr<AssetMeta> materialMeta = AssetManager::GetInstance().GetAssetMeta(materialGuid);
		if (!materialMeta) {
			std::cerr << "[InspectorPanel] Material asset not found" << std::endl;
			return;
		}

		// Load the material
		std::shared_ptr<Material> material = ResourceManager::GetInstance().GetResource<Material>(materialMeta->sourceFilePath);
		if (!material) {
			std::cerr << "[InspectorPanel] Failed to load material: " << materialMeta->sourceFilePath << std::endl;
			return;
		}

		// If material doesn't have a name, set it from the filename
		if (material->GetName().empty() || material->GetName() == "DefaultMaterial") {
			std::filesystem::path path(materialMeta->sourceFilePath);
			std::string name = path.stem().string(); // Get filename without extension
			material->SetName(name);
			std::cout << "[InspectorPanel] Set material name to: " << name << std::endl;
		}

		// Apply the material to the entire entity (like Unity)
		modelRenderer.SetMaterial(material);
		std::cout << "[InspectorPanel] Applied material '" << material->GetName() << "' to entity" << std::endl;

	} catch (const std::exception& e) {
		std::cerr << "[InspectorPanel] Error applying material to model: " << e.what() << std::endl;
	}
}

void InspectorPanel::ApplyMaterialToModelByPath(Entity entity, const std::string& materialPath) {
	try {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

		if (!ecsManager.HasComponent<ModelRenderComponent>(entity)) {
			std::cerr << "[InspectorPanel] Entity does not have ModelRenderComponent" << std::endl;
			return;
		}

		ModelRenderComponent& modelRenderer = ecsManager.GetComponent<ModelRenderComponent>(entity);

		if (!modelRenderer.model) {
			std::cerr << "[InspectorPanel] Model is not loaded" << std::endl;
			return;
		}

		// Load the material directly by path
		std::shared_ptr<Material> material = ResourceManager::GetInstance().GetResource<Material>(materialPath);
		if (!material) {
			std::cerr << "[InspectorPanel] Failed to load material: " << materialPath << std::endl;
			return;
		}

		// If material doesn't have a name, set it from the filename
		if (material->GetName().empty() || material->GetName() == "DefaultMaterial") {
			std::filesystem::path path(materialPath);
			std::string name = path.stem().string(); // Get filename without extension
			material->SetName(name);
			std::cout << "[InspectorPanel] Set material name to: " << name << std::endl;
		}

		// Apply the material to the entire entity (like Unity)
		modelRenderer.SetMaterial(material);
		std::cout << "[InspectorPanel] Applied material '" << material->GetName() << "' to entity (by path)" << std::endl;

	} catch (const std::exception& e) {
		std::cerr << "[InspectorPanel] Error applying material to model by path: " << e.what() << std::endl;
	}
}

void InspectorPanel::DrawSelectedAsset(const GUID_128& assetGuid) {
	try {
		// Get asset metadata from AssetManager
		std::shared_ptr<AssetMeta> assetMeta = AssetManager::GetInstance().GetAssetMeta(assetGuid);
		std::string sourceFilePath;
		
		if (!assetMeta) {
			// Check if this is a fallback GUID - try to find the file path from the asset browser
			std::cout << "[Inspector] AssetMeta not found for GUID, trying fallback path lookup" << std::endl;
			
			sourceFilePath = AssetBrowserPanel::GetFallbackGuidFilePath(assetGuid);
			if (sourceFilePath.empty()) {
				ImGui::Text("Asset not found - no metadata or fallback path available");

				// Lock button on the same line
				GUID_128 selectedAsset = GUIManager::GetSelectedAsset();
				ImGui::SameLine(ImGui::GetWindowWidth() - 35);
				if (ImGui::Button(inspectorLocked ? ICON_FA_LOCK : ICON_FA_UNLOCK, ImVec2(30, 0))) {
					inspectorLocked = !inspectorLocked;
					if (inspectorLocked) {
						// Lock to current content (entity or asset)
						if (selectedAsset.high != 0 || selectedAsset.low != 0) {
							lockedAsset = selectedAsset;
							lockedEntity = static_cast<Entity>(-1);
						} else {
							lockedEntity = GUIManager::GetSelectedEntity();
							lockedAsset = {0, 0};
						}
					} else {
						// Unlock
						lockedEntity = static_cast<Entity>(-1);
						lockedAsset = {0, 0};
					}
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(inspectorLocked ? "Unlock Inspector" : "Lock Inspector");
				}
				return;
			}
			std::cout << "[Inspector] Found fallback path: " << sourceFilePath << std::endl;
		} else {
			sourceFilePath = assetMeta->sourceFilePath;
		}

		// Determine asset type from extension
		std::filesystem::path assetPath(sourceFilePath);
		std::string extension = assetPath.extension().string();

		// Get selected asset for lock callback
		GUID_128 selectedAsset = GUIManager::GetSelectedAsset();

		// Handle different asset types
		if (extension == ".mat") {
			// Check if we have a cached material for this asset
			if (!cachedMaterial || cachedMaterialGuid.high != assetGuid.high || cachedMaterialGuid.low != assetGuid.low) {
				// Convert to absolute path to avoid path resolution issues
				std::filesystem::path absolutePath = std::filesystem::absolute(sourceFilePath);
				std::string absolutePathStr = absolutePath.string();

                // Load material and cache it
                std::cout << "[Inspector] Loading material from: " << sourceFilePath << std::endl;
                std::cout << "[Inspector] Absolute path: " << absolutePathStr << std::endl;
                cachedMaterial = ResourceManager::GetInstance().GetResource<Material>(absolutePathStr);
                if (cachedMaterial) {
                    cachedMaterialGuid = assetGuid;
                    cachedMaterialPath = sourceFilePath;
                    std::cout << "[Inspector] Successfully loaded and cached material: " << cachedMaterial->GetName() << " with " << cachedMaterial->GetAllTextureInfo().size() << " textures" << std::endl;
                } else {
                    cachedMaterial.reset();
                    cachedMaterialGuid = {0, 0};
                    cachedMaterialPath.clear();
                    ImGui::Text("Failed to load material");
                    return;
                }
            }

			// Use cached material with lock button
			auto lockCallback = [this, selectedAsset]() {
				inspectorLocked = !inspectorLocked;
				if (inspectorLocked) {
					lockedAsset = selectedAsset;
					lockedEntity = static_cast<Entity>(-1);
				} else {
					lockedEntity = static_cast<Entity>(-1);
					lockedAsset = {0, 0};
				}
			};

			MaterialInspector::DrawMaterialAsset(cachedMaterial, sourceFilePath, true, &inspectorLocked, lockCallback);
		} else {
			ImGui::Text("Asset type not supported for editing in Inspector");
		}

	} catch (const std::exception& e) {
		ImGui::Text("Error accessing asset: %s", e.what());
	}
}

void InspectorPanel::DrawLightComponents(Entity entity) {
	try {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

		// Draw DirectionalLightComponent if present
		if (ecsManager.HasComponent<DirectionalLightComponent>(entity)) {
			if (DrawComponentHeaderWithRemoval("Directional Light", entity, "DirectionalLightComponent")) {
				ImGui::PushID("DirectionalLight");
				DirectionalLightComponent& light = ecsManager.GetComponent<DirectionalLightComponent>(entity);

				ImGui::Checkbox("Enabled", &light.enabled);
				ImGui::ColorEdit3("Color", &light.color.x);
				ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 10.0f);

				// Direction controls with visual helper
				ImGui::Text("Direction");
				ImGui::DragFloat3("##Direction", &light.direction.x, 0.01f, -1.0f, 1.0f);

				// Direction visualization
				ImGui::SameLine();
				if (ImGui::Button("Normalize")) {
					light.direction = light.direction.Normalized();
				}

				// Show direction as normalized vector and common presets
				Vector3D normalizedDir = light.direction.Normalized();
				ImGui::Text("Normalized: (%.2f, %.2f, %.2f)", normalizedDir.x, normalizedDir.y, normalizedDir.z);

				// Common direction presets
				ImGui::Text("Presets:");
				if (ImGui::Button("Down")) light.direction = Vector3D(0.0f, -1.0f, 0.0f);
				ImGui::SameLine();
				if (ImGui::Button("Forward-Down")) light.direction = Vector3D(-0.2f, -1.0f, -0.3f);
				ImGui::SameLine();
				if (ImGui::Button("Side-Down")) light.direction = Vector3D(-1.0f, -1.0f, 0.0f);

				// Visual direction indicator
				ImGui::Text("Direction Visualization:");
				ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
				ImVec2 canvas_size = ImVec2(100, 100);
				ImDrawList* draw_list = ImGui::GetWindowDrawList();

				// Draw a circle representing the "world"
				ImVec2 center = ImVec2(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y + canvas_size.y * 0.5f);
				draw_list->AddCircle(center, 40.0f, IM_COL32(100, 100, 100, 255), 0, 2.0f);

				// Draw direction arrow (project 3D direction to 2D)
				Vector3D dir = light.direction.Normalized();
				ImVec2 arrow_end = ImVec2(center.x + dir.x * 35.0f, center.y + dir.y * 35.0f);
				draw_list->AddLine(center, arrow_end, IM_COL32(255, 255, 0, 255), 3.0f);

				// Arrow head
				ImVec2 arrowDir = ImVec2(arrow_end.x - center.x, arrow_end.y - center.y);
				float arrowLength = sqrt(arrowDir.x * arrowDir.x + arrowDir.y * arrowDir.y);
				if (arrowLength > 0) {
					arrowDir.x /= arrowLength;
					arrowDir.y /= arrowLength;
					ImVec2 perpendicular = ImVec2(-arrowDir.y, arrowDir.x);
					ImVec2 arrowHead1 = ImVec2(arrow_end.x - arrowDir.x * 8 + perpendicular.x * 4, arrow_end.y - arrowDir.y * 8 + perpendicular.y * 4);
					ImVec2 arrowHead2 = ImVec2(arrow_end.x - arrowDir.x * 8 - perpendicular.x * 4, arrow_end.y - arrowDir.y * 8 - perpendicular.y * 4);
					draw_list->AddLine(arrow_end, arrowHead1, IM_COL32(255, 255, 0, 255), 2.0f);
					draw_list->AddLine(arrow_end, arrowHead2, IM_COL32(255, 255, 0, 255), 2.0f);
				}

				ImGui::Dummy(canvas_size);

				ImGui::Separator();
				ImGui::Text("Lighting Properties");
				ImGui::ColorEdit3("Ambient", &light.ambient.x);
				ImGui::ColorEdit3("Diffuse", &light.diffuse.x);
				ImGui::ColorEdit3("Specular", &light.specular.x);

				ImGui::PopID();
			}
		}

		// Draw PointLightComponent if present
		if (ecsManager.HasComponent<PointLightComponent>(entity)) {
			if (DrawComponentHeaderWithRemoval("Point Light", entity, "PointLightComponent")) {
				ImGui::PushID("PointLight");
				PointLightComponent& light = ecsManager.GetComponent<PointLightComponent>(entity);

				ImGui::Checkbox("Enabled", &light.enabled);
				ImGui::ColorEdit3("Color", &light.color.x);
				ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 10.0f);

				ImGui::Separator();
				ImGui::Text("Attenuation");
				ImGui::DragFloat("Constant", &light.constant, 0.01f, 0.0f, 2.0f);
				ImGui::DragFloat("Linear", &light.linear, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Quadratic", &light.quadratic, 0.01f, 0.0f, 1.0f);

				ImGui::Separator();
				ImGui::Text("Lighting Properties");
				ImGui::ColorEdit3("Ambient", &light.ambient.x);
				ImGui::ColorEdit3("Diffuse", &light.diffuse.x);
				ImGui::ColorEdit3("Specular", &light.specular.x);

				ImGui::PopID();
			}
		}

		// Draw SpotLightComponent if present
		if (ecsManager.HasComponent<SpotLightComponent>(entity)) {
			if (DrawComponentHeaderWithRemoval("Spot Light", entity, "SpotLightComponent")) {
				ImGui::PushID("SpotLight");
				SpotLightComponent& light = ecsManager.GetComponent<SpotLightComponent>(entity);

				ImGui::Checkbox("Enabled", &light.enabled);
				ImGui::ColorEdit3("Color", &light.color.x);
				ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 10.0f);
				ImGui::DragFloat3("Direction", &light.direction.x, 0.1f, -1.0f, 1.0f);

				ImGui::Separator();
				ImGui::Text("Cone Settings");
				float cutOffDegrees = glm::degrees(glm::acos(light.cutOff));
				float outerCutOffDegrees = glm::degrees(glm::acos(light.outerCutOff));
				if (ImGui::DragFloat("Inner Cutoff", &cutOffDegrees, 1.0f, 0.0f, 90.0f)) {
					light.cutOff = glm::cos(glm::radians(cutOffDegrees));
				}
				if (ImGui::DragFloat("Outer Cutoff", &outerCutOffDegrees, 1.0f, 0.0f, 90.0f)) {
					light.outerCutOff = glm::cos(glm::radians(outerCutOffDegrees));
				}

				ImGui::Separator();
				ImGui::Text("Attenuation");
				ImGui::DragFloat("Constant", &light.constant, 0.01f, 0.0f, 2.0f);
				ImGui::DragFloat("Linear", &light.linear, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Quadratic", &light.quadratic, 0.01f, 0.0f, 1.0f);

				ImGui::Separator();
				ImGui::Text("Lighting Properties");
				ImGui::ColorEdit3("Ambient", &light.ambient.x);
				ImGui::ColorEdit3("Diffuse", &light.diffuse.x);
				ImGui::ColorEdit3("Specular", &light.specular.x);

				ImGui::PopID();
			}
		}
	} catch (const std::exception& e) {
		ImGui::Text("Error accessing light components: %s", e.what());
	}
}

void InspectorPanel::DrawAddComponentButton(Entity entity) {
	ImGui::Text("Add Component");

	if (ImGui::Button("Add Component", ImVec2(-1, 30))) {
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (ImGui::BeginPopup("AddComponentPopup")) {
		try {
			ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

			ImGui::Text("Select Component to Add:");
			ImGui::Separator();

			// Rendering Components
			if (ImGui::BeginMenu("Rendering")) {
				if (!ecsManager.HasComponent<ModelRenderComponent>(entity)) {
					if (ImGui::MenuItem("Model Renderer")) {
						AddComponent(entity, "ModelRenderComponent");
					}
				}
				if (!ecsManager.HasComponent<SpriteRenderComponent>(entity)) {
					if (ImGui::MenuItem("Sprite Renderer")) {
						AddComponent(entity, "SpriteRenderComponent");
					}
				}
				if (!ecsManager.HasComponent<TextRenderComponent>(entity)) {
					if (ImGui::MenuItem("Text Renderer")) {
						AddComponent(entity, "TextRenderComponent");
					}
				}
				if (!ecsManager.HasComponent<ParticleComponent>(entity)) {
					if (ImGui::MenuItem("Particle System")) {
						AddComponent(entity, "ParticleComponent");
					}
				}
				ImGui::EndMenu();
			}

			// Audio Components
			if (ImGui::BeginMenu("Audio")) {
				if (!ecsManager.HasComponent<AudioComponent>(entity)) {
					if (ImGui::MenuItem("Audio Source")) {
						AddComponent(entity, "AudioComponent");
					}
				}
				ImGui::EndMenu();
			}

			// Lighting Components
			if (ImGui::BeginMenu("Lighting")) {
				if (!ecsManager.HasComponent<DirectionalLightComponent>(entity)) {
					if (ImGui::MenuItem("Directional Light")) {
						AddComponent(entity, "DirectionalLightComponent");
					}
				}
				if (!ecsManager.HasComponent<PointLightComponent>(entity)) {
					if (ImGui::MenuItem("Point Light")) {
						AddComponent(entity, "PointLightComponent");
					}
				}
				if (!ecsManager.HasComponent<SpotLightComponent>(entity)) {
					if (ImGui::MenuItem("Spot Light")) {
						AddComponent(entity, "SpotLightComponent");
					}
				}
				ImGui::EndMenu();
			}

			// Physics Components
			if (ImGui::BeginMenu("Physics")) {
				if (!ecsManager.HasComponent<ColliderComponent>(entity)) {
					if (ImGui::MenuItem("Collider")) {
						AddComponent(entity, "ColliderComponent");
					}
				}
				if (!ecsManager.HasComponent<RigidBodyComponent>(entity)) {
					if (ImGui::MenuItem("RigidBody")) {
						AddComponent(entity, "RigidBodyComponent");
					}
				}
				ImGui::EndMenu();
			}

		} catch (const std::exception& e) {
			ImGui::Text("Error: %s", e.what());
		}

		ImGui::EndPopup();
	}
}

void InspectorPanel::AddComponent(Entity entity, const std::string& componentType) {
	try {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

		if (componentType == "ModelRenderComponent") {
			ModelRenderComponent component; // Use default constructor

			// Set default shader GUID for new components
			component.shaderGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(ResourceManager::GetPlatformShaderPath("default"));

			// Load the default shader
			std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(component.shaderGUID);
			component.shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(component.shaderGUID, shaderPath);

			if (component.shader) {
				std::cout << "[Inspector] Default shader loaded successfully for new ModelRenderComponent" << std::endl;
			} else {
				std::cerr << "[Inspector] Warning: Failed to load default shader for new ModelRenderComponent" << std::endl;
			}

			ecsManager.AddComponent<ModelRenderComponent>(entity, component);
			std::cout << "[Inspector] Added ModelRenderComponent to entity " << entity << " (ready for model assignment)" << std::endl;
		}
		else if (componentType == "AudioComponent") {
			AudioComponent component;
			ecsManager.AddComponent<AudioComponent>(entity, component);
			std::cout << "[Inspector] Added AudioComponent to entity " << entity << std::endl;
		}
		else if (componentType == "SpriteRenderComponent") {
			// Set default shader GUID for sprite
			GUID_128 spriteShaderGUID = MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("sprite"));

			// Load the default sprite shader
			std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(spriteShaderGUID);
			auto shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(spriteShaderGUID, shaderPath);

			SpriteRenderComponent component;
			component.shader = shader;
			component.shaderGUID = spriteShaderGUID;
			component.texture = nullptr; // Will be set via drag-and-drop
			component.is3D = false; // Default to 2D
			component.isVisible = true;
			component.scale = Vector3D(100.0f, 100.0f, 1.0f); // Default 100x100 pixels for 2D

			ecsManager.AddComponent<SpriteRenderComponent>(entity, component);

			// Ensure entity has a Transform component for positioning
			if (!ecsManager.HasComponent<Transform>(entity)) {
				Transform transform;
				ecsManager.AddComponent<Transform>(entity, transform);
				std::cout << "[Inspector] Added Transform component for Sprite positioning" << std::endl;
			}

			std::cout << "[Inspector] Added SpriteRenderComponent to entity " << entity << std::endl;
		}
		else if (componentType == "DirectionalLightComponent") {
			DirectionalLightComponent component;
			// Set reasonable default values (matching SceneInstance.cpp)
			component.direction = Vector3D(-0.2f, -1.0f, -0.3f);
			component.ambient = Vector3D(0.05f, 0.05f, 0.05f);
			component.diffuse = Vector3D(0.4f, 0.4f, 0.4f);
			component.specular = Vector3D(0.5f, 0.5f, 0.5f);
			component.enabled = true;

			ecsManager.AddComponent<DirectionalLightComponent>(entity, component);

			// Ensure entity has a Transform component
			if (!ecsManager.HasComponent<Transform>(entity)) {
				Transform transform;
				ecsManager.AddComponent<Transform>(entity, transform);
			}

			//// Register entity with lighting system
			//if (ecsManager.lightingSystem) {
			//	ecsManager.lightingSystem->RegisterEntity(entity);
			//}

			std::cout << "[Inspector] Added DirectionalLightComponent to entity " << entity << std::endl;
		}
		else if (componentType == "PointLightComponent") {
			PointLightComponent component;
			// Set reasonable default values (matching SceneInstance.cpp)
			component.ambient = Vector3D(0.05f, 0.05f, 0.05f);
			component.diffuse = Vector3D(0.8f, 0.8f, 0.8f);
			component.specular = Vector3D(1.0f, 1.0f, 1.0f);
			component.constant = 1.0f;
			component.linear = 0.09f;
			component.quadratic = 0.032f;
			component.enabled = true;

			ecsManager.AddComponent<PointLightComponent>(entity, component);

			// Ensure entity has a Transform component for positioning
			if (!ecsManager.HasComponent<Transform>(entity)) {
				Transform transform;
				ecsManager.AddComponent<Transform>(entity, transform);
				std::cout << "[Inspector] Added Transform component for PointLight positioning" << std::endl;
			}

			//// Register entity with lighting system
			//if (ecsManager.lightingSystem) {
			//	ecsManager.lightingSystem->RegisterEntity(entity);
			//}

			std::cout << "[Inspector] Added PointLightComponent to entity " << entity << std::endl;
		}
		else if (componentType == "SpotLightComponent") {
			SpotLightComponent component;
			// Set reasonable default values (matching SceneInstance.cpp)
			component.direction = Vector3D(0.0f, 0.0f, -1.0f);
			component.ambient = Vector3D::Zero();
			component.diffuse = Vector3D::Ones();
			component.specular = Vector3D::Ones();
			component.constant = 1.0f;
			component.linear = 0.09f;
			component.quadratic = 0.032f;
			component.cutOff = 0.976f;      // cos(12.5 degrees)
			component.outerCutOff = 0.966f; // cos(15 degrees)
			component.enabled = true;

			ecsManager.AddComponent<SpotLightComponent>(entity, component);

			// Ensure entity has a Transform component
			if (!ecsManager.HasComponent<Transform>(entity)) {
				Transform transform;
				ecsManager.AddComponent<Transform>(entity, transform);
			}

			//// Register entity with lighting system
			//if (ecsManager.lightingSystem) {
			//	ecsManager.lightingSystem->RegisterEntity(entity);
			//}

			std::cout << "[Inspector] Added SpotLightComponent to entity " << entity << std::endl;
		}
		else if (componentType == "ParticleComponent") {
			ParticleComponent component;
			// Default values are already set in ParticleComponent's default constructor
			// emissionRate = 10.0f
			// maxParticles = 1000
			// particleLifetime = 2.0f
			// startSize = 0.1f, endSize = 0.0f
			// startColor = white, endColor = white with alpha 0
			// gravity = (0, -9.8, 0)
			// velocityRandomness = 1.0f
			// initialVelocity = (0, 1, 0)
			// isEmitting = true

			component.isVisible = true;

			// Load default particle texture and shader (required for rendering)
			std::string defaultTexturePath = AssetManager::GetInstance().GetRootAssetDirectory() + "/Textures/awesomeface.png";
			component.particleTexture = ResourceManager::GetInstance().GetResource<Texture>(defaultTexturePath);
			component.texturePath = defaultTexturePath;  // Store path for display
			component.textureGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(defaultTexturePath);
			component.particleShader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("particle"));

			ecsManager.AddComponent<ParticleComponent>(entity, component);

			// Ensure entity has a Transform component for emitter positioning
			if (!ecsManager.HasComponent<Transform>(entity)) {
				Transform transform;
				ecsManager.AddComponent<Transform>(entity, transform);
				std::cout << "[Inspector] Added Transform component for Particle emitter positioning" << std::endl;
			}

			std::cout << "[Inspector] Added ParticleComponent to entity " << entity << std::endl;
		}
		else if (componentType == "TextRenderComponent") {
			// Load default font and shader GUIDs (matching SceneInstance.cpp)
			std::string defaultFontPath = AssetManager::GetInstance().GetRootAssetDirectory() + "/Fonts/Kenney Mini.ttf";
			GUID_128 fontGUID = MetaFilesManager::GetGUID128FromAssetFile(defaultFontPath);
			GUID_128 shaderGUID = MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("text"));

			// Create component using constructor (matching SceneInstance.cpp pattern)
			TextRenderComponent component("New Text", 48, fontGUID, shaderGUID);

			// Set additional default values
			component.color = Vector3D(1.0f, 1.0f, 1.0f); // White
			component.alignment = TextRenderComponent::Alignment::LEFT;
			component.alignmentInt = 0;
			component.is3D = false;
			component.isVisible = true;
			component.position = Vector3D(100.0f, 100.0f, 0.0f); // Default screen position
			component.scale = 1.0f;

			// Load font and shader resources
			if (std::filesystem::exists(defaultFontPath)) {
				component.font = ResourceManager::GetInstance().GetFontResource(defaultFontPath);
			} else {
				std::cerr << "[Inspector] Warning: Default font not found at " << defaultFontPath << std::endl;
			}

			component.shader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("text"));
			if (!component.shader) {
				std::cerr << "[Inspector] Warning: Failed to load text shader" << std::endl;
			}

			ecsManager.AddComponent<TextRenderComponent>(entity, component);

			// Ensure entity has a Transform component for positioning
			if (!ecsManager.HasComponent<Transform>(entity)) {
				Transform transform;
				transform.localPosition = Vector3D(100.0f, 100.0f, 0.0f); // Default screen position
				ecsManager.AddComponent<Transform>(entity, transform);
				std::cout << "[Inspector] Added Transform component for Text positioning" << std::endl;
			}

			std::cout << "[Inspector] Added TextRenderComponent to entity " << entity << std::endl;
		}
		else if (componentType == "ColliderComponent") {
			ColliderComponent component;
			// Set default box shape - shape will be created by physics system
			component.shapeType = ColliderShapeType::Box;
			component.boxHalfExtents = Vector3D(0.5f, 0.5f, 0.5f);
			component.layer = Layers::MOVING;
			component.shape = nullptr; // Physics system will create the shape
			component.version = 1; // Mark as needing creation

			ecsManager.AddComponent<ColliderComponent>(entity, component);

			// Ensure entity has Transform component
			if (!ecsManager.HasComponent<Transform>(entity)) {
				Transform transform;
				ecsManager.AddComponent<Transform>(entity, transform);
				std::cout << "[Inspector] Added Transform component for Collider" << std::endl;
			}

			std::cout << "[Inspector] Added ColliderComponent to entity " << entity << std::endl;
		}
		else if (componentType == "RigidBodyComponent") {
			RigidBodyComponent component;
			component.motion = Motion::Dynamic;
			component.ccd = false;

			ecsManager.AddComponent<RigidBodyComponent>(entity, component);

			// Ensure entity has Transform component
			if (!ecsManager.HasComponent<Transform>(entity)) {
				Transform transform;
				ecsManager.AddComponent<Transform>(entity, transform);
				std::cout << "[Inspector] Added Transform component for RigidBody" << std::endl;
			}

			std::cout << "[Inspector] Added RigidBodyComponent to entity " << entity << std::endl;
		}
		else {
			std::cerr << "[Inspector] Unknown component type: " << componentType << std::endl;
		}

	} catch (const std::exception& e) {
		std::cerr << "[Inspector] Failed to add component " << componentType << " to entity " << entity << ": " << e.what() << std::endl;
	}
}

bool InspectorPanel::DrawComponentHeaderWithRemoval(const char* label, Entity entity, const std::string& componentType, ImGuiTreeNodeFlags flags) {
	
	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));

	
	ImGui::Spacing();

	
	ImGui::PushID(label);
	bool componentEnabled = true; // TODO: Get actual enabled state from component
	ImGui::Checkbox("##ComponentEnabled", &componentEnabled);
	ImGui::PopID();

	// Collapsing header on same line
	ImGui::SameLine();
	bool isOpen = ImGui::CollapsingHeader(label, flags);

	
	ImGui::SameLine(ImGui::GetWindowWidth() - 40);
	ImGui::PushID((label + std::string("_gear")).c_str());
	if (ImGui::SmallButton(ICON_FA_GEAR)) {
		std::string popupName = "ComponentContextMenu_" + componentType;
		ImGui::OpenPopup(popupName.c_str());
	}
	ImGui::PopID();

	// Context menu for component removal
	std::string popupName = "ComponentContextMenu_" + componentType;
	if (ImGui::BeginPopup(popupName.c_str())) {
		if (ImGui::MenuItem("Remove Component")) {
			// Queue the component removal for processing after ImGui rendering is complete
			pendingComponentRemovals.push_back({entity, componentType});
		}
		if (ImGui::MenuItem("Reset")) {
			// TODO: Implement reset functionality
		}
		if (ImGui::MenuItem("Copy Component")) {
			// TODO: Implement copy functionality
		}
		if (ImGui::MenuItem("Paste Component Values")) {
			// TODO: Implement paste functionality
		}
		ImGui::EndPopup();
	}

	ImGui::PopStyleColor(3);

	
	if (isOpen) {
		ImGui::Spacing();
	}

	return isOpen;
}

void InspectorPanel::ProcessPendingComponentRemovals() {
	for (const auto& request : pendingComponentRemovals) {
		try {
			ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

			// Remove the component based on type
			if (request.componentType == "DirectionalLightComponent") {
				ecsManager.RemoveComponent<DirectionalLightComponent>(request.entity);
				std::cout << "[Inspector] Removed DirectionalLightComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "PointLightComponent") {
				ecsManager.RemoveComponent<PointLightComponent>(request.entity);
				std::cout << "[Inspector] Removed PointLightComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "SpotLightComponent") {
				ecsManager.RemoveComponent<SpotLightComponent>(request.entity);
				std::cout << "[Inspector] Removed SpotLightComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "ModelRenderComponent") {
				ecsManager.RemoveComponent<ModelRenderComponent>(request.entity);
				std::cout << "[Inspector] Removed ModelRenderComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "SpriteRenderComponent") {
				ecsManager.RemoveComponent<SpriteRenderComponent>(request.entity);
				std::cout << "[Inspector] Removed SpriteRenderComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "TextRenderComponent") {
				ecsManager.RemoveComponent<TextRenderComponent>(request.entity);
				std::cout << "[Inspector] Removed TextRenderComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "ParticleComponent") {
				ecsManager.RemoveComponent<ParticleComponent>(request.entity);
				std::cout << "[Inspector] Removed ParticleComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "AudioComponent") {
				ecsManager.RemoveComponent<AudioComponent>(request.entity);
				std::cout << "[Inspector] Removed AudioComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "ColliderComponent") {
				ecsManager.RemoveComponent<ColliderComponent>(request.entity);
				std::cout << "[Inspector] Removed ColliderComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "RigidBodyComponent") {
				ecsManager.RemoveComponent<RigidBodyComponent>(request.entity);
				std::cout << "[Inspector] Removed RigidBodyComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "TransformComponent") {
				std::cerr << "[Inspector] Cannot remove TransformComponent - all entities must have one" << std::endl;
			}
			else {
				std::cerr << "[Inspector] Unknown component type for removal: " << request.componentType << std::endl;
			}
		} catch (const std::exception& e) {
			std::cerr << "[Inspector] Failed to remove component " << request.componentType << " from entity " << request.entity << ": " << e.what() << std::endl;
		}
	}

	// Clear the queue after processing
	pendingComponentRemovals.clear();
}

void InspectorPanel::ApplyModelToRenderer(Entity entity, const GUID_128& modelGuid, const std::string& modelPath) {
	try {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

		if (!ecsManager.HasComponent<ModelRenderComponent>(entity)) {
			std::cerr << "[Inspector] Entity " << entity << " does not have ModelRenderComponent" << std::endl;
			return;
		}

		ModelRenderComponent& modelRenderer = ecsManager.GetComponent<ModelRenderComponent>(entity);

		std::cout << "[Inspector] Applying model to entity " << entity << " - GUID: {" << modelGuid.high << ", " << modelGuid.low << "}, Path: " << modelPath << std::endl;

		// Try to load model using GUID first, then fallback to path
		std::shared_ptr<Model> loadedModel = nullptr;

		if (modelGuid.high != 0 || modelGuid.low != 0) {
			std::cout << "[Inspector] Loading model using GUID..." << std::endl;
			loadedModel = ResourceManager::GetInstance().GetResourceFromGUID<Model>(modelGuid, modelPath);
		} else if (!modelPath.empty()) {
			std::cout << "[Inspector] Loading model using path: " << modelPath << std::endl;
			loadedModel = ResourceManager::GetInstance().GetResource<Model>(modelPath);
		}

		if (loadedModel) {
			std::cout << "[Inspector] Model loaded successfully, applying to ModelRenderComponent..." << std::endl;
			modelRenderer.model = loadedModel;
			modelRenderer.modelGUID = modelGuid;
			modelRenderer.shader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("default"));
			modelRenderer.shaderGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(ResourceManager::GetPlatformShaderPath("default"));

			// Ensure entity has a shader for rendering
			if (modelRenderer.shaderGUID.high == 0 && modelRenderer.shaderGUID.low == 0) {
				std::cout << "[Inspector] Setting default shader for entity " << entity << std::endl;
				modelRenderer.shaderGUID = {0x007ebbc8de41468e, 0x0002c7078200001b}; // Default shader GUID
			}

			//// Load the shader if it's not already loaded
			//if (!modelRenderer.shader) {
			//	std::cout << "[Inspector] Loading shader for entity " << entity << std::endl;
			//	//std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(modelRenderer.shaderGUID);
			//	// By default, always load the default shader for models.
			//	modelRenderer.shader = ResourceManager::GetInstance().GetResource<Shader>(ResourceManager::GetPlatformShaderPath("default"));

			//	if (modelRenderer.shader) {
			//		std::cout << "[Inspector] Shader loaded successfully" << std::endl;
			//	} else {
			//		std::cerr << "[Inspector] Failed to load shader for entity " << entity << std::endl;
			//	}
			//}

			std::cout << "[Inspector] Model successfully applied to entity " << entity << std::endl;
		} else {
			std::cerr << "[Inspector] Failed to load model for entity " << entity << std::endl;
		}

	} catch (const std::exception& e) {
		std::cerr << "[Inspector] Error applying model to entity " << entity << ": " << e.what() << std::endl;
	}
}

void InspectorPanel::DrawColliderComponent(Entity entity) {
	try {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
		ColliderComponent& collider = ecsManager.GetComponent<ColliderComponent>(entity);

		ImGui::PushID("ColliderComponent");

		// Shape Type dropdown
		ImGui::Text("Shape Type");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1);
		const char* shapeTypes[] = { "Box", "Sphere", "Capsule", "Cylinder" };
		int currentShapeType = static_cast<int>(collider.shapeType);
		EditorComponents::PushComboColors();
		if (ImGui::Combo("##ShapeType", &currentShapeType, shapeTypes, IM_ARRAYSIZE(shapeTypes))) {
			collider.shapeType = static_cast<ColliderShapeType>(currentShapeType);
			collider.version++; // Mark for recreation
		}
		EditorComponents::PopComboColors();

		// Shape Parameters based on type
		bool shapeParamsChanged = false;
		switch (collider.shapeType) {
			case ColliderShapeType::Box: {
				ImGui::Text("Half Extents");
				ImGui::SameLine();
				float halfExtents[3] = { collider.boxHalfExtents.x, collider.boxHalfExtents.y, collider.boxHalfExtents.z };
				if (ImGui::DragFloat3("##HalfExtents", halfExtents, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
					collider.boxHalfExtents = Vector3D(halfExtents[0], halfExtents[1], halfExtents[2]);
					shapeParamsChanged = true;
				}
				break;
			}
			case ColliderShapeType::Sphere: {
				ImGui::Text("Radius");
				ImGui::SameLine();
				if (ImGui::DragFloat("##SphereRadius", &collider.sphereRadius, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
					shapeParamsChanged = true;
				}
				break;
			}
			case ColliderShapeType::Capsule: {
				ImGui::Text("Radius");
				ImGui::SameLine();
				if (ImGui::DragFloat("##CapsuleRadius", &collider.capsuleRadius, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
					shapeParamsChanged = true;
				}
				ImGui::Text("Half Height");
				ImGui::SameLine();
				if (ImGui::DragFloat("##CapsuleHalfHeight", &collider.capsuleHalfHeight, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
					shapeParamsChanged = true;
				}
				break;
			}
			case ColliderShapeType::Cylinder: {
				ImGui::Text("Radius");
				ImGui::SameLine();
				if (ImGui::DragFloat("##CylinderRadius", &collider.cylinderRadius, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
					shapeParamsChanged = true;
				}
				ImGui::Text("Half Height");
				ImGui::SameLine();
				if (ImGui::DragFloat("##CylinderHalfHeight", &collider.cylinderHalfHeight, 0.1f, 0.01f, FLT_MAX, "%.2f")) {
					shapeParamsChanged = true;
				}
				break;
			}
		}

		if (shapeParamsChanged) {
			collider.version++; // Mark for recreation - physics system will recreate the shape
		}

		// Physics Layer dropdown
		ImGui::Text("Layer");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1);
		const char* layers[] = { "Non-Moving", "Moving", "Sensor", "Debris" };
		int currentLayer = static_cast<int>(collider.layer);
		EditorComponents::PushComboColors();
		if (ImGui::Combo("##PhysicsLayer", &currentLayer, layers, IM_ARRAYSIZE(layers))) {
			collider.layer = static_cast<JPH::ObjectLayer>(currentLayer);
			collider.version++; // Mark for recreation
		}
		EditorComponents::PopComboColors();

		ImGui::PopID();
	}
	catch (const std::exception& e) {
		ImGui::Text("Error rendering ColliderComponent: %s", e.what());
	}
}

void InspectorPanel::DrawRigidBodyComponent(Entity entity) {
	try {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
		RigidBodyComponent& rigidBody = ecsManager.GetComponent<RigidBodyComponent>(entity);

		ImGui::PushID("RigidBodyComponent");

		// Motion Type dropdown
		ImGui::Text("Motion");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1);
		const char* motionTypes[] = { "Static", "Kinematic", "Dynamic" };
		int currentMotion = static_cast<int>(rigidBody.motion);
		EditorComponents::PushComboColors();
		if (ImGui::Combo("##MotionType", &currentMotion, motionTypes, IM_ARRAYSIZE(motionTypes))) {
			rigidBody.motion = static_cast<Motion>(currentMotion);
			rigidBody.motion_dirty = true; // Mark for recreation
		}
		EditorComponents::PopComboColors();

		// CCD checkbox
		ImGui::Text("CCD");
		ImGui::SameLine();
		if (ImGui::Checkbox("##CCD", &rigidBody.ccd)) {
			rigidBody.motion_dirty = true; // Mark for recreation
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Continuous Collision Detection - prevents fast-moving objects from tunneling");
		}

		ImGui::PopID();
	}
	catch (const std::exception& e) {
		ImGui::Text("Error rendering RigidBodyComponent: %s", e.what());
	}
}
