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
#include <Graphics/Camera/CameraComponent.hpp>
#include <Physics/ColliderComponent.hpp>
#include <Physics/RigidBodyComponent.hpp>
#include <Physics/CollisionLayers.hpp>
#include <Physics/PhysicsSystem.hpp>
#include <Graphics/Texture.h>
#include <Graphics/ShaderClass.h>
#include <Graphics/GraphicsManager.hpp>
#include <Asset Manager/AssetManager.hpp>
#include <Asset Manager/ResourceManager.hpp>
#include <Asset Manager/MetaFilesManager.hpp>
#include <Utilities/GUID.hpp>
#include "PrefabLinkComponent.hpp"
#include <ECS/TagComponent.hpp>
#include <ECS/LayerComponent.hpp>
#include <ECS/TagManager.hpp>
#include <ECS/LayerManager.hpp>
#include "Game AI/Brain.hpp"
#include "Game AI/BrainFactory.hpp"
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
#include "Sound/AudioComponent.hpp"
#include "Sound/AudioListenerComponent.hpp"
#include <Animation/AnimationComponent.hpp>
#include <RunTimeVar.hpp>
#include <Panels/AssetInspector.hpp>
#include "ReflectionRenderer.hpp"

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

static inline bool IsPrefabInstance(ECSManager& ecs, Entity e) {
	return ecs.HasComponent<PrefabLinkComponent>(e);
}


InspectorPanel::InspectorPanel()
	: EditorPanel("Inspector", true) {
	// Register custom field renderers for special cases
	static bool renderersRegistered = false;
	if (!renderersRegistered) {
		RegisterInspectorCustomRenderers();
		renderersRegistered = true;
	}
}

void InspectorPanel::DrawComponentGeneric(void* componentPtr, const char* componentTypeName, Entity entity) {
	// Get type descriptor from reflection system
	auto& lookup = TypeDescriptor::type_descriptor_lookup();
	auto it = lookup.find(componentTypeName);
	if (it == lookup.end()) {
		ImGui::TextDisabled("Component not reflected: %s", componentTypeName);
		return;
	}

	TypeDescriptor_Struct* typeDesc = dynamic_cast<TypeDescriptor_Struct*>(it->second);
	if (!typeDesc) {
		ImGui::TextDisabled("Not a struct type: %s", componentTypeName);
		return;
	}

	// Render using reflection
	ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
	ImGui::PushID(componentPtr);
	try {
		ReflectionRenderer::RenderComponent(componentPtr, typeDesc, entity, ecs);
	} catch (const std::exception& e) {
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error rendering component: %s", e.what());
	}
	ImGui::PopID();
}

void InspectorPanel::DrawComponentsViaReflection(Entity entity) {
	ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

	// Component info structure for generic rendering
	struct ComponentInfo {
		const char* displayName;
		const char* typeName;
		std::function<void*()> getComponent;
		std::function<bool()> hasComponent;
	};

	// Define all components that can be rendered via reflection
	std::vector<ComponentInfo> components = {
		// Name component (rendered at top, before other components)
		{"Name", "NameComponent",
			[&]() { return ecs.HasComponent<NameComponent>(entity) ?
				(void*)&ecs.GetComponent<NameComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<NameComponent>(entity); }},

		// Tag component (rendered with Name on same line)
		{"Tag", "TagComponent",
			[&]() { return ecs.HasComponent<TagComponent>(entity) ?
				(void*)&ecs.GetComponent<TagComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<TagComponent>(entity); }},

		// Layer component (rendered with Tag on same line)
		{"Layer", "LayerComponent",
			[&]() { return ecs.HasComponent<LayerComponent>(entity) ?
				(void*)&ecs.GetComponent<LayerComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<LayerComponent>(entity); }},

		// Transform (special handling via custom renderer for TransformSystem)
		{"Transform", "Transform",
			[&]() { return ecs.HasComponent<Transform>(entity) ?
				(void*)&ecs.GetComponent<Transform>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<Transform>(entity); }},

		// Model Renderer (special handling for drag-drop in custom renderer)
		{"Model Renderer", "ModelRenderComponent",
			[&]() { return ecs.HasComponent<ModelRenderComponent>(entity) ?
				(void*)&ecs.GetComponent<ModelRenderComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<ModelRenderComponent>(entity); }},

		// Render components
		{"Sprite Renderer", "SpriteRenderComponent",
			[&]() { return ecs.HasComponent<SpriteRenderComponent>(entity) ?
				(void*)&ecs.GetComponent<SpriteRenderComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<SpriteRenderComponent>(entity); }},

		{"Text Renderer", "TextRenderComponent",
			[&]() { return ecs.HasComponent<TextRenderComponent>(entity) ?
				(void*)&ecs.GetComponent<TextRenderComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<TextRenderComponent>(entity); }},

		{"Particle System", "ParticleComponent",
			[&]() { return ecs.HasComponent<ParticleComponent>(entity) ?
				(void*)&ecs.GetComponent<ParticleComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<ParticleComponent>(entity); }},

		// Audio component
		{"Audio Source", "AudioComponent",
			[&]() { return ecs.HasComponent<AudioComponent>(entity) ?
				(void*)&ecs.GetComponent<AudioComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<AudioComponent>(entity); }},

		{"Audio Listener", "AudioListenerComponent",
			[&]() { return ecs.HasComponent<AudioListenerComponent>(entity) ?
				(void*)&ecs.GetComponent<AudioListenerComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<AudioListenerComponent>(entity); }},

		// Light components
		{"Directional Light", "DirectionalLightComponent",
			[&]() { return ecs.HasComponent<DirectionalLightComponent>(entity) ?
				(void*)&ecs.GetComponent<DirectionalLightComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<DirectionalLightComponent>(entity); }},

		{"Point Light", "PointLightComponent",
			[&]() { return ecs.HasComponent<PointLightComponent>(entity) ?
				(void*)&ecs.GetComponent<PointLightComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<PointLightComponent>(entity); }},

		{"Spot Light", "SpotLightComponent",
			[&]() { return ecs.HasComponent<SpotLightComponent>(entity) ?
				(void*)&ecs.GetComponent<SpotLightComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<SpotLightComponent>(entity); }},

		// Physics components
		{"Collider", "ColliderComponent",
			[&]() { return ecs.HasComponent<ColliderComponent>(entity) ?
				(void*)&ecs.GetComponent<ColliderComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<ColliderComponent>(entity); }},

		{"Rigid Body", "RigidBodyComponent",
			[&]() { return ecs.HasComponent<RigidBodyComponent>(entity) ?
				(void*)&ecs.GetComponent<RigidBodyComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<RigidBodyComponent>(entity); }},

		// Camera component
		{"Camera", "CameraComponent",
			[&]() { return ecs.HasComponent<CameraComponent>(entity) ?
				(void*)&ecs.GetComponent<CameraComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<CameraComponent>(entity); }},

		{"Animation Component", "AnimationComponent",
			[&]() { return ecs.HasComponent<AnimationComponent>(entity) ?
				(void*)&ecs.GetComponent<AnimationComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<AnimationComponent>(entity); }},

		{"Brain", "Brain",
			[&]() { return ecs.HasComponent<Brain>(entity) ?
				(void*)&ecs.GetComponent<Brain>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<Brain>(entity); }},
	};

	// Render each component that exists
	for (const auto& info : components) {
		if (!info.hasComponent()) continue;

		void* componentPtr = info.getComponent();
		if (!componentPtr) continue;

		// Special components (Name, Tag, Layer) don't use collapsing headers
		bool isSpecialComponent = (std::string(info.typeName) == "NameComponent" ||
								   std::string(info.typeName) == "TagComponent" ||
								   std::string(info.typeName) == "LayerComponent");

		if (isSpecialComponent) {
			// Render directly without collapsing header
			DrawComponentGeneric(componentPtr, info.typeName, entity);
		} else {
			// Normal components get collapsing header
			if (DrawComponentHeaderWithRemoval(info.displayName, entity, info.typeName)) {
				DrawComponentGeneric(componentPtr, info.typeName, entity);
			}
		}
	}
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
				ImGui::SameLine(ImGui::GetWindowWidth() - 40);
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
					ImGui::SameLine(ImGui::GetWindowWidth() - 42);
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

					// All components (Name, Tag, Layer, Transform, etc.) are now rendered via reflection
					// See DrawComponentsViaReflection() which uses custom renderers for special UI

					// ===================================================================
					// COMPONENT RENDERING VIA REFLECTION
					// ===================================================================
					// All components are now rendered using the reflection system.
					// Special cases (Transform, Collider, Camera, etc.) have custom
					// renderers registered in InspectorCustomRenderers.cpp.
					// ===================================================================
					DrawComponentsViaReflection(displayEntity);

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

void InspectorPanel::DrawTagComponent(Entity entity) {
	try {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
		if (!ecsManager.HasComponent<TagComponent>(entity)) {
			ecsManager.AddComponent<TagComponent>(entity, TagComponent{0});
		}
		TagComponent& tagComponent = ecsManager.GetComponent<TagComponent>(entity);

		ImGui::PushID("TagComponent");

		// Get available tags
		const auto& availableTags = TagManager::GetInstance().GetAllTags();

		// Create items for combo box, including "Add Tag..." option
		std::vector<std::string> tagItems;
		tagItems.reserve(availableTags.size() + 1);
		for (const auto& tag : availableTags) {
			tagItems.push_back(tag);
		}
		tagItems.push_back("Add Tag...");

		// Convert to const char* array for ImGui
		std::vector<const char*> tagItemPtrs;
		tagItemPtrs.reserve(tagItems.size());
		for (const auto& item : tagItems) {
			tagItemPtrs.push_back(item.c_str());
		}

		// Ensure tagIndex is valid
		if (tagComponent.tagIndex < 0 || tagComponent.tagIndex >= static_cast<int>(availableTags.size())) {
			tagComponent.tagIndex = 0; // Default to first tag
		}

		// Combo box for tag selection
		int currentTag = tagComponent.tagIndex;
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::Combo("##Tag", &currentTag, tagItemPtrs.data(), static_cast<int>(tagItemPtrs.size()))) {
			if (currentTag >= 0 && currentTag < static_cast<int>(availableTags.size())) {
				tagComponent.tagIndex = currentTag;
			} else if (currentTag == static_cast<int>(availableTags.size())) {
				// "Add Tag..." was selected - open Tags & Layers window
				auto tagsLayersPanel = GUIManager::GetPanelManager().GetPanel("Tags & Layers");
				if (tagsLayersPanel) {
					tagsLayersPanel->SetOpen(true);
				}
				// Reset selection to current tag
				currentTag = tagComponent.tagIndex;
			}
		}

		ImGui::PopID();
	} catch (const std::exception& e) {
		ImGui::Text("Error accessing TagComponent: %s", e.what());
	}
}

void InspectorPanel::DrawLayerComponent(Entity entity) {
	try {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
		if (!ecsManager.HasComponent<LayerComponent>(entity)) {
			ecsManager.AddComponent<LayerComponent>(entity, LayerComponent{0});
		}
		LayerComponent& layerComponent = ecsManager.GetComponent<LayerComponent>(entity);

		ImGui::PushID("LayerComponent");

		// Get available layers
		const auto& availableLayers = LayerManager::GetInstance().GetAllLayers();

		// Create items for combo box (only show named layers)
		std::vector<std::string> layerItems;
		std::vector<int> layerIndices;
		for (int i = 0; i < LayerManager::MAX_LAYERS; ++i) {
			const std::string& layerName = availableLayers[i];
			if (!layerName.empty()) {
				layerItems.push_back(std::to_string(i) + ": " + layerName);
				layerIndices.push_back(i);
			}
		}

		// Add "Add Layer..." option
		layerItems.push_back("Add Layer...");
		std::vector<int> tempIndices = layerIndices;
		tempIndices.push_back(-1); // Special value for "Add Layer..."

		// Convert to const char* array for ImGui
		std::vector<const char*> layerItemPtrs;
		layerItemPtrs.reserve(layerItems.size());
		for (const auto& item : layerItems) {
			layerItemPtrs.push_back(item.c_str());
		}

		// Ensure layerIndex is valid
		if (layerComponent.layerIndex < 0 || layerComponent.layerIndex >= LayerManager::MAX_LAYERS) {
			layerComponent.layerIndex = 0; // Default to first layer
		}

		// Find current selection index in our filtered list
		int currentSelection = -1;
		for (size_t i = 0; i < layerIndices.size(); ++i) {
			if (layerIndices[i] == layerComponent.layerIndex) {
				currentSelection = static_cast<int>(i);
				break;
			}
		}

		// If current layer is not in the named list, add it
		if (currentSelection == -1) {
			const std::string& currentLayerName = layerComponent.GetLayerName();
			if (!currentLayerName.empty()) {
				std::string item = std::to_string(layerComponent.layerIndex) + ": " + currentLayerName;
				layerItems.insert(layerItems.end() - 1, item); // Insert before "Add Layer..."
				tempIndices.insert(tempIndices.end() - 1, layerComponent.layerIndex);
				layerItemPtrs.insert(layerItemPtrs.end() - 1, layerItems[layerItems.size() - 2].c_str());
				currentSelection = static_cast<int>(layerItems.size() - 2);
			} else {
				// Default to first item
				currentSelection = 0;
				layerComponent.layerIndex = layerIndices[0];
			}
		}

		// Combo box for layer selection
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::Combo("##Layer", &currentSelection, layerItemPtrs.data(), static_cast<int>(layerItemPtrs.size()))) {
			if (currentSelection >= 0 && currentSelection < static_cast<int>(tempIndices.size())) {
				int selectedIndex = tempIndices[currentSelection];
				if (selectedIndex == -1) {
					// "Add Layer..." was selected - open Tags & Layers window
					auto tagsLayersPanel = GUIManager::GetPanelManager().GetPanel("Tags & Layers");
					if (tagsLayersPanel) {
						tagsLayersPanel->SetOpen(true);
					}
					// Reset selection to current layer
					currentSelection = -1;
					for (size_t i = 0; i < layerIndices.size(); ++i) {
						if (layerIndices[i] == layerComponent.layerIndex) {
							currentSelection = static_cast<int>(i);
							break;
						}
					}
				} else {
					layerComponent.layerIndex = selectedIndex;
				}
			}
		}

		ImGui::PopID();
	} catch (const std::exception& e) {
		ImGui::Text("Error accessing LayerComponent: %s", e.what());
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

//void InspectorPanel::DrawBrainComponent(Entity entity) {
//	try {
//		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
//		Brain& brain = ecsManager.GetComponent<Brain>(entity);
//
//		ImGui::Separator();
//		ImGui::Text("Active State: %s", brain.impl ? brain.impl->activeStateName() : "-");
//
//		if (ImGui::Button(brain.started ? "Rebuild" : "Build")) {
//			if (brain.impl) brain.impl->onExit(ecsManager, entity);
//			brain.impl = game_ai::CreateFor(ecsManager, entity, brain.kind);  // use your overload
//			if (brain.impl) { brain.impl->onEnter(ecsManager, entity); brain.started = true; }
//		}
//		ImGui::SameLine();
//		if (ImGui::Button("Stop")) {
//			if (brain.impl) brain.impl->onExit(ecsManager, entity);
//			brain.impl.reset();
//			brain.started = false;
//		}
//	} catch (const std::exception& e) {
//		ImGui::Text("Error accessing Brain component: %s", e.what());
//	}
//}

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
				ImGui::SameLine(ImGui::GetWindowWidth() - 40);
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

		// Lock/Unlock callback
		auto lockCallback = [this, selectedAsset]() {
			inspectorLocked = !inspectorLocked;
			if (inspectorLocked) {
				lockedAsset = selectedAsset;
				lockedEntity = static_cast<Entity>(-1);
			}
			else {
				lockedEntity = static_cast<Entity>(-1);
				lockedAsset = { 0, 0 };
			}
			};

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

			MaterialInspector::DrawMaterialAsset(cachedMaterial, sourceFilePath, true, &inspectorLocked, lockCallback);
		} 
		else if (AssetManager::GetInstance().IsAssetExtensionSupported(extension)) {
			std::shared_ptr<AssetMeta> assetMeta = AssetManager::GetInstance().GetAssetMeta(selectedAsset);
			AssetInspector::DrawAssetMetaInfo(assetMeta, sourceFilePath, true, &inspectorLocked, lockCallback);
		}
		else {
			ImGui::Text("Asset type not supported for editing in Inspector");
		}

	} catch (const std::exception& e) {
		ImGui::Text("Error accessing asset: %s", e.what());
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
				if (!ecsManager.HasComponent<AudioListenerComponent>(entity)) {
					if (ImGui::MenuItem("Audio Listener")) {
						AddComponent(entity, "AudioListenerComponent");
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

			// Camera Components
			if (ImGui::BeginMenu("Camera")) {
				if (!ecsManager.HasComponent<CameraComponent>(entity)) {
					if (ImGui::MenuItem("Camera")) {
						AddComponent(entity, "CameraComponent");
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

			// Animation Components
			if (ImGui::BeginMenu("Animation")) {
				if (!ecsManager.HasComponent<AnimationComponent>(entity)) {
					if (ImGui::MenuItem("Animation Component")) {
						AddComponent(entity, "AnimationComponent");
					}
				}
				ImGui::EndMenu();
			}

			// AI Components
			if (ImGui::BeginMenu("AI")) {
				if (!ecsManager.HasComponent<Brain>(entity)) {
					if (ImGui::MenuItem("Brain")) {
						AddComponent(entity, "Brain");
					}
				}
				ImGui::EndMenu();
			}

			// General Components
			if (ImGui::BeginMenu("General")) {
				if (!ecsManager.HasComponent<TagComponent>(entity)) {
					if (ImGui::MenuItem("Tag")) {
						AddComponent(entity, "TagComponent");
					}
				}
				if (!ecsManager.HasComponent<LayerComponent>(entity)) {
					if (ImGui::MenuItem("Layer")) {
						AddComponent(entity, "LayerComponent");
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
		else if (componentType == "AudioListenerComponent") {
			AudioListenerComponent component;
			ecsManager.AddComponent<AudioListenerComponent>(entity, component);
			std::cout << "[Inspector] Added AudioListenerComponent to entity " << entity << std::endl;
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

			// Ensure entity has Transform component
			if (!ecsManager.HasComponent<Transform>(entity)) {
				Transform transform;
				ecsManager.AddComponent<Transform>(entity, transform);
				std::cout << "[Inspector] Added Transform component for Collider" << std::endl;
			}

			ColliderComponent component;
			// Set default box shape - shape will be created by physics system
			component.shapeType = ColliderShapeType::Box;
			component.shapeTypeID = static_cast<int>(component.shapeType);
			if (ecsManager.HasComponent<ModelRenderComponent>(entity))
			{
				auto& rc = ecsManager.GetComponent<ModelRenderComponent>(entity);
				auto& transform = ecsManager.GetComponent<Transform>(entity);
				if (rc.model)
					component.boxHalfExtents = rc.CalculateModelHalfExtent(*rc.model);	//no need apply local scale
			}
			component.layer = Layers::MOVING;
			component.layerID = static_cast<int>(component.layer);
			component.shape = nullptr; // Physics system will create the shape
			component.version = 1; // Mark as needing creation

			ecsManager.AddComponent<ColliderComponent>(entity, component);


			std::cout << "[Inspector] Added ColliderComponent to entity " << entity << std::endl;
		}
		else if (componentType == "RigidBodyComponent") {
			RigidBodyComponent component;
			component.motion = Motion::Static;
			component.motionID = static_cast<int>(component.motion);
	
			ecsManager.AddComponent<RigidBodyComponent>(entity, component);

			// Ensure entity has Transform component
			if (!ecsManager.HasComponent<Transform>(entity)) {
				Transform transform;
				ecsManager.AddComponent<Transform>(entity, transform);
				std::cout << "[Inspector] Added Transform component for RigidBody" << std::endl;
			}

			std::cout << "[Inspector] Added RigidBodyComponent to entity " << entity << std::endl;
		}
		else if (componentType == "TagComponent") {
			TagComponent component;
			ecsManager.AddComponent<TagComponent>(entity, component);
			std::cout << "[Inspector] Added TagComponent to entity " << entity << std::endl;
		}
		else if (componentType == "LayerComponent") {
			LayerComponent component;
			ecsManager.AddComponent<LayerComponent>(entity, component);
			std::cout << "[Inspector] Added LayerComponent to entity " << entity << std::endl;
		}
		else if (componentType == "CameraComponent") {
			CameraComponent component;
			// Set reasonable defaults
			component.isActive = false;  // Don't activate immediately
			component.priority = 0;
			component.fov = 45.0f;
			component.nearPlane = 0.1f;
			component.farPlane = 100.0f;
			component.projectionType = ProjectionType::PERSPECTIVE;
			component.useFreeRotation = true;
			component.yaw = -90.0f;
			component.pitch = 0.0f;
			component.movementSpeed = 2.5f;
			component.mouseSensitivity = 0.1f;
			component.minZoom = 1.0f;
			component.maxZoom = 90.0f;

			ecsManager.AddComponent<CameraComponent>(entity, component);

			// Ensure entity has Transform component for positioning
			if (!ecsManager.HasComponent<Transform>(entity)) {
				Transform transform;
				ecsManager.AddComponent<Transform>(entity, transform);
				std::cout << "[Inspector] Added Transform component for Camera positioning" << std::endl;
			}

			std::cout << "[Inspector] Added CameraComponent to entity " << entity << std::endl;
		}
		else if (componentType == "AnimationComponent") {
			AnimationComponent component;
			ecsManager.AddComponent<AnimationComponent>(entity, component);

			if (!ecsManager.HasComponent<Transform>(entity)) {
				Transform transform;
				ecsManager.AddComponent<Transform>(entity, transform);
				std::cout << "[Inspector] Added Transform component for Animator" << std::endl;
			}

			std::cout << "[Inspector] Added AnimationComponent to entity " << entity << std::endl;
		}
		else if (componentType == "Brain") {
			Brain component;
			ecsManager.AddComponent<Brain>(entity, component);
			std::cout << "[Inspector] Added Brain to entity " << entity << std::endl;
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

	// Core components cannot be disabled, so don't show checkbox
	bool isCoreComponent = (componentType == "Transform" ||
							componentType == "NameComponent" ||
							componentType == "TagComponent" ||
							componentType == "LayerComponent");

	if (!isCoreComponent) {
		// Component enable/disable checkbox (Unity-style)
		// Get reference to actual component's enabled/isActive/isVisible field
		ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
		bool* enabledFieldPtr = nullptr;

		// Get the appropriate enabled field for each component type
		if (componentType == "CameraComponent") {
			auto& comp = ecs.GetComponent<CameraComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		} else if (componentType == "DirectionalLightComponent") {
			auto& comp = ecs.GetComponent<DirectionalLightComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		} else if (componentType == "PointLightComponent") {
			auto& comp = ecs.GetComponent<PointLightComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		} else if (componentType == "SpotLightComponent") {
			auto& comp = ecs.GetComponent<SpotLightComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		} else if (componentType == "ModelRenderComponent") {
			auto& comp = ecs.GetComponent<ModelRenderComponent>(entity);
			enabledFieldPtr = &comp.isVisible;
		} else if (componentType == "SpriteRenderComponent") {
			auto& comp = ecs.GetComponent<SpriteRenderComponent>(entity);
			enabledFieldPtr = &comp.isVisible;
		} else if (componentType == "TextRenderComponent") {
			auto& comp = ecs.GetComponent<TextRenderComponent>(entity);
			enabledFieldPtr = &comp.isVisible;
		} else if (componentType == "ParticleComponent") {
			auto& comp = ecs.GetComponent<ParticleComponent>(entity);
			enabledFieldPtr = &comp.isVisible;
		} else if (componentType == "AudioComponent") {
			auto& comp = ecs.GetComponent<AudioComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		} else if (componentType == "AudioListenerComponent") {
			auto& comp = ecs.GetComponent<AudioListenerComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		} else if (componentType == "ColliderComponent") {
			auto& comp = ecs.GetComponent<ColliderComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		} else if (componentType == "RigidBodyComponent") {
			auto& comp = ecs.GetComponent<RigidBodyComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		} else if (componentType == "AnimationComponent") {
			auto& comp = ecs.GetComponent<AnimationComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		}

		if (enabledFieldPtr) {
			// Style the checkbox to match entity checkbox (white checkmark, smaller size)
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
			ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // White checkmark
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.3f, 0.3f, 1.0f)); // Dark gray background
			ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

			std::string checkboxId = "##ComponentEnabled_" + componentType;
			ImGui::PushID(entity);
			ImGui::Checkbox(checkboxId.c_str(), enabledFieldPtr);
			ImGui::PopID();

			ImGui::PopStyleColor(4);
			ImGui::PopStyleVar();

			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Enable/Disable Component");
			}

			// Collapsing header on same line
			ImGui::SameLine();
		}
	}

	bool checkisOpen = ImGui::CollapsingHeader(label, flags);

	// Check for right-click on the collapsing header
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
		std::string popupName = "ComponentContextMenu_" + componentType;
		ImGui::OpenPopup(popupName.c_str());
	}

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
		//if (ImGui::MenuItem("Reset")) {
		//	// TODO: Implement reset functionality
		//}
		//if (ImGui::MenuItem("Copy Component")) {
		//	// TODO: Implement copy functionality
		//}
		//if (ImGui::MenuItem("Paste Component Values")) {
		//	// TODO: Implement paste functionality
		//}
		ImGui::EndPopup();
	}

	ImGui::PopStyleColor(3);

	
	if (checkisOpen) {
		ImGui::Spacing();
	}

	return checkisOpen;
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
			else if (request.componentType == "AudioListenerComponent") {
				ecsManager.RemoveComponent<AudioListenerComponent>(request.entity);
				std::cout << "[Inspector] Removed AudioListenerComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "ColliderComponent") {
				ecsManager.RemoveComponent<ColliderComponent>(request.entity);
				std::cout << "[Inspector] Removed ColliderComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "RigidBodyComponent") {
				ecsManager.RemoveComponent<RigidBodyComponent>(request.entity);
				std::cout << "[Inspector] Removed RigidBodyComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "CameraComponent") {
				ecsManager.RemoveComponent<CameraComponent>(request.entity);
				std::cout << "[Inspector] Removed CameraComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "AnimationComponent") {
				ecsManager.RemoveComponent<AnimationComponent>(request.entity);
				std::cout << "[Inspector] Removed AnimationComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "Brain") {
				ecsManager.RemoveComponent<Brain>(request.entity);
				std::cout << "[Inspector] Removed Brain from entity " << request.entity << std::endl;
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

			if (loadedModel->meshes[0].material) {
				modelRenderer.material = loadedModel->meshes[0].material;
				std::string materialPath = AssetManager::GetInstance().GetAssetPathFromAssetName(modelRenderer.material->GetName() + ".mat");
				modelRenderer.materialGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(materialPath);
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