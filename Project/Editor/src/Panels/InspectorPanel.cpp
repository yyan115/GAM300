#include "Panels/InspectorPanel.hpp"
#include "Panels/AssetBrowserPanel.hpp"
#include "EditorComponents.hpp"
#include "imgui.h"
#include "GUIManager.hpp"
#include "SnapshotManager.hpp"
#include "UndoableWidgets.hpp"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include <Graphics/Model/ModelRenderComponent.hpp>
#include <Graphics/Lights/LightComponent.hpp>
#include <Graphics/TextRendering/TextRenderComponent.hpp>
#include <Graphics/Particle/ParticleComponent.hpp>
#include <Graphics/Camera/CameraComponent.hpp>
#include <Graphics/Sprite/SpriteAnimationComponent.hpp>
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
#include "Prefab/PrefabLinkComponent.hpp"
#include <ECS/TagComponent.hpp>
#include <ECS/LayerComponent.hpp>
#include <ECS/TagManager.hpp>
#include <ECS/LayerManager.hpp>
#include "Game AI/BrainComponent.hpp"
#include "Game AI/BrainFactory.hpp"
#include <cstring>
#include <filesystem>
#include <thread>
#include <chrono>
#include <glm/glm.hpp>
#include <FileWatch.hpp>

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
#include "UI/Button/ButtonComponent.hpp"
#include "Sound/AudioReverbZoneComponent.hpp"
#include <Animation/AnimationComponent.hpp>
#include <Animation/AnimationStateMachine.hpp>
#include <Script/ScriptComponentData.hpp>
#include <RunTimeVar.hpp>
#include <Panels/AssetInspector.hpp>
#include "ReflectionRenderer.hpp"
#include "UI/Slider/SliderComponent.hpp"
#include "UI/Anchor/UIAnchorComponent.hpp"
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include "Hierarchy/ParentComponent.hpp"
#include "Hierarchy/ChildrenComponent.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <Panels/PrefabEditorPanel.hpp>
#include <Prefab/PrefabIO.hpp>

// Component clipboard for copy/paste functionality (Unity-style)
namespace {
    struct ComponentClipboard {
        std::string componentType;      // Type name of copied component
        std::string jsonData;           // Serialized component data as JSON string
        bool hasData = false;           // Whether clipboard contains valid data

        void Clear() {
            componentType.clear();
            jsonData.clear();
            hasData = false;
        }
    };

    static ComponentClipboard g_ComponentClipboard;
}

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
		if (UndoableWidgets::Checkbox(label.c_str(), &b)) {
			c.overrideFromPrefab = b;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(Instance)");
	}
}

static inline bool IsPrefabInstance(ECSManager& ecs, Entity e) {
	return ecs.HasComponent<PrefabLinkComponent>(e);
}

std::vector<InspectorPanel::ComponentRemovalRequest> InspectorPanel::pendingComponentRemovals;
std::vector<InspectorPanel::ComponentResetRequest> InspectorPanel::pendingComponentResets;

InspectorPanel::InspectorPanel()
	: EditorPanel("Inspector", true) {
	// Register custom field renderers for special cases
	static bool renderersRegistered = false;
	if (!renderersRegistered) {
		RegisterInspectorCustomRenderers();
		renderersRegistered = true;
	}

	// Initialize file watcher for scripts
	try {
		std::string scriptsFolder = AssetManager::GetInstance().GetRootAssetDirectory() + "/Scripts";
		scriptFileWatcher = std::make_unique<filewatch::FileWatch<std::string>>(scriptsFolder, [this](const std::string& path, const filewatch::Event& event) {
			OnScriptFileChanged(path, event);
		});
	}
	catch (const std::exception& e) {
		std::cerr << "[InspectorPanel] Failed to initialize script file watcher: " << e.what() << std::endl;
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
	}
	catch (const std::exception& e) {
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
		std::function<void* ()> getComponent;
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

		{"Sprite Animation", "SpriteAnimationComponent",
			[&]() { return ecs.HasComponent<SpriteAnimationComponent>(entity) ?
				(void*)&ecs.GetComponent<SpriteAnimationComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<SpriteAnimationComponent>(entity); }},

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

		{"Audio Reverb Zone", "AudioReverbZoneComponent",
			[&]() { return ecs.HasComponent<AudioReverbZoneComponent>(entity) ?
				(void*)&ecs.GetComponent<AudioReverbZoneComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<AudioReverbZoneComponent>(entity); }},

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

		{"RigidBody", "RigidBodyComponent",
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

		{"Brain Component", "BrainComponent",
			[&]() { return ecs.HasComponent<BrainComponent>(entity) ?
				(void*)&ecs.GetComponent<BrainComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<BrainComponent>(entity); }},

		// Script component
		{"Script", "ScriptComponentData",
			[&]() { return ecs.HasComponent<ScriptComponentData>(entity) ?
				(void*)&ecs.GetComponent<ScriptComponentData>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<ScriptComponentData>(entity); }},

		// UI components
		{"Button", "ButtonComponent",
			[&]() { return ecs.HasComponent<ButtonComponent>(entity) ?
				(void*)&ecs.GetComponent<ButtonComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<ButtonComponent>(entity); }},
		{"Slider", "SliderComponent",
			[&]() { return ecs.HasComponent<SliderComponent>(entity) ?
				(void*)&ecs.GetComponent<SliderComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<SliderComponent>(entity); }},
		{"UI Anchor", "UIAnchorComponent",
			[&]() { return ecs.HasComponent<UIAnchorComponent>(entity) ?
				(void*)&ecs.GetComponent<UIAnchorComponent>(entity) : nullptr; },
			[&]() { return ecs.HasComponent<UIAnchorComponent>(entity); }},
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
		}
		else {
			// Normal components get collapsing header
			if (DrawComponentHeaderWithRemoval(info.displayName, entity, info.typeName, componentPtr)) {
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
			}
			else if (lockedAsset.high != 0 || lockedAsset.low != 0) {
				displayAsset = lockedAsset;
			}
		}
		else {
			// Show current selection
			if (selectedAsset.high != 0 || selectedAsset.low != 0) {
				displayAsset = selectedAsset;
			}
			else {
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
						lockedAsset = { 0, 0 };
						displayEntity = GUIManager::GetSelectedEntity();
						displayAsset = GUIManager::GetSelectedAsset();
					}
				}
				catch (...) {
					// If there's any error, unlock
					inspectorLocked = false;
					lockedEntity = static_cast<Entity>(-1);
					lockedAsset = { 0, 0 };
					displayEntity = GUIManager::GetSelectedEntity();
					displayAsset = GUIManager::GetSelectedAsset();
				}
			}
			// Note: Asset validation could be added here if needed
		}

		// Display content
		if (displayAsset.high != 0 || displayAsset.low != 0) {
			DrawSelectedAsset(displayAsset);
		}
		else {
			// Clear cached material when no asset is selected
			if (cachedMaterial) {
				std::cout << "[Inspector] Clearing cached material" << std::endl;
				cachedMaterial.reset();
				cachedMaterialGuid = { 0, 0 };
				cachedMaterialPath.clear();
			}

			// Check for multi-entity selection
			const std::vector<Entity>& selectedEntities = GUIManager::GetSelectedEntities();

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
						}
						else {
							lockedEntity = GUIManager::GetSelectedEntity();
							lockedAsset = { 0, 0 };
						}
					}
					else {
						// Unlock
						lockedEntity = static_cast<Entity>(-1);
						lockedAsset = { 0, 0 };
					}
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(inspectorLocked ? "Unlock Inspector" : "Lock Inspector");
				}

				ImGui::Text("Select an object in the Scene Hierarchy or an asset in the Asset Browser to view its properties");
				if (inspectorLocked) {
					ImGui::Text("Inspector is locked but no valid content is selected.");
				}
			}
			else if (selectedEntities.size() > 1 && !inspectorLocked) {
				// Multi-entity selection mode
				DrawMultiEntityInspector(selectedEntities);
			}
			else {
				try {
					if (!PrefabEditor::IsInPrefabEditorMode()) {
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
								}
								else {
									lockedEntity = GUIManager::GetSelectedEntity();
									lockedAsset = { 0, 0 };
								}
							}
							else {
								// Unlock
								lockedEntity = static_cast<Entity>(-1);
								lockedAsset = { 0, 0 };
							}
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip(inspectorLocked ? "Unlock Inspector" : "Lock Inspector");
						}
						ImGui::Separator();
					}
					else {
						ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
						ImGui::Text("Editing Prefab:");
						ImGui::Separator();
					}

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

					if (!PrefabEditor::IsInPrefabEditorMode()) {
						// Add Component button
						ImGui::Separator();
						DrawAddComponentButton(displayEntity);
					}

				}
				catch (const std::exception& e) {
					ImGui::Text("Error accessing entity: %s", e.what());
				}
			}
		}
	}

	// Process any pending component removals after ImGui rendering is complete
	ProcessPendingComponentRemovals();
	ProcessPendingComponentResets();

	ImGui::End();

	ImGui::PopStyleColor(2);  // Pop WindowBg and ChildBg colors
}

void InspectorPanel::DrawTagComponent(Entity entity) {
	try {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
		if (!ecsManager.HasComponent<TagComponent>(entity)) {
			ecsManager.AddComponent<TagComponent>(entity, TagComponent{ 0 });
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
		if (UndoableWidgets::Combo("##Tag", &currentTag, tagItemPtrs.data(), static_cast<int>(tagItemPtrs.size()))) {
			if (currentTag >= 0 && currentTag < static_cast<int>(availableTags.size())) {
				tagComponent.tagIndex = currentTag;
			}
			else if (currentTag == static_cast<int>(availableTags.size())) {
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
	}
	catch (const std::exception& e) {
		ImGui::Text("Error accessing TagComponent: %s", e.what());
	}
}

void InspectorPanel::DrawLayerComponent(Entity entity) {
	try {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
		if (!ecsManager.HasComponent<LayerComponent>(entity)) {
			ecsManager.AddComponent<LayerComponent>(entity, LayerComponent{ 0 });
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
			}
			else {
				// Default to first item
				currentSelection = 0;
				layerComponent.layerIndex = layerIndices[0];
			}
		}

		// Combo box for layer selection
		ImGui::SetNextItemWidth(120.0f);
		if (UndoableWidgets::Combo("##Layer", &currentSelection, layerItemPtrs.data(), static_cast<int>(layerItemPtrs.size()))) {
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
				}
				else {
					layerComponent.layerIndex = selectedIndex;
				}
			}
		}

		ImGui::PopID();
	}
	catch (const std::exception& e) {
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
		}
		else {
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
				DraggedModelGuid = { 0, 0 };
				DraggedModelPath.clear();
			}
			EditorComponents::EndDragDropTarget();
		}

		if (modelRenderer.shader) {
			ImGui::Text("Shader: Loaded");
		}
		else {
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
		}
		else if (modelRenderer.model && !modelRenderer.model->meshes.empty()) {
			// Show default material from first mesh
			auto& defaultMaterial = modelRenderer.model->meshes[0].material;
			if (defaultMaterial) {
				buttonText = defaultMaterial->GetName() + " (default)";
			}
			else {
				buttonText = "None (Material)";
			}
		}
		else {
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
				}
				else {
					MaterialInspector::ApplyMaterialToModelByPath(entity, DraggedMaterialPath);
				}

				// Clear the drag state
				DraggedMaterialGuid = { 0, 0 };
				DraggedMaterialPath.clear();
			}
			EditorComponents::EndDragDropTarget();
		}

		ImGui::PopID();
	}
	catch (const std::exception& e) {
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
						}
						else {
							lockedEntity = GUIManager::GetSelectedEntity();
							lockedAsset = { 0, 0 };
						}
					}
					else {
						// Unlock
						lockedEntity = static_cast<Entity>(-1);
						lockedAsset = { 0, 0 };
					}
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(inspectorLocked ? "Unlock Inspector" : "Lock Inspector");
				}
				return;
			}
		}
		else {
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
				}
				else {
					cachedMaterial.reset();
					cachedMaterialGuid = { 0, 0 };
					cachedMaterialPath.clear();
					ImGui::Text("Failed to load material");
					return;
				}
			}

			MaterialInspector::DrawMaterialAsset(cachedMaterial, sourceFilePath, true, &inspectorLocked, lockCallback);
		}
		else if (AssetManager::GetInstance().IsAssetExtensionSupported(extension)) {
			std::shared_ptr<AssetMeta> _assetMeta = AssetManager::GetInstance().GetAssetMeta(selectedAsset);
			AssetInspector::DrawAssetMetaInfo(_assetMeta, sourceFilePath, true, &inspectorLocked, lockCallback);
		}
		else {
			ImGui::Text("Asset type not supported for editing in Inspector");
		}

	}
	catch (const std::exception& e) {
		ImGui::Text("Error accessing asset: %s", e.what());
	}
}

void InspectorPanel::DrawAddComponentButton(Entity entity) {
	ImGui::Text("Add Component");

	// Calculate button width - split space if paste is available
	float availWidth = ImGui::GetContentRegionAvail().x;
	bool canPasteComponent = g_ComponentClipboard.hasData && !HasComponent(entity, g_ComponentClipboard.componentType);
	float buttonWidth = canPasteComponent ? (availWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f : availWidth;

	if (ImGui::Button("Add Component", ImVec2(buttonWidth, 30))) {
		ImGui::OpenPopup("AddComponentPopup");
		componentSearchActive = true;
		memset(componentSearchBuffer, 0, sizeof(componentSearchBuffer));
		resetComponentTrees = true;
	}

	// Paste Component button - only show if clipboard has data and entity doesn't have that component
	if (canPasteComponent) {
		ImGui::SameLine();
		if (ImGui::Button("Paste Component", ImVec2(buttonWidth, 30))) {
			// Take snapshot for undo
			SnapshotManager::GetInstance().TakeSnapshot("Paste Component: " + g_ComponentClipboard.componentType);

			// First add the component
			AddComponent(entity, g_ComponentClipboard.componentType);

			// Then paste the values using reflection
			auto& lookup = TypeDescriptor::type_descriptor_lookup();
			auto it = lookup.find(g_ComponentClipboard.componentType);
			if (it != lookup.end()) {
				TypeDescriptor_Struct* typeDesc = dynamic_cast<TypeDescriptor_Struct*>(it->second);
				if (typeDesc) {
					// Get the newly added component pointer
					void* componentPtr = GetComponentPtr(entity, g_ComponentClipboard.componentType);

					if (componentPtr) {
						// Parse JSON from clipboard
						rapidjson::Document doc;
						doc.Parse(g_ComponentClipboard.jsonData.c_str());

						if (!doc.HasParseError() && doc.IsObject()) {
							// Deserialize field by field
							std::vector<TypeDescriptor_Struct::Member> members = typeDesc->GetMembers();
							for (const auto& member : members) {
								if (doc.HasMember(member.name)) {
									void* fieldPtr = member.get_ptr(componentPtr);
									if (fieldPtr && member.type) {
										member.type->Deserialize(fieldPtr, doc[member.name]);
									}
								}
							}

							// Post-paste sync for components with enum/ID field pairs
							ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
							if (g_ComponentClipboard.componentType == "ColliderComponent") {
								auto& collider = ecsManager.GetComponent<ColliderComponent>(entity);
								// Sync enum fields from their serialized ID fields
								collider.shapeType = static_cast<ColliderShapeType>(collider.shapeTypeID);
								collider.layer = static_cast<JPH::ObjectLayer>(collider.layerID);
								collider.version++; // Mark for physics system update
							}
							else if (g_ComponentClipboard.componentType == "RigidBodyComponent") {
								auto& rb = ecsManager.GetComponent<RigidBodyComponent>(entity);
								rb.motion = static_cast<Motion>(rb.motionID);
							}
							else if (g_ComponentClipboard.componentType == "ScriptComponentData") {
								auto& scriptComp = ecsManager.GetComponent<ScriptComponentData>(entity);
								// Resolve script paths and GUIDs for each script
								for (auto& script : scriptComp.scripts) {
									bool guidIsZero = (script.scriptGuid.high == 0 && script.scriptGuid.low == 0);
									// If scriptGuidStr is set but scriptGuid is empty, convert it
									if (!script.scriptGuidStr.empty() && guidIsZero) {
										script.scriptGuid = GUIDUtilities::ConvertStringToGUID128(script.scriptGuidStr);
										guidIsZero = false;
									}
									// If scriptPath is empty but we have a GUID, resolve the path
									if (script.scriptPath.empty() && !guidIsZero) {
										script.scriptPath = AssetManager::GetInstance().GetAssetPathFromGUID(script.scriptGuid);
									}
									// Reset runtime state for the new entity
									script.instanceId = -1;
									script.instanceCreated = false;
								}
							}
						}
					}
				}
			}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Paste %s from clipboard", g_ComponentClipboard.componentType.c_str());
		}
	}

	ImGui::SetNextWindowSize(ImVec2(ImGui::GetItemRectSize().x, ImGui::GetContentRegionAvail().y), ImGuiCond_Appearing);

	if (ImGui::BeginPopup("AddComponentPopup")) {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

			struct ComponentEntry {
				std::string displayName;
				std::string componentType;
				std::string category;
				bool isScript = false;
				std::string scriptPath;
			};

			// Static category icons
			static const std::unordered_map<std::string, std::string> categoryIcons = {
				{"Rendering", ICON_FA_CUBE},
				{"Audio", ICON_FA_HEADPHONES},
				{"Lighting", ICON_FA_LIGHTBULB},
				{"Camera", ICON_FA_CAMERA},
				{"Physics", ICON_FA_CUBES},
				{"Animation", ICON_FA_PLAY},
				{"AI", ICON_FA_BRAIN},
				{"Scripting", ICON_FA_CODE},
				{"General", ICON_FA_TAG},
				{"Scripts", ICON_FA_FILE_CODE},
				{"UI", ICON_FA_HAND_POINTER}
			};

			std::vector<ComponentEntry> allComponents;

			if (!ecsManager.HasComponent<ModelRenderComponent>(entity)) {
				allComponents.push_back({ "Model Renderer", "ModelRenderComponent", "Rendering" });
			}
			if (!ecsManager.HasComponent<SpriteRenderComponent>(entity)) {
				allComponents.push_back({ "Sprite Renderer", "SpriteRenderComponent", "Rendering" });
			}
			if (!ecsManager.HasComponent<SpriteAnimationComponent>(entity)) {
				allComponents.push_back({"Sprite Animation", "SpriteAnimationComponent", "Rendering"});
			}
			if (!ecsManager.HasComponent<TextRenderComponent>(entity)) {
				allComponents.push_back({ "Text Renderer", "TextRenderComponent", "Rendering" });
			}
			if (!ecsManager.HasComponent<ParticleComponent>(entity)) {
				allComponents.push_back({ "Particle System", "ParticleComponent", "Rendering" });
			}
			if (!ecsManager.HasComponent<AudioComponent>(entity)) {
				allComponents.push_back({ "Audio Source", "AudioComponent", "Audio" });
			}
			if (!ecsManager.HasComponent<AudioListenerComponent>(entity)) {
				allComponents.push_back({ "Audio Listener", "AudioListenerComponent", "Audio" });
			}
			if (!ecsManager.HasComponent<AudioReverbZoneComponent>(entity)) {
				allComponents.push_back({ "Audio Reverb Zone", "AudioReverbZoneComponent", "Audio" });
			}
			if (!ecsManager.HasComponent<DirectionalLightComponent>(entity)) {
				allComponents.push_back({ "Directional Light", "DirectionalLightComponent", "Lighting" });
			}
			if (!ecsManager.HasComponent<PointLightComponent>(entity)) {
				allComponents.push_back({ "Point Light", "PointLightComponent", "Lighting" });
			}
			if (!ecsManager.HasComponent<SpotLightComponent>(entity)) {
				allComponents.push_back({ "Spot Light", "SpotLightComponent", "Lighting" });
			}
			if (!ecsManager.HasComponent<CameraComponent>(entity)) {
				allComponents.push_back({ "Camera", "CameraComponent", "Camera" });
			}
			if (!ecsManager.HasComponent<ColliderComponent>(entity)) {
				allComponents.push_back({ "Collider", "ColliderComponent", "Physics" });
			}
			if (!ecsManager.HasComponent<RigidBodyComponent>(entity)) {
				allComponents.push_back({ "RigidBody", "RigidBodyComponent", "Physics" });
			}
			if (!ecsManager.HasComponent<AnimationComponent>(entity)) {
				allComponents.push_back({ "Animation Component", "AnimationComponent", "Animation" });
			}
			if (!ecsManager.HasComponent<BrainComponent>(entity)) {
				allComponents.push_back({ "Brain", "Brain", "AI" });
			}
			if (!ecsManager.HasComponent<ScriptComponentData>(entity)) {
				allComponents.push_back({ "Script", "ScriptComponentData", "Scripting" });
			}
			if (!ecsManager.HasComponent<TagComponent>(entity)) {
				allComponents.push_back({ "Tag", "TagComponent", "General" });
			}
			if (!ecsManager.HasComponent<LayerComponent>(entity)) {
				allComponents.push_back({ "Layer", "LayerComponent", "General" });
			}
			if (!ecsManager.HasComponent<ButtonComponent>(entity)) {
				allComponents.push_back({ "Button", "ButtonComponent", "UI" });
			}
			if (!ecsManager.HasComponent<SliderComponent>(entity)) {
				allComponents.push_back({ "Slider", "SliderComponent", "UI" });
			}
			if (!ecsManager.HasComponent<UIAnchorComponent>(entity)) {
				allComponents.push_back({ "UI Anchor", "UIAnchorComponent", "UI" });
			}

		// Cache scripts to avoid filesystem scanning every frame
		std::string scriptsFolder = AssetManager::GetInstance().GetRootAssetDirectory() + "/Scripts";
		if (cachedScripts.empty()) {
			cachedScripts.clear();
			if (std::filesystem::exists(scriptsFolder)) {
				for (const auto& entry : std::filesystem::recursive_directory_iterator(scriptsFolder)) {
					if (entry.is_regular_file() && entry.path().extension() == ".lua") {
						cachedScripts.push_back(entry.path().generic_string());
					}
				}
			}
		}		for (const auto& scriptPath : cachedScripts) {
			std::string scriptName = std::filesystem::path(scriptPath).stem().string();
			allComponents.push_back(ComponentEntry{scriptName, "", "Scripts", true, scriptPath});
		}			ImGui::SetNextItemWidth(-1);
			if (componentSearchActive) {
				ImGui::SetKeyboardFocusHere();
				componentSearchActive = false;
			}
			ImGui::InputTextWithHint("##ComponentSearch", "Search", componentSearchBuffer, sizeof(componentSearchBuffer));

			ImGui::Separator();

			ImGui::BeginChild("ComponentList", ImVec2(0, 300), true);

			std::string searchStr = componentSearchBuffer;
			std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

			bool isSearching = !searchStr.empty();

			if (isSearching) {
				std::vector<ComponentEntry> filteredComponents;
				for (const auto& comp : allComponents) {
					std::string displayNameLower = comp.displayName;
					std::transform(displayNameLower.begin(), displayNameLower.end(), displayNameLower.begin(), ::tolower);

					std::string categoryLower = comp.category;
					std::transform(categoryLower.begin(), categoryLower.end(), categoryLower.begin(), ::tolower);

					if (displayNameLower.find(searchStr) != std::string::npos ||
						categoryLower.find(searchStr) != std::string::npos) {
						filteredComponents.push_back(comp);
					}
				}

				bool hasScriptResults = false;
				for (const auto& comp : filteredComponents) {
					if (comp.category == "Scripts") {
						hasScriptResults = true;
						break;
					}
				}

				if (hasScriptResults && searchStr.length() >= 2) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
					if (ImGui::Selectable((std::string(ICON_FA_PLUS) + " New script").c_str())) {
						ImGui::PopStyleColor();
						ImGui::CloseCurrentPopup();
					}
					else {
						ImGui::PopStyleColor();
					}
				}

				if (filteredComponents.empty()) {
					ImGui::TextDisabled("No results found");
				}
				else {
					for (const auto& comp : filteredComponents) {
						std::string icon = categoryIcons.at(comp.category);
						if (ImGui::Selectable((icon + " " + comp.displayName).c_str())) {
							if (comp.isScript) {
								if (!ecsManager.HasComponent<ScriptComponentData>(entity)) {
									AddComponent(entity, "ScriptComponentData");
								}

								auto& scriptComp = ecsManager.GetComponent<ScriptComponentData>(entity);

								// Add new script to the scripts vector
								ScriptData newScript{};
								newScript.scriptGuid = AssetManager::GetInstance().GetGUID128FromAssetMeta(comp.scriptPath);
								newScript.scriptGuidStr = GUIDUtilities::ConvertGUID128ToString(newScript.scriptGuid);
								newScript.scriptPath = comp.scriptPath;
								newScript.instanceCreated = false;
								newScript.instanceId = -1;
								scriptComp.scripts.push_back(newScript);

								SnapshotManager::GetInstance().TakeSnapshot("Add Script: " + comp.displayName);
							} else {
								AddComponent(entity, comp.componentType);
							}

							ImGui::CloseCurrentPopup();
						}
					}
				}
			}
			else {
				std::unordered_map<std::string, std::vector<ComponentEntry>> categorizedComponents;
				for (const auto& comp : allComponents) {
					categorizedComponents[comp.category].push_back(comp);
				}

				std::vector<std::string> categoryOrder = {
					"Rendering", "Audio", "Lighting", "Camera", "Physics",
					"Animation", "AI", "Scripting", "General", "Scripts", "UI"
				};

				for (const auto& category : categoryOrder) {
					auto it = categorizedComponents.find(category);
					if (it == categorizedComponents.end() || it->second.empty()) {
						continue;
					}

					if (resetComponentTrees) {
						ImGui::SetNextItemOpen(false);
					}

					std::string catIcon = categoryIcons.at(category);
					if (ImGui::TreeNode((catIcon + " " + category).c_str())) {
						for (const auto& comp : it->second) {
							std::string compIcon = categoryIcons.at(comp.category);
							if (ImGui::Selectable((compIcon + " " + comp.displayName).c_str())) {
								if (comp.isScript) {
									if (!ecsManager.HasComponent<ScriptComponentData>(entity)) {
										AddComponent(entity, "ScriptComponentData");
									}

									auto& scriptComp = ecsManager.GetComponent<ScriptComponentData>(entity);

									// Add new script to the scripts vector
									ScriptData newScript{};
									newScript.scriptGuid = AssetManager::GetInstance().GetGUID128FromAssetMeta(comp.scriptPath);
									newScript.scriptGuidStr = GUIDUtilities::ConvertGUID128ToString(newScript.scriptGuid);
									newScript.scriptPath = comp.scriptPath;
									newScript.instanceCreated = false;
									newScript.instanceId = -1;
									scriptComp.scripts.push_back(newScript);

									SnapshotManager::GetInstance().TakeSnapshot("Add Script: " + comp.displayName);
								} else {
									AddComponent(entity, comp.componentType);
								}

								ImGui::CloseCurrentPopup();
							}
						}
						ImGui::TreePop();
					}
				}

				if (resetComponentTrees) {
					resetComponentTrees = false;
				}
			}

			ImGui::EndChild();

		ImGui::EndPopup();
	}
}

void InspectorPanel::DrawAddComponentButtonMulti(const std::vector<Entity>& entities) {
	if (entities.empty()) return;

	ImGui::Text("Add Component to All");
	ImGui::SameLine();
	ImGui::TextDisabled("(%zu entities)", entities.size());

	float availWidth = ImGui::GetContentRegionAvail().x;

	if (ImGui::Button("Add Component to All", ImVec2(availWidth, 30))) {
		ImGui::OpenPopup("AddComponentMultiPopup");
		componentSearchActive = true;
		memset(componentSearchBuffer, 0, sizeof(componentSearchBuffer));
		resetComponentTrees = true;
	}

	ImGui::SetNextWindowSize(ImVec2(ImGui::GetItemRectSize().x, ImGui::GetContentRegionAvail().y), ImGuiCond_Appearing);

	if (ImGui::BeginPopup("AddComponentMultiPopup")) {
		ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

		struct ComponentEntry {
			std::string displayName;
			std::string componentType;
			std::string category;
			bool isScript = false;
			std::string scriptPath;
		};

		// Static category icons
		static const std::unordered_map<std::string, std::string> categoryIcons = {
			{"Rendering", ICON_FA_CUBE},
			{"Audio", ICON_FA_HEADPHONES},
			{"Lighting", ICON_FA_LIGHTBULB},
			{"Camera", ICON_FA_CAMERA},
			{"Physics", ICON_FA_CUBES},
			{"Animation", ICON_FA_PLAY},
			{"AI", ICON_FA_BRAIN},
			{"Scripting", ICON_FA_CODE},
			{"General", ICON_FA_TAG},
			{"Scripts", ICON_FA_FILE_CODE},
			{"UI", ICON_FA_HAND_POINTER}
		};

		std::vector<ComponentEntry> allComponents;

		// Helper lambda to check if ALL entities lack a component
		auto allEntitiesLackComponent = [&](auto hasComponentFunc) {
			for (const auto& entity : entities) {
				if (hasComponentFunc(entity)) return false;
			}
			return true;
		};

		// Only show components that NONE of the entities have
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<ModelRenderComponent>(e); })) {
			allComponents.push_back({ "Model Renderer", "ModelRenderComponent", "Rendering" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<SpriteRenderComponent>(e); })) {
			allComponents.push_back({ "Sprite Renderer", "SpriteRenderComponent", "Rendering" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<SpriteAnimationComponent>(e); })) {
			allComponents.push_back({"Sprite Animation", "SpriteAnimationComponent", "Rendering"});
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<TextRenderComponent>(e); })) {
			allComponents.push_back({ "Text Renderer", "TextRenderComponent", "Rendering" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<ParticleComponent>(e); })) {
			allComponents.push_back({ "Particle System", "ParticleComponent", "Rendering" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<AudioComponent>(e); })) {
			allComponents.push_back({ "Audio Source", "AudioComponent", "Audio" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<AudioListenerComponent>(e); })) {
			allComponents.push_back({ "Audio Listener", "AudioListenerComponent", "Audio" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<AudioReverbZoneComponent>(e); })) {
			allComponents.push_back({ "Audio Reverb Zone", "AudioReverbZoneComponent", "Audio" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<DirectionalLightComponent>(e); })) {
			allComponents.push_back({ "Directional Light", "DirectionalLightComponent", "Lighting" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<PointLightComponent>(e); })) {
			allComponents.push_back({ "Point Light", "PointLightComponent", "Lighting" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<SpotLightComponent>(e); })) {
			allComponents.push_back({ "Spot Light", "SpotLightComponent", "Lighting" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<CameraComponent>(e); })) {
			allComponents.push_back({ "Camera", "CameraComponent", "Camera" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<ColliderComponent>(e); })) {
			allComponents.push_back({ "Collider", "ColliderComponent", "Physics" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<RigidBodyComponent>(e); })) {
			allComponents.push_back({ "RigidBody", "RigidBodyComponent", "Physics" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<AnimationComponent>(e); })) {
			allComponents.push_back({ "Animation Component", "AnimationComponent", "Animation" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<BrainComponent>(e); })) {
			allComponents.push_back({ "Brain", "Brain", "AI" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<ScriptComponentData>(e); })) {
			allComponents.push_back({ "Script", "ScriptComponentData", "Scripting" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<TagComponent>(e); })) {
			allComponents.push_back({ "Tag", "TagComponent", "General" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<LayerComponent>(e); })) {
			allComponents.push_back({ "Layer", "LayerComponent", "General" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<ButtonComponent>(e); })) {
			allComponents.push_back({ "Button", "ButtonComponent", "UI" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<SliderComponent>(e); })) {
			allComponents.push_back({ "Slider", "SliderComponent", "UI" });
		}
		if (allEntitiesLackComponent([&](Entity e) { return ecsManager.HasComponent<UIAnchorComponent>(e); })) {
			allComponents.push_back({ "UI Anchor", "UIAnchorComponent", "UI" });
		}

		// Cache scripts
		std::string scriptsFolder = AssetManager::GetInstance().GetRootAssetDirectory() + "/Scripts";
		if (cachedScripts.empty()) {
			cachedScripts.clear();
			if (std::filesystem::exists(scriptsFolder)) {
				for (const auto& entry : std::filesystem::recursive_directory_iterator(scriptsFolder)) {
					if (entry.is_regular_file() && entry.path().extension() == ".lua") {
						cachedScripts.push_back(entry.path().generic_string());
					}
				}
			}
		}
		for (const auto& scriptPath : cachedScripts) {
			std::string scriptName = std::filesystem::path(scriptPath).stem().string();
			allComponents.push_back(ComponentEntry{scriptName, "", "Scripts", true, scriptPath});
		}

		ImGui::SetNextItemWidth(-1);
		if (componentSearchActive) {
			ImGui::SetKeyboardFocusHere();
			componentSearchActive = false;
		}
		ImGui::InputTextWithHint("##ComponentSearchMulti", "Search", componentSearchBuffer, sizeof(componentSearchBuffer));

		ImGui::Separator();

		ImGui::BeginChild("ComponentListMulti", ImVec2(0, 300), true);

		std::string searchStr = componentSearchBuffer;
		std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

		bool isSearching = !searchStr.empty();

		// Lambda to add component to all entities
		auto addComponentToAll = [&](const ComponentEntry& comp) {
			SnapshotManager::GetInstance().TakeSnapshot("Add " + comp.displayName + " to " + std::to_string(entities.size()) + " entities");

			for (const auto& entity : entities) {
				if (comp.isScript) {
					if (!ecsManager.HasComponent<ScriptComponentData>(entity)) {
						AddComponent(entity, "ScriptComponentData");
					}
					auto& scriptComp = ecsManager.GetComponent<ScriptComponentData>(entity);
					ScriptData newScript{};
					newScript.scriptGuid = AssetManager::GetInstance().GetGUID128FromAssetMeta(comp.scriptPath);
					newScript.scriptGuidStr = GUIDUtilities::ConvertGUID128ToString(newScript.scriptGuid);
					newScript.scriptPath = comp.scriptPath;
					newScript.instanceCreated = false;
					newScript.instanceId = -1;
					scriptComp.scripts.push_back(newScript);
				} else {
					if (!HasComponent(entity, comp.componentType)) {
						AddComponent(entity, comp.componentType);
					}
				}
			}
			std::cout << "[Inspector] Added " << comp.displayName << " to " << entities.size() << " entities" << std::endl;
		};

		if (isSearching) {
			std::vector<ComponentEntry> filteredComponents;
			for (const auto& comp : allComponents) {
				std::string displayNameLower = comp.displayName;
				std::transform(displayNameLower.begin(), displayNameLower.end(), displayNameLower.begin(), ::tolower);

				std::string categoryLower = comp.category;
				std::transform(categoryLower.begin(), categoryLower.end(), categoryLower.begin(), ::tolower);

				if (displayNameLower.find(searchStr) != std::string::npos ||
					categoryLower.find(searchStr) != std::string::npos) {
					filteredComponents.push_back(comp);
				}
			}

			if (filteredComponents.empty()) {
				ImGui::TextDisabled("No results found");
			}
			else {
				for (const auto& comp : filteredComponents) {
					std::string icon = categoryIcons.at(comp.category);
					if (ImGui::Selectable((icon + " " + comp.displayName).c_str())) {
						addComponentToAll(comp);
						ImGui::CloseCurrentPopup();
					}
				}
			}
		}
		else {
			std::unordered_map<std::string, std::vector<ComponentEntry>> categorizedComponents;
			for (const auto& comp : allComponents) {
				categorizedComponents[comp.category].push_back(comp);
			}

			std::vector<std::string> categoryOrder = {
				"Rendering", "Audio", "Lighting", "Camera", "Physics",
				"Animation", "AI", "Scripting", "General", "Scripts", "UI"
			};

			for (const auto& category : categoryOrder) {
				auto it = categorizedComponents.find(category);
				if (it == categorizedComponents.end() || it->second.empty()) {
					continue;
				}

				if (resetComponentTrees) {
					ImGui::SetNextItemOpen(false);
				}

				std::string catIcon = categoryIcons.at(category);
				if (ImGui::TreeNode((catIcon + " " + category).c_str())) {
					for (const auto& comp : it->second) {
						std::string compIcon = categoryIcons.at(comp.category);
						if (ImGui::Selectable((compIcon + " " + comp.displayName).c_str())) {
							addComponentToAll(comp);
							ImGui::CloseCurrentPopup();
						}
					}
					ImGui::TreePop();
				}
			}

			if (resetComponentTrees) {
				resetComponentTrees = false;
			}
		}

		ImGui::EndChild();

		ImGui::EndPopup();
	}
}

void InspectorPanel::AddComponent(Entity entity, const std::string& componentType) {
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
			}
			else {
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
		else if (componentType == "AudioReverbZoneComponent") {
			AudioReverbZoneComponent component;
			ecsManager.AddComponent<AudioReverbZoneComponent>(entity, component);
			std::cout << "[Inspector] Added AudioReverbZoneComponent to entity " << entity << std::endl;
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
		else if (componentType == "SpriteAnimationComponent") {
			SpriteAnimationComponent component;
			// Initialize with some default values
			component.playbackSpeed = 1.0f;
			component.playing = false;
			component.currentClipIndex = -1;
			component.currentFrameIndex = 0;
			component.timeInCurrentFrame = 0.0f;

			ecsManager.AddComponent<SpriteAnimationComponent>(entity, component);
			std::cout << "[Inspector] Added SpriteAnimationComponent to entity " << entity << std::endl;
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

			// Load font and shader resources
			if (std::filesystem::exists(defaultFontPath)) {
				component.font = ResourceManager::GetInstance().GetFontResource(defaultFontPath);
			}
			else {
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
				if (rc.model)
				{
					component.boxHalfExtents = rc.CalculateModelHalfExtent(*rc.model);	//no need apply local scale
					//component.center = rc.CalculateCenter(*rc.model);
				}
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
			BrainComponent component;
			ecsManager.AddComponent<BrainComponent>(entity, component);
			std::cout << "[Inspector] Added Brain to entity " << entity << std::endl;
		}
		else if (componentType == "ScriptComponentData") {
			ScriptComponentData component;
			// Default values are set in the struct definition
			// scriptPath is empty, enabled is true, etc.
			ecsManager.AddComponent<ScriptComponentData>(entity, component);
			std::cout << "[Inspector] Added ScriptComponentData to entity " << entity << " (ready for script assignment)" << std::endl;
		}
		else if (componentType == "ButtonComponent") {
			ButtonComponent component;
			component.interactable = true;
			ecsManager.AddComponent<ButtonComponent>(entity, component);
			std::cout << "[Inspector] Added ButtonComponent to entity " << entity << std::endl;
		}
		else if (componentType == "SliderComponent") {
			// Ensure entity has Transform component for UI positioning
			if (!ecsManager.HasComponent<Transform>(entity)) {
				Transform transform;
				transform.localScale = Vector3D(200.0f, 20.0f, 1.0f);
				ecsManager.AddComponent<Transform>(entity, transform);
				std::cout << "[Inspector] Added Transform component for Slider" << std::endl;
			}

			SliderComponent component;
			component.minValue = 0.0f;
			component.maxValue = 1.0f;
			component.value = 0.0f;
			component.interactable = true;
			component.horizontal = true;
			component.wholeNumbers = false;

			auto createSliderSpriteChild = [&](const std::string& childName, int sortingOrder) -> Entity {
				Entity child = ecsManager.CreateEntity();
				ecsManager.GetComponent<NameComponent>(child).name = childName;

				GUID_128 spriteShaderGUID = MetaFilesManager::GetGUID128FromAssetFile(ResourceManager::GetPlatformShaderPath("sprite"));
				std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(spriteShaderGUID);
				auto shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(spriteShaderGUID, shaderPath);

				SpriteRenderComponent sprite;
				sprite.shader = shader;
				sprite.shaderGUID = spriteShaderGUID;
				sprite.texture = nullptr;
				sprite.is3D = false;
				sprite.isVisible = true;
				sprite.sortingOrder = sortingOrder;
				ecsManager.AddComponent<SpriteRenderComponent>(child, sprite);

				return child;
			};

			auto attachChild = [&](Entity parent, Entity child) {
				auto& guidRegistry = EntityGUIDRegistry::GetInstance();
				GUID_128 parentGuid = guidRegistry.GetGUIDByEntity(parent);
				GUID_128 childGuid = guidRegistry.GetGUIDByEntity(child);

				if (!ecsManager.HasComponent<ParentComponent>(child)) {
					ecsManager.AddComponent<ParentComponent>(child, ParentComponent{ parentGuid });
				}

				if (!ecsManager.HasComponent<ChildrenComponent>(parent)) {
					ecsManager.AddComponent<ChildrenComponent>(parent, ChildrenComponent{});
				}

				auto& children = ecsManager.GetComponent<ChildrenComponent>(parent).children;
				if (std::find(children.begin(), children.end(), childGuid) == children.end()) {
					children.push_back(childGuid);
				}
			};

			Entity trackEntity = createSliderSpriteChild("Slider_Track", 0);
			Entity handleEntity = createSliderSpriteChild("Slider_Handle", 1);

			component.trackEntityGuid = EntityGUIDRegistry::GetInstance().GetGUIDByEntity(trackEntity);
			component.handleEntityGuid = EntityGUIDRegistry::GetInstance().GetGUIDByEntity(handleEntity);

			attachChild(entity, trackEntity);
			attachChild(entity, handleEntity);

			// Set default transforms for children
			if (ecsManager.HasComponent<Transform>(entity)) {
				auto& parentTransform = ecsManager.GetComponent<Transform>(entity);
				auto& trackTransform = ecsManager.GetComponent<Transform>(trackEntity);
				auto& handleTransform = ecsManager.GetComponent<Transform>(handleEntity);

				trackTransform.localPosition = Vector3D(0.0f, 0.0f, 0.0f);
				trackTransform.localScale = parentTransform.localScale;
				trackTransform.isDirty = true;

				float handleSize = std::max(10.0f, parentTransform.localScale.y);
				handleTransform.localPosition = Vector3D(0.0f, 0.0f, 0.0f);
				handleTransform.localScale = Vector3D(handleSize, handleSize, 1.0f);
				handleTransform.isDirty = true;
			}

			ecsManager.AddComponent<SliderComponent>(entity, component);
			std::cout << "[Inspector] Added SliderComponent to entity " << entity << std::endl;
		}
		else if (componentType == "UIAnchorComponent") {
			UIAnchorComponent component;
			// Default to center anchor
			component.anchorX = 0.5f;
			component.anchorY = 0.5f;
			component.offsetX = 0.0f;
			component.offsetY = 0.0f;
			component.sizeMode = UISizeMode::Fixed;

			ecsManager.AddComponent<UIAnchorComponent>(entity, component);

			// Ensure entity has a Transform component for positioning
			if (!ecsManager.HasComponent<Transform>(entity)) {
				Transform transform;
				ecsManager.AddComponent<Transform>(entity, transform);
				std::cout << "[Inspector] Added Transform component for UI Anchor" << std::endl;
			}

			std::cout << "[Inspector] Added UIAnchorComponent to entity " << entity << std::endl;
		}
		else {
			std::cerr << "[Inspector] Unknown component type: " << componentType << std::endl;
		}

	// Take snapshot after adding component (for undo)
	SnapshotManager::GetInstance().TakeSnapshot("Add Component: " + componentType);
}

void InspectorPanel::OnScriptFileChanged(const std::string& path, const filewatch::Event& event) {
	// Commented out to fix warning C4100 - unreferenced parameter
	// Remove this line when 'path' is used
	(void)path;

	// Invalidate script cache on any change
	if (event == filewatch::Event::added || event == filewatch::Event::removed ||
		event == filewatch::Event::renamed_old || event == filewatch::Event::renamed_new ||
		event == filewatch::Event::modified) {
		cachedScripts.clear();
	}
}

bool InspectorPanel::DrawComponentHeaderWithRemoval(const char* label, Entity entity, const std::string& componentType, void* componentPtr, ImGuiTreeNodeFlags flags) {

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
		// Component enable/disable checkbox
		// Get reference to actual component's enabled/isActive/isVisible field
		ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
		bool* enabledFieldPtr = nullptr;

		// Get the appropriate enabled field for each component type
		if (componentType == "CameraComponent") {
			auto& comp = ecs.GetComponent<CameraComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		}
		else if (componentType == "DirectionalLightComponent") {
			auto& comp = ecs.GetComponent<DirectionalLightComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		}
		else if (componentType == "PointLightComponent") {
			auto& comp = ecs.GetComponent<PointLightComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		}
		else if (componentType == "SpotLightComponent") {
			auto& comp = ecs.GetComponent<SpotLightComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		}
		else if (componentType == "ModelRenderComponent") {
			auto& comp = ecs.GetComponent<ModelRenderComponent>(entity);
			enabledFieldPtr = &comp.isVisible;
		}
		else if (componentType == "SpriteRenderComponent") {
			auto& comp = ecs.GetComponent<SpriteRenderComponent>(entity);
			enabledFieldPtr = &comp.isVisible;
		}
		else if (componentType == "TextRenderComponent") {
			auto& comp = ecs.GetComponent<TextRenderComponent>(entity);
			enabledFieldPtr = &comp.isVisible;
		}
		else if (componentType == "ParticleComponent") {
			auto& comp = ecs.GetComponent<ParticleComponent>(entity);
			enabledFieldPtr = &comp.isVisible;
		}
		else if (componentType == "AudioComponent") {
			auto& comp = ecs.GetComponent<AudioComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		}
		else if (componentType == "AudioListenerComponent") {
			auto& comp = ecs.GetComponent<AudioListenerComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		}
		else if (componentType == "ColliderComponent") {
			auto& comp = ecs.GetComponent<ColliderComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		}
		else if (componentType == "RigidBodyComponent") {
			auto& comp = ecs.GetComponent<RigidBodyComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		}
		else if (componentType == "AnimationComponent") {
			auto& comp = ecs.GetComponent<AnimationComponent>(entity);
			enabledFieldPtr = &comp.enabled;
		} else if (componentType == "SpriteAnimationComponent") {
			auto& comp = ecs.GetComponent<SpriteAnimationComponent>(entity);
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
			UndoableWidgets::Checkbox(checkboxId.c_str(), enabledFieldPtr);
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

	// Context menu for component operations
	std::string popupName = "ComponentContextMenu_" + componentType;
	if (ImGui::BeginPopup(popupName.c_str())) {
		if (ImGui::MenuItem("Remove Component")) {
			// Queue the component removal for processing after ImGui rendering is complete
			pendingComponentRemovals.push_back({ entity, componentType });
		}
		if (ImGui::MenuItem("Reset Component")) {
			// Queue the component reset for processing after ImGui rendering is complete
			pendingComponentResets.push_back({ entity, componentType });
		}

		ImGui::Separator();

		// Copy Component - serialize to clipboard
		if (ImGui::MenuItem("Copy Component")) {
			if (componentPtr) {
				// Get type descriptor from reflection system
				auto& lookup = TypeDescriptor::type_descriptor_lookup();
				auto it = lookup.find(componentType);
				if (it != lookup.end()) {
					TypeDescriptor_Struct* typeDesc = dynamic_cast<TypeDescriptor_Struct*>(it->second);
					if (typeDesc) {
						// Serialize component to JSON
						rapidjson::Document doc;
						doc.SetObject();
						auto& allocator = doc.GetAllocator();

						// Serialize each member
						std::vector<TypeDescriptor_Struct::Member> members = typeDesc->GetMembers();
						for (const auto& member : members) {
							void* fieldPtr = member.get_ptr(componentPtr);
							if (fieldPtr && member.type) {
								rapidjson::Document fieldDoc;
								member.type->SerializeJson(fieldPtr, fieldDoc);
								rapidjson::Value key(member.name, allocator);
								rapidjson::Value val(fieldDoc, allocator);
								doc.AddMember(key, val, allocator);
							}
						}

						// Convert to string
						rapidjson::StringBuffer buffer;
						rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
						doc.Accept(writer);

						// Store in clipboard
						g_ComponentClipboard.componentType = componentType;
						g_ComponentClipboard.jsonData = buffer.GetString();
						g_ComponentClipboard.hasData = true;
					}
				}
			}
		}

		// Paste Component Values - only enabled if clipboard has same component type
		bool canPaste = g_ComponentClipboard.hasData && g_ComponentClipboard.componentType == componentType;
		if (ImGui::MenuItem("Paste Component Values", nullptr, false, canPaste)) {
			if (componentPtr && canPaste) {
				// Take snapshot for undo
				SnapshotManager::GetInstance().TakeSnapshot("Paste Component Values");

				// Get type descriptor
				auto& lookup = TypeDescriptor::type_descriptor_lookup();
				auto it = lookup.find(componentType);
				if (it != lookup.end()) {
					TypeDescriptor_Struct* typeDesc = dynamic_cast<TypeDescriptor_Struct*>(it->second);
					if (typeDesc) {
						// Parse JSON from clipboard
						rapidjson::Document doc;
						doc.Parse(g_ComponentClipboard.jsonData.c_str());

						if (!doc.HasParseError() && doc.IsObject()) {
							// Deserialize field by field (matching how we serialized)
							std::vector<TypeDescriptor_Struct::Member> members = typeDesc->GetMembers();
							for (const auto& member : members) {
								if (doc.HasMember(member.name)) {
									void* fieldPtr = member.get_ptr(componentPtr);
									if (fieldPtr && member.type) {
										member.type->Deserialize(fieldPtr, doc[member.name]);
									}
								}
							}

							// Post-paste sync for components with enum/ID field pairs
							if (componentType == "ColliderComponent") {
								auto* collider = static_cast<ColliderComponent*>(componentPtr);
								// Sync enum fields from their serialized ID fields
								collider->shapeType = static_cast<ColliderShapeType>(collider->shapeTypeID);
								collider->layer = static_cast<JPH::ObjectLayer>(collider->layerID);
								collider->version++; // Mark for physics system update
							}
							else if (componentType == "RigidBodyComponent") {
								auto* rb = static_cast<RigidBodyComponent*>(componentPtr);
								rb->motion = static_cast<Motion>(rb->motionID);
							}
							else if (componentType == "ScriptComponentData") {
								auto* scriptComp = static_cast<ScriptComponentData*>(componentPtr);
								// Resolve script paths and GUIDs for each script
								for (auto& script : scriptComp->scripts) {
									bool guidIsZero = (script.scriptGuid.high == 0 && script.scriptGuid.low == 0);
									// If scriptGuidStr is set but scriptGuid is empty, convert it
									if (!script.scriptGuidStr.empty() && guidIsZero) {
										script.scriptGuid = GUIDUtilities::ConvertStringToGUID128(script.scriptGuidStr);
										guidIsZero = false;
									}
									// If scriptPath is empty but we have a GUID, resolve the path
									if (script.scriptPath.empty() && !guidIsZero) {
										script.scriptPath = AssetManager::GetInstance().GetAssetPathFromGUID(script.scriptGuid);
									}
									// Reset runtime state
									script.instanceId = -1;
									script.instanceCreated = false;
								}
							}
						}
					}
				}
			}
		}

		// Show what's in clipboard as tooltip
		if (ImGui::IsItemHovered() && g_ComponentClipboard.hasData) {
			ImGui::SetTooltip("Clipboard: %s", g_ComponentClipboard.componentType.c_str());
		}

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
			else if (request.componentType == "AudioReverbZoneComponent") {
				ecsManager.RemoveComponent<AudioReverbZoneComponent>(request.entity);
				std::cout << "[Inspector] Removed AudioReverbZoneComponent from entity " << request.entity << std::endl;
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
				ecsManager.GetComponent<ModelRenderComponent>(request.entity).SetAnimator(nullptr);
				std::cout << "[Inspector] Removed AnimationComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "Brain") {
				ecsManager.RemoveComponent<BrainComponent>(request.entity);
				std::cout << "[Inspector] Removed Brain from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "ScriptComponentData") {
				ecsManager.RemoveComponent<ScriptComponentData>(request.entity);
				std::cout << "[Inspector] Removed ScriptComponentData from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "ButtonComponent") {
				ecsManager.RemoveComponent<ButtonComponent>(request.entity);
				std::cout << "[Inspector] Removed ButtonComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "SliderComponent") {
				if (ecsManager.HasComponent<SliderComponent>(request.entity)) {
					auto& sliderComp = ecsManager.GetComponent<SliderComponent>(request.entity);
					Entity trackEntity = static_cast<Entity>(-1);
					Entity handleEntity = static_cast<Entity>(-1);
					if (sliderComp.trackEntityGuid.high != 0 || sliderComp.trackEntityGuid.low != 0) {
						trackEntity = EntityGUIDRegistry::GetInstance().GetEntityByGUID(sliderComp.trackEntityGuid);
					}
					if (sliderComp.handleEntityGuid.high != 0 || sliderComp.handleEntityGuid.low != 0) {
						handleEntity = EntityGUIDRegistry::GetInstance().GetEntityByGUID(sliderComp.handleEntityGuid);
					}

					if (trackEntity != static_cast<Entity>(-1)) {
						ecsManager.DestroyEntity(trackEntity);
					}
					if (handleEntity != static_cast<Entity>(-1)) {
						ecsManager.DestroyEntity(handleEntity);
					}
				}

				ecsManager.RemoveComponent<SliderComponent>(request.entity);
				std::cout << "[Inspector] Removed SliderComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "UIAnchorComponent") {
				ecsManager.RemoveComponent<UIAnchorComponent>(request.entity);
				std::cout << "[Inspector] Removed UIAnchorComponent from entity " << request.entity << std::endl;
			}
			else if (request.componentType == "TransformComponent") {
				std::cerr << "[Inspector] Cannot remove TransformComponent - all entities must have one" << std::endl;
			}
			else {
				std::cerr << "[Inspector] Unknown component type for removal: " << request.componentType << std::endl;
			}
		}
		catch (const std::exception& e) {
			std::cerr << "[Inspector] Failed to remove component " << request.componentType << " from entity " << request.entity << ": " << e.what() << std::endl;
		}
	}

	// Clear the queue after processing
	pendingComponentRemovals.clear();
}

void InspectorPanel::ProcessPendingComponentResets() {
	for (const auto& request : pendingComponentResets) {
		try {
			ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

			// Reset the component based on type by assigning default-constructed value
			if (request.componentType == "DirectionalLightComponent") {
				ecsManager.GetComponent<DirectionalLightComponent>(request.entity) = DirectionalLightComponent{};
				std::cout << "[Inspector] Reset DirectionalLightComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "PointLightComponent") {
				ecsManager.GetComponent<PointLightComponent>(request.entity) = PointLightComponent{};
				std::cout << "[Inspector] Reset PointLightComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "SpotLightComponent") {
				ecsManager.GetComponent<SpotLightComponent>(request.entity) = SpotLightComponent{};
				std::cout << "[Inspector] Reset SpotLightComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "ModelRenderComponent") {
				ecsManager.GetComponent<ModelRenderComponent>(request.entity) = ModelRenderComponent{};
				std::cout << "[Inspector] Reset ModelRenderComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "SpriteRenderComponent") {
				ecsManager.GetComponent<SpriteRenderComponent>(request.entity) = SpriteRenderComponent{};
				std::cout << "[Inspector] Reset SpriteRenderComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "TextRenderComponent") {
				ecsManager.GetComponent<TextRenderComponent>(request.entity) = TextRenderComponent{};
				std::cout << "[Inspector] Reset TextRenderComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "ParticleComponent") {
				ecsManager.GetComponent<ParticleComponent>(request.entity) = ParticleComponent{};
				std::cout << "[Inspector] Reset ParticleComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "AudioComponent") {
				ecsManager.GetComponent<AudioComponent>(request.entity) = AudioComponent{};
				std::cout << "[Inspector] Reset AudioComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "AudioListenerComponent") {
				ecsManager.GetComponent<AudioListenerComponent>(request.entity) = AudioListenerComponent{};
				std::cout << "[Inspector] Reset AudioListenerComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "AudioReverbZoneComponent") {
				ecsManager.GetComponent<AudioReverbZoneComponent>(request.entity) = AudioReverbZoneComponent{};
				std::cout << "[Inspector] Reset AudioReverbZoneComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "ColliderComponent") {
				ecsManager.GetComponent<ColliderComponent>(request.entity) = ColliderComponent{};
				std::cout << "[Inspector] Reset ColliderComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "RigidBodyComponent") {
				ecsManager.GetComponent<RigidBodyComponent>(request.entity) = RigidBodyComponent{};
				std::cout << "[Inspector] Reset RigidBodyComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "CameraComponent") {
				ecsManager.GetComponent<CameraComponent>(request.entity) = CameraComponent{};
				std::cout << "[Inspector] Reset CameraComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "BrainComponent") {
				ecsManager.GetComponent<BrainComponent>(request.entity) = BrainComponent{};
				std::cout << "[Inspector] Reset BrainComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "ScriptComponentData") {
				ecsManager.GetComponent<ScriptComponentData>(request.entity) = ScriptComponentData{};
				std::cout << "[Inspector] Reset ScriptComponentData on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "ButtonComponent") {
				ecsManager.GetComponent<ButtonComponent>(request.entity) = ButtonComponent{};
				std::cout << "[Inspector] Reset ButtonComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "SliderComponent") {
				auto& sliderComp = ecsManager.GetComponent<SliderComponent>(request.entity);
				GUID_128 trackGuid = sliderComp.trackEntityGuid;
				GUID_128 handleGuid = sliderComp.handleEntityGuid;
				sliderComp = SliderComponent{};
				sliderComp.trackEntityGuid = trackGuid;
				sliderComp.handleEntityGuid = handleGuid;
				std::cout << "[Inspector] Reset SliderComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "UIAnchorComponent") {
				ecsManager.GetComponent<UIAnchorComponent>(request.entity) = UIAnchorComponent{};
				std::cout << "[Inspector] Reset UIAnchorComponent on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "Transform") {
				// Reset transform to default values
				ecsManager.GetComponent<Transform>(request.entity) = Transform{};
				std::cout << "[Inspector] Reset Transform on entity " << request.entity << std::endl;
			}
			else if (request.componentType == "AnimationComponent") {
				auto& animComp = ecsManager.GetComponent<AnimationComponent>(request.entity);
				// Clear all animation data
				animComp.ClearClips();
				animComp.clipPaths.clear();
				animComp.clipGUIDs.clear();
				animComp.clipCount = 0;
				animComp.controllerPath.clear();
				animComp.enabled = true;
				animComp.isPlay = false;
				animComp.isLoop = true;
				animComp.speed = 1.0f;
				// Clear state machine
				AnimationStateMachine* sm = animComp.GetStateMachine();
				if (sm) {
					sm->Clear();
				}
				std::cout << "[Inspector] Reset AnimationComponent on entity " << request.entity << std::endl;
			}
			else {
				std::cerr << "[Inspector] Unknown component type for reset: " << request.componentType << std::endl;
			}
		}
		catch (const std::exception& e) {
			std::cerr << "[Inspector] Failed to reset component " << request.componentType << " on entity " << request.entity << ": " << e.what() << std::endl;
		}
	}

	// Clear the queue after processing
	pendingComponentResets.clear();
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
		}
		else if (!modelPath.empty()) {
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
				modelRenderer.shaderGUID = { 0x007ebbc8de41468e, 0x0002c7078200001b }; // Default shader GUID
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
		}
		else {
			std::cerr << "[Inspector] Failed to load model for entity " << entity << std::endl;
		}

	}
	catch (const std::exception& e) {
		std::cerr << "[Inspector] Error applying model to entity " << entity << ": " << e.what() << std::endl;
	}
}

// ============================================================================
// Multi-Entity Editing Functions
// ============================================================================

bool InspectorPanel::HasComponent(Entity entity, const std::string& componentType) {
	ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

	if (componentType == "Transform") return ecs.HasComponent<Transform>(entity);
	if (componentType == "NameComponent") return ecs.HasComponent<NameComponent>(entity);
	if (componentType == "TagComponent") return ecs.HasComponent<TagComponent>(entity);
	if (componentType == "LayerComponent") return ecs.HasComponent<LayerComponent>(entity);
	if (componentType == "ModelRenderComponent") return ecs.HasComponent<ModelRenderComponent>(entity);
	if (componentType == "SpriteRenderComponent") return ecs.HasComponent<SpriteRenderComponent>(entity);
	if (componentType == "SpriteAnimationComponent") return ecs.HasComponent<SpriteAnimationComponent>(entity);
	if (componentType == "TextRenderComponent") return ecs.HasComponent<TextRenderComponent>(entity);
	if (componentType == "ParticleComponent") return ecs.HasComponent<ParticleComponent>(entity);
	if (componentType == "AudioComponent") return ecs.HasComponent<AudioComponent>(entity);
	if (componentType == "AudioListenerComponent") return ecs.HasComponent<AudioListenerComponent>(entity);
	if (componentType == "AudioReverbZoneComponent") return ecs.HasComponent<AudioReverbZoneComponent>(entity);
	if (componentType == "DirectionalLightComponent") return ecs.HasComponent<DirectionalLightComponent>(entity);
	if (componentType == "PointLightComponent") return ecs.HasComponent<PointLightComponent>(entity);
	if (componentType == "SpotLightComponent") return ecs.HasComponent<SpotLightComponent>(entity);
	if (componentType == "ColliderComponent") return ecs.HasComponent<ColliderComponent>(entity);
	if (componentType == "RigidBodyComponent") return ecs.HasComponent<RigidBodyComponent>(entity);
	if (componentType == "CameraComponent") return ecs.HasComponent<CameraComponent>(entity);
	if (componentType == "AnimationComponent") return ecs.HasComponent<AnimationComponent>(entity);
	if (componentType == "BrainComponent" || componentType == "Brain") return ecs.HasComponent<BrainComponent>(entity);
	if (componentType == "ScriptComponentData") return ecs.HasComponent<ScriptComponentData>(entity);
	if (componentType == "ButtonComponent") return ecs.HasComponent<ButtonComponent>(entity);
	if (componentType == "SliderComponent") return ecs.HasComponent<SliderComponent>(entity);
	if (componentType == "UIAnchorComponent") return ecs.HasComponent<UIAnchorComponent>(entity);

	return false;
}

void* InspectorPanel::GetComponentPtr(Entity entity, const std::string& componentType) {
	ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

	if (componentType == "Transform" && ecs.HasComponent<Transform>(entity))
		return &ecs.GetComponent<Transform>(entity);
	if (componentType == "NameComponent" && ecs.HasComponent<NameComponent>(entity))
		return &ecs.GetComponent<NameComponent>(entity);
	if (componentType == "TagComponent" && ecs.HasComponent<TagComponent>(entity))
		return &ecs.GetComponent<TagComponent>(entity);
	if (componentType == "LayerComponent" && ecs.HasComponent<LayerComponent>(entity))
		return &ecs.GetComponent<LayerComponent>(entity);
	if (componentType == "ModelRenderComponent" && ecs.HasComponent<ModelRenderComponent>(entity))
		return &ecs.GetComponent<ModelRenderComponent>(entity);
	if (componentType == "SpriteRenderComponent" && ecs.HasComponent<SpriteRenderComponent>(entity))
		return &ecs.GetComponent<SpriteRenderComponent>(entity);
	if (componentType == "SpriteAnimationComponent" && ecs.HasComponent<SpriteAnimationComponent>(entity))
		return &ecs.GetComponent<SpriteAnimationComponent>(entity);
	if (componentType == "TextRenderComponent" && ecs.HasComponent<TextRenderComponent>(entity))
		return &ecs.GetComponent<TextRenderComponent>(entity);
	if (componentType == "ParticleComponent" && ecs.HasComponent<ParticleComponent>(entity))
		return &ecs.GetComponent<ParticleComponent>(entity);
	if (componentType == "AudioComponent" && ecs.HasComponent<AudioComponent>(entity))
		return &ecs.GetComponent<AudioComponent>(entity);
	if (componentType == "AudioListenerComponent" && ecs.HasComponent<AudioListenerComponent>(entity))
		return &ecs.GetComponent<AudioListenerComponent>(entity);
	if (componentType == "AudioReverbZoneComponent" && ecs.HasComponent<AudioReverbZoneComponent>(entity))
		return &ecs.GetComponent<AudioReverbZoneComponent>(entity);
	if (componentType == "DirectionalLightComponent" && ecs.HasComponent<DirectionalLightComponent>(entity))
		return &ecs.GetComponent<DirectionalLightComponent>(entity);
	if (componentType == "PointLightComponent" && ecs.HasComponent<PointLightComponent>(entity))
		return &ecs.GetComponent<PointLightComponent>(entity);
	if (componentType == "SpotLightComponent" && ecs.HasComponent<SpotLightComponent>(entity))
		return &ecs.GetComponent<SpotLightComponent>(entity);
	if (componentType == "ColliderComponent" && ecs.HasComponent<ColliderComponent>(entity))
		return &ecs.GetComponent<ColliderComponent>(entity);
	if (componentType == "RigidBodyComponent" && ecs.HasComponent<RigidBodyComponent>(entity))
		return &ecs.GetComponent<RigidBodyComponent>(entity);
	if (componentType == "CameraComponent" && ecs.HasComponent<CameraComponent>(entity))
		return &ecs.GetComponent<CameraComponent>(entity);
	if (componentType == "AnimationComponent" && ecs.HasComponent<AnimationComponent>(entity))
		return &ecs.GetComponent<AnimationComponent>(entity);
	if (componentType == "BrainComponent" && ecs.HasComponent<BrainComponent>(entity))
		return &ecs.GetComponent<BrainComponent>(entity);
	if (componentType == "ScriptComponentData" && ecs.HasComponent<ScriptComponentData>(entity))
		return &ecs.GetComponent<ScriptComponentData>(entity);
	if (componentType == "ButtonComponent" && ecs.HasComponent<ButtonComponent>(entity))
		return &ecs.GetComponent<ButtonComponent>(entity);
	if (componentType == "SliderComponent" && ecs.HasComponent<SliderComponent>(entity))
		return &ecs.GetComponent<SliderComponent>(entity);
	if (componentType == "UIAnchorComponent" && ecs.HasComponent<UIAnchorComponent>(entity))
		return &ecs.GetComponent<UIAnchorComponent>(entity);

	return nullptr;
}

std::vector<std::string> InspectorPanel::GetSharedComponentTypes(const std::vector<Entity>& entities) {
	if (entities.empty()) return {};

	// List of all possible component types
	std::vector<std::string> allComponentTypes = {
		"Transform",
		"ModelRenderComponent",
		"SpriteRenderComponent",
		"SpriteAnimationComponent",
		"TextRenderComponent",
		"ParticleComponent",
		"AudioComponent",
		"AudioListenerComponent",
		"AudioReverbZoneComponent",
		"DirectionalLightComponent",
		"PointLightComponent",
		"SpotLightComponent",
		"ColliderComponent",
		"RigidBodyComponent",
		"CameraComponent",
		"AnimationComponent",
		"BrainComponent",
		"ScriptComponentData",
		"ButtonComponent",
		"UIAnchorComponent"
	};

	std::vector<std::string> sharedComponents;

	// Check each component type to see if ALL selected entities have it
	for (const auto& componentType : allComponentTypes) {
		bool allHave = true;
		for (Entity entity : entities) {
			if (!HasComponent(entity, componentType)) {
				allHave = false;
				break;
			}
		}
		if (allHave) {
			sharedComponents.push_back(componentType);
		}
	}

	return sharedComponents;
}

void InspectorPanel::DrawSharedComponentsHeader(const std::vector<Entity>& entities) {
	ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

	// Display selection info
	ImGui::Text("%zu entities selected", entities.size());
	ImGui::Separator();

	// Show entity names
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
	std::string entityNames;
	for (size_t i = 0; i < std::min(entities.size(), size_t(5)); ++i) {
		if (ecs.HasComponent<NameComponent>(entities[i])) {
			if (!entityNames.empty()) entityNames += ", ";
			entityNames += ecs.GetComponent<NameComponent>(entities[i]).name;
		}
	}
	if (entities.size() > 5) {
		entityNames += ", ...";
	}
	ImGui::TextWrapped("%s", entityNames.c_str());
	ImGui::PopStyleColor();
	ImGui::Separator();
}

void InspectorPanel::DrawSharedComponentGeneric(const std::vector<Entity>& entities, const std::string& componentType) {
	if (entities.empty()) return;

	ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

	// Get type descriptor from reflection system
	auto& lookup = TypeDescriptor::type_descriptor_lookup();
	auto it = lookup.find(componentType);
	if (it == lookup.end()) {
		return;
	}

	TypeDescriptor_Struct* typeDesc = dynamic_cast<TypeDescriptor_Struct*>(it->second);
	if (!typeDesc) {
		return;
	}

	// Get display name for the component
	static const std::unordered_map<std::string, std::string> componentDisplayNames = {
		{"Transform", "Transform"},
		{"ModelRenderComponent", "Model Renderer"},
		{"SpriteRenderComponent", "Sprite Renderer"},
		{"SpriteAnimationComponent", "Sprite Animation"},
		{"TextRenderComponent", "Text Renderer"},
		{"ParticleComponent", "Particle System"},
		{"AudioComponent", "Audio Source"},
		{"AudioListenerComponent", "Audio Listener"},
		{"AudioReverbZoneComponent", "Audio Reverb Zone"},
		{"DirectionalLightComponent", "Directional Light"},
		{"PointLightComponent", "Point Light"},
		{"SpotLightComponent", "Spot Light"},
		{"ColliderComponent", "Collider"},
		{"RigidBodyComponent", "RigidBody"},
		{"CameraComponent", "Camera"},
		{"AnimationComponent", "Animation"},
		{"BrainComponent", "Brain"},
		{"ScriptComponentData", "Script"},
		{"ButtonComponent", "Button"},
		{"UIAnchorComponent", "UI Anchor"}
	};

	std::string displayName = componentType;
	auto nameIt = componentDisplayNames.find(componentType);
	if (nameIt != componentDisplayNames.end()) {
		displayName = nameIt->second;
	}

	// Draw collapsing header for the component
	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));

	ImGui::Spacing();

	std::string headerLabel = displayName + " (Multi-Edit)";
	bool isOpen = ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

	ImGui::PopStyleColor(3);

	if (!isOpen) return;

	ImGui::Spacing();

	// Get members from the first entity to determine structure
	Entity firstEntity = entities[0];
	void* firstComponentPtr = nullptr;

	// Get component pointer for the first entity
	if (componentType == "Transform" && ecs.HasComponent<Transform>(firstEntity)) {
		firstComponentPtr = &ecs.GetComponent<Transform>(firstEntity);
	}
	else if (componentType == "ModelRenderComponent" && ecs.HasComponent<ModelRenderComponent>(firstEntity)) {
		firstComponentPtr = &ecs.GetComponent<ModelRenderComponent>(firstEntity);
	}
	else if (componentType == "SpriteRenderComponent" && ecs.HasComponent<SpriteRenderComponent>(firstEntity)) {
		firstComponentPtr = &ecs.GetComponent<SpriteRenderComponent>(firstEntity);
	}
	else if (componentType == "ColliderComponent" && ecs.HasComponent<ColliderComponent>(firstEntity)) {
		firstComponentPtr = &ecs.GetComponent<ColliderComponent>(firstEntity);
	}
	else if (componentType == "RigidBodyComponent" && ecs.HasComponent<RigidBodyComponent>(firstEntity)) {
		firstComponentPtr = &ecs.GetComponent<RigidBodyComponent>(firstEntity);
	}
	else if (componentType == "CameraComponent" && ecs.HasComponent<CameraComponent>(firstEntity)) {
		firstComponentPtr = &ecs.GetComponent<CameraComponent>(firstEntity);
	}
	else if (componentType == "DirectionalLightComponent" && ecs.HasComponent<DirectionalLightComponent>(firstEntity)) {
		firstComponentPtr = &ecs.GetComponent<DirectionalLightComponent>(firstEntity);
	}
	else if (componentType == "PointLightComponent" && ecs.HasComponent<PointLightComponent>(firstEntity)) {
		firstComponentPtr = &ecs.GetComponent<PointLightComponent>(firstEntity);
	}
	else if (componentType == "SpotLightComponent" && ecs.HasComponent<SpotLightComponent>(firstEntity)) {
		firstComponentPtr = &ecs.GetComponent<SpotLightComponent>(firstEntity);
	}
	else if (componentType == "AudioComponent" && ecs.HasComponent<AudioComponent>(firstEntity)) {
		firstComponentPtr = &ecs.GetComponent<AudioComponent>(firstEntity);
	}
	else if (componentType == "ParticleComponent" && ecs.HasComponent<ParticleComponent>(firstEntity)) {
		firstComponentPtr = &ecs.GetComponent<ParticleComponent>(firstEntity);
	}
	else if (componentType == "AnimationComponent" && ecs.HasComponent<AnimationComponent>(firstEntity)) {
		firstComponentPtr = &ecs.GetComponent<AnimationComponent>(firstEntity);
	}

	if (!firstComponentPtr) {
		ImGui::TextDisabled("Cannot edit this component type in multi-edit mode");
		return;
	}

	// For Transform component, provide a simplified multi-edit interface
	if (componentType == "Transform") {
		ImGui::TextDisabled("Transform editing applies offset to all selected entities");
		ImGui::Spacing();

		static Vector3D positionOffset(0.0f, 0.0f, 0.0f);
		static Vector3D rotationOffset(0.0f, 0.0f, 0.0f);
		static Vector3D scaleMultiplier(1.0f, 1.0f, 1.0f);

		// Position offset
		ImGui::Text("Position Offset:");
		float pos[3] = { positionOffset.x, positionOffset.y, positionOffset.z };
		if (ImGui::DragFloat3("##PosOffset", pos, 0.1f)) {
			Vector3D newOffset(pos[0], pos[1], pos[2]);
			Vector3D delta = newOffset - positionOffset;
			positionOffset = newOffset;

			// Apply delta to all entities
			for (Entity entity : entities) {
				if (ecs.HasComponent<Transform>(entity)) {
					Transform& transform = ecs.GetComponent<Transform>(entity);
					transform.localPosition = transform.localPosition + delta;
					transform.isDirty = true;
				}
			}
		}

		// Rotation offset (in Euler degrees)
		ImGui::Text("Rotation Offset:");
		float rot[3] = { rotationOffset.x, rotationOffset.y, rotationOffset.z };
		if (ImGui::DragFloat3("##RotOffset", rot, 0.5f)) {
			Vector3D newOffset(rot[0], rot[1], rot[2]);
			Vector3D delta = newOffset - rotationOffset;
			rotationOffset = newOffset;

			// Apply delta rotation to all entities
			// Convert delta from Euler degrees to quaternion
			Quaternion deltaQuat = Quaternion::FromEulerDegrees(delta);

			for (Entity entity : entities) {
				if (ecs.HasComponent<Transform>(entity)) {
					Transform& transform = ecs.GetComponent<Transform>(entity);
					// Compose rotations: apply delta rotation to current rotation
					transform.localRotation = deltaQuat * transform.localRotation;
					transform.isDirty = true;
				}
			}
		}

		// Scale multiplier
		ImGui::Text("Scale Multiplier:");
		float scl[3] = { scaleMultiplier.x, scaleMultiplier.y, scaleMultiplier.z };
		if (ImGui::DragFloat3("##ScaleMultiplier", scl, 0.01f, 0.01f, 100.0f)) {
			Vector3D newMultiplier(scl[0], scl[1], scl[2]);

			// Calculate ratio
			Vector3D ratio(
				scaleMultiplier.x > 0.001f ? newMultiplier.x / scaleMultiplier.x : 1.0f,
				scaleMultiplier.y > 0.001f ? newMultiplier.y / scaleMultiplier.y : 1.0f,
				scaleMultiplier.z > 0.001f ? newMultiplier.z / scaleMultiplier.z : 1.0f
			);
			scaleMultiplier = newMultiplier;

			// Apply ratio to all entities
			for (Entity entity : entities) {
				if (ecs.HasComponent<Transform>(entity)) {
					Transform& transform = ecs.GetComponent<Transform>(entity);
					transform.localScale.x *= ratio.x;
					transform.localScale.y *= ratio.y;
					transform.localScale.z *= ratio.z;
					transform.isDirty = true;
				}
			}
		}

		// Reset button
		if (ImGui::Button("Reset Offsets")) {
			positionOffset = Vector3D(0.0f, 0.0f, 0.0f);
			rotationOffset = Vector3D(0.0f, 0.0f, 0.0f);
			scaleMultiplier = Vector3D(1.0f, 1.0f, 1.0f);
		}
	}
	else {
		// For other components, just show the first entity's component with a note
		ImGui::TextDisabled("Showing values from first selected entity");
		ImGui::TextDisabled("Editing will apply to all selected entities");
		ImGui::Spacing();

		// Render using reflection (changes will be applied to first entity only for now)
		ImGui::PushID(firstComponentPtr);
		try {
			ReflectionRenderer::RenderComponent(firstComponentPtr, typeDesc, firstEntity, ecs);
		}
		catch (const std::exception& e) {
			ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", e.what());
		}
		ImGui::PopID();
	}
}

void InspectorPanel::DrawMultiEntityInspector(const std::vector<Entity>& entities) {
	if (entities.empty()) return;

	try {
		// Draw header with entity count and names
		DrawSharedComponentsHeader(entities);

		// Get list of shared components
		std::vector<std::string> sharedComponents = GetSharedComponentTypes(entities);

		if (sharedComponents.empty()) {
			ImGui::TextDisabled("No shared components between selected entities");
		}
		else {
			// Display info about shared components
			ImGui::Text("Shared Components:");
			ImGui::SameLine();
			ImGui::TextDisabled("(%zu)", sharedComponents.size());
			ImGui::Separator();

			// Draw each shared component
			for (const auto& componentType : sharedComponents) {
				DrawSharedComponentGeneric(entities, componentType);
			}
		}

		// Add component button for multi-entity selection
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		DrawAddComponentButtonMulti(entities);
	}
	catch (const std::exception& e) {
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", e.what());
	}
}