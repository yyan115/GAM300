/* Start Header ************************************************************************/
/*!
\file       InspectorCustomRenderers.cpp
\author     Lucas Yee
\date       2025
\brief      Custom field renderers for Inspector components that need special handling.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "pch.h"
#include "ReflectionRenderer.hpp"
#include "ECS/ECSManager.hpp"
#include "Transform/TransformSystem.hpp"
#include "Physics/ColliderComponent.hpp"
#include "Physics/CollisionLayers.hpp"
#include "Graphics/Camera/CameraComponent.hpp"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include "Math/Vector3D.hpp"
#include <glm/glm.hpp>
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Graphics/Particle/ParticleComponent.hpp"
#include "Graphics/TextRendering/TextRenderComponent.hpp"
#include "Physics/RigidBodyComponent.hpp"
#include "Physics/Kinematics/CharacterControllerComponent.hpp"
#include "Graphics/Lights/LightComponent.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Sound/AudioComponent.hpp"
#include "SnapshotManager.hpp"
#include "Sound/AudioListenerComponent.hpp"
#include "Sound/AudioReverbZoneComponent.hpp"
#include "ECS/NameComponent.hpp"
#include "ECS/ActiveComponent.hpp"
#include "EditorState.hpp"
#include "ECS/TagComponent.hpp"
#include "ECS/LayerComponent.hpp"
#include "ECS/TagManager.hpp"
#include "ECS/LayerManager.hpp"
#include "Animation/AnimationComponent.hpp"
#include "Game AI/BrainComponent.hpp"
#include "Game AI/BrainFactory.hpp"
#include "Script/ScriptComponentData.hpp"
#include "imgui.h"
#include "EditorComponents.hpp"
#include "../../../Libraries/IconFontCppHeaders/IconsFontAwesome6.h"
#include "UndoableWidgets.hpp"
#include <glm/glm.hpp>
#include <cfloat>

// External drag-drop state
extern GUID_128 DraggedModelGuid;
extern std::string DraggedModelPath;
extern GUID_128 DraggedMaterialGuid;
extern std::string DraggedMaterialPath;
extern GUID_128 DraggedAudioGuid;
extern std::string DraggedAudioPath;
extern GUID_128 DraggedFontGuid;
extern std::string DraggedFontPath;
extern GUID_128 DraggedScriptGuid;
extern std::string DraggedScriptPath;

void RegisterInspectorCustomRenderers()
{
    // ==================== CUSTOM TYPE RENDERERS ====================
    // Register custom renderer for glm::vec3 (used by CameraComponent and others)

    ReflectionRenderer::RegisterCustomRenderer("glm::vec3",
    [](const char *name, void *ptr, Entity, ECSManager &)
    {
        glm::vec3 *vec = static_cast<glm::vec3 *>(ptr);

        // Convert field name from camelCase to "Proper Case"
        std::string displayName = name;
        if (!displayName.empty())
        {
            displayName[0] = static_cast<char>(std::toupper(displayName[0]));
            for (size_t i = 1; i < displayName.size(); ++i)
            {
                if (std::isupper(displayName[i]) && i > 0 && std::islower(displayName[i - 1]))
                {
                    displayName.insert(i, " ");
                    i++;
                }
            }
        }

        ImGui::Text("%s", displayName.c_str());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);

        float values[3] = {vec->x, vec->y, vec->z};
        std::string id = std::string("##") + name + "_" + std::to_string(reinterpret_cast<uintptr_t>(ptr));

        // Use UndoableWidgets wrapper - handles undo/redo automatically!
        bool modified = UndoableWidgets::DragFloat3(id.c_str(), values, 0.1f);

        if (modified)
        {
            vec->x = values[0];
            vec->y = values[1];
            vec->z = values[2];
            return true;
        }

        return false;
    });

    // ==================== NAME COMPONENT ====================
    // Name component is rendered without collapsing header at the top

    ReflectionRenderer::RegisterComponentRenderer("NameComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
        ecs;
        NameComponent &nameComp = *static_cast<NameComponent *>(componentPtr);

        // Unity-style checkbox on the left (from ActiveComponent)
        if (ecs.HasComponent<ActiveComponent>(entity))
        {
            auto &activeComp = ecs.GetComponent<ActiveComponent>(entity);

            // Style the checkbox to be smaller with white checkmark
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));                  // Smaller padding
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));      // White checkmark
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));        // Dark gray background
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f)); // Lighter on hover
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));  // Even lighter when clicking

            // Use UndoableWidgets wrapper for automatic undo/redo
            UndoableWidgets::Checkbox("##EntityActive", &activeComp.isActive);

            ImGui::PopStyleColor(4); // Pop all 4 colors
            ImGui::PopStyleVar();    // Pop padding

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Enable/Disable Entity");
            }
            ImGui::SameLine();
        }

        // Simple text input for name (no collapsing header)
        char buf[128] = {};
        std::snprintf(buf, sizeof(buf), "%s", nameComp.name.c_str());
        if (UndoableWidgets::InputText("Name", buf, sizeof(buf)))
        {
            nameComp.name = buf;
        }

        return true; // Skip default rendering (we rendered everything)
    });

    // ==================== TAG COMPONENT ====================
    // Tag component uses TagManager dropdown (rendered inline with Layer)

    ReflectionRenderer::RegisterComponentRenderer("TagComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
        ecs;
        TagComponent &tagComp = *static_cast<TagComponent *>(componentPtr);

        // Get available tags
        const auto &availableTags = TagManager::GetInstance().GetAllTags();

        // Create items for combo box, including "Add Tag..." option
        std::vector<std::string> tagItems;
        tagItems.reserve(availableTags.size() + 1);
        for (const auto &tag : availableTags)
        {
        tagItems.push_back(tag);
        }
        tagItems.push_back("Add Tag...");

        // Convert to const char* array for ImGui
        std::vector<const char *> tagItemPtrs;
        tagItemPtrs.reserve(tagItems.size());
        for (const auto &item : tagItems)
        {
        tagItemPtrs.push_back(item.c_str());
        }

        // Ensure tagIndex is valid
        if (tagComp.tagIndex < 0 || tagComp.tagIndex >= static_cast<int>(availableTags.size()))
        {
        tagComp.tagIndex = 0; // Default to first tag
        }

        // Inline rendering (no label, just combo) - using UndoableWidgets
        ImGui::Text("Tag");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        int currentTag = tagComp.tagIndex;
        if (UndoableWidgets::Combo("##Tag", &currentTag, tagItemPtrs.data(), static_cast<int>(tagItemPtrs.size())))
        {
        if (currentTag >= 0 && currentTag < static_cast<int>(availableTags.size()))
        {
        tagComp.tagIndex = currentTag;
        }
        else if (currentTag == static_cast<int>(availableTags.size()))
        {
        // "Add Tag..." was selected - could open Tags & Layers window here
        // Reset selection to current tag
        currentTag = tagComp.tagIndex;
        }
        }

        ImGui::SameLine(); // Keep Layer on same line

        return true; // Skip default rendering
    });

    // ==================== LAYER COMPONENT ====================
    // Layer component uses LayerManager dropdown (rendered inline with Tag)

    ReflectionRenderer::RegisterComponentRenderer("LayerComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
        ecs;
        LayerComponent &layerComp = *static_cast<LayerComponent *>(componentPtr);

        // Get available layers
        const auto &availableLayers = LayerManager::GetInstance().GetAllLayers();

        // Create items for combo box (only show named layers)
        std::vector<std::string> layerItems;
        std::vector<int> layerIndices;
        for (int i = 0; i < LayerManager::MAX_LAYERS; ++i)
        {
            const std::string &layerName = availableLayers[i];
            if (!layerName.empty())
            {
                layerItems.push_back(std::to_string(i) + ": " + layerName);
                layerIndices.push_back(i);
            }
        }

        // Add "Add Layer..." option
        layerItems.push_back("Add Layer...");
        std::vector<int> tempIndices = layerIndices;
        tempIndices.push_back(-1); // Special value for "Add Layer..."

        // Convert to const char* array for ImGui
        std::vector<const char *> layerItemPtrs;
        layerItemPtrs.reserve(layerItems.size());
        for (const auto &item : layerItems)
        {
            layerItemPtrs.push_back(item.c_str());
        }

        // Ensure layerIndex is valid
        if (layerComp.layerIndex < 0 || layerComp.layerIndex >= LayerManager::MAX_LAYERS)
        {
            layerComp.layerIndex = 0; // Default to first layer
        }

        // Find current selection index in our filtered list
        int currentSelection = -1;
        for (size_t i = 0; i < layerIndices.size(); ++i)
        {
            if (layerIndices[i] == layerComp.layerIndex)
            {
                currentSelection = static_cast<int>(i);
                break;
            }
        }

        // If current layer is not in the named list, default to first
        if (currentSelection == -1 && !layerIndices.empty())
        {
            currentSelection = 0;
            layerComp.layerIndex = layerIndices[0];
        }

        // Inline rendering (continues from Tag on same line) - using UndoableWidgets
        ImGui::Text("Layer");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (UndoableWidgets::Combo("##Layer", &currentSelection, layerItemPtrs.data(), static_cast<int>(layerItemPtrs.size())))
        {
            if (currentSelection >= 0 && currentSelection < static_cast<int>(tempIndices.size()))
            {
                int selectedIndex = tempIndices[currentSelection];
                if (selectedIndex != -1)
                {
                    layerComp.layerIndex = selectedIndex;
                }
            }
        }

        ImGui::Separator(); // Add separator after Tag/Layer line

        return true; // Skip default rendering
    });

    // ==================== TRANSFORM COMPONENT ====================
    // Transform needs to use TransformSystem for setting values

    ReflectionRenderer::RegisterFieldRenderer("Transform", "localPosition",
    [](const char *name, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        Vector3D *pos = static_cast<Vector3D *>(ptr);
        float arr[3] = {pos->x, pos->y, pos->z};

        ImGui::Text("Position");
        ImGui::SameLine();

        // Use UndoableWidgets wrapper for automatic undo/redo
        bool changed = UndoableWidgets::DragFloat3("##Position", arr, 0.1f, -FLT_MAX, FLT_MAX, "%.3f");

        if (changed)
        {
        ecs.transformSystem->SetLocalPosition(entity, {arr[0], arr[1], arr[2]});
        return true;
        }
        return false;
    });

    ReflectionRenderer::RegisterFieldRenderer("Transform", "localRotation",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        Quaternion *quat = static_cast<Quaternion *>(ptr);
        Vector3D euler = quat->ToEulerDegrees();
        float arr[3] = {euler.x, euler.y, euler.z};

        ImGui::Text("Rotation");
        ImGui::SameLine();

        // Use UndoableWidgets wrapper for automatic undo/redo
        bool changed = UndoableWidgets::DragFloat3("##Rotation", arr, 1.0f, -180.0f, 180.0f, "%.1f");

        if (changed)
        {
        ecs.transformSystem->SetLocalRotation(entity, {arr[0], arr[1], arr[2]});
        return true;
        }
        return false;
    });

    ReflectionRenderer::RegisterFieldRenderer("Transform", "localScale",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        Vector3D *scale = static_cast<Vector3D *>(ptr);
        float arr[3] = {scale->x, scale->y, scale->z};

        ImGui::Text("Scale");
        ImGui::SameLine();

        // Use UndoableWidgets wrapper for automatic undo/redo
        bool changed = UndoableWidgets::DragFloat3("##Scale", arr, 0.1f, 0.001f, FLT_MAX, "%.3f");

        if (changed)
        {
            ecs.transformSystem->SetLocalScale(entity, {arr[0], arr[1], arr[2]});
            return true;
        }
        return false;
    });

    // ==================== COLLIDER COMPONENT ====================
    // ColliderComponent needs custom rendering for shape type and parameters

    ReflectionRenderer::RegisterFieldRenderer("ColliderComponent", "shapeTypeID",
    [](const char *, void *, Entity entity, ECSManager &ecs)
    {
        ecs;
        auto &collider = ecs.GetComponent<ColliderComponent>(entity);
        auto &rc = ecs.GetComponent<ModelRenderComponent>(entity);

        ImGui::Text("Shape Type");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        const char *shapeTypes[] = {"Box", "Sphere", "Capsule", "Cylinder"};
        int currentShapeType = static_cast<int>(collider.shapeType);

        EditorComponents::PushComboColors();
        bool changed = UndoableWidgets::Combo("##ShapeType", &currentShapeType, shapeTypes, 4);
        EditorComponents::PopComboColors();

        if (changed)
        {
            collider.shapeType = static_cast<ColliderShapeType>(currentShapeType);
            collider.shapeTypeID = currentShapeType;
            collider.version++;
        }

        // Shape Parameters based on type
        bool shapeParamsChanged = false;
        Vector3D halfExtent = rc.CalculateModelHalfExtent(*rc.model);
        float radius = rc.CalculateModelRadius(*rc.model);

        switch (collider.shapeType)
        {
        case ColliderShapeType::Box:
        {
            ImGui::Text("Half Extents");
            ImGui::SameLine();
            collider.boxHalfExtents = halfExtent;
            float halfExtents[3] = {collider.boxHalfExtents.x, collider.boxHalfExtents.y, collider.boxHalfExtents.z};
            if (UndoableWidgets::DragFloat3("##HalfExtents", halfExtents, 0.1f, 0.01f, FLT_MAX, "%.2f"))
            {
                collider.boxHalfExtents = Vector3D(halfExtents[0], halfExtents[1], halfExtents[2]);
                shapeParamsChanged = true;
            }
            break;
        }
        case ColliderShapeType::Sphere:
        {
            ImGui::Text("Radius");
            ImGui::SameLine();
            collider.sphereRadius = radius;
            if (UndoableWidgets::DragFloat("##SphereRadius", &collider.sphereRadius, 0.1f, 0.01f, FLT_MAX, "%.2f"))
            {
                shapeParamsChanged = true;
            }
            break;
        }
        case ColliderShapeType::Capsule:
        {
            ImGui::Text("Radius");
            ImGui::SameLine();
            collider.capsuleRadius = std::min(halfExtent.x, halfExtent.z);
            if (UndoableWidgets::DragFloat("##CapsuleRadius", &collider.capsuleRadius, 0.1f, 0.01f, FLT_MAX, "%.2f"))
            {
                shapeParamsChanged = true;
            }
            ImGui::Text("Half Height");
            ImGui::SameLine();
            collider.capsuleHalfHeight = halfExtent.y;
            if (UndoableWidgets::DragFloat("##CapsuleHalfHeight", &collider.capsuleHalfHeight, 0.1f, 0.01f, FLT_MAX, "%.2f"))
            {
                shapeParamsChanged = true;
            }
            break;
        }
        case ColliderShapeType::Cylinder:
        {
            ImGui::Text("Radius");
            ImGui::SameLine();
            collider.cylinderRadius = std::min(halfExtent.x, halfExtent.z);
            if (UndoableWidgets::DragFloat("##CylinderRadius", &collider.cylinderRadius, 0.1f, 0.01f, FLT_MAX, "%.2f"))
            {
                shapeParamsChanged = true;
            }
            ImGui::Text("Half Height");
            ImGui::SameLine();
            collider.cylinderRadius = halfExtent.y;
            if (UndoableWidgets::DragFloat("##CylinderHalfHeight", &collider.cylinderHalfHeight, 0.1f, 0.01f, FLT_MAX, "%.2f"))
            {
                shapeParamsChanged = true;
            }
            break;
        }
        }

        if (shapeParamsChanged)
        {
            collider.version++;
        }

        return changed || shapeParamsChanged;
    });

    ReflectionRenderer::RegisterFieldRenderer("ColliderComponent", "layerID",
    [](const char *, void *, Entity entity, ECSManager &ecs)
    {
        ecs;
        auto &collider = ecs.GetComponent<ColliderComponent>(entity);

        ImGui::Text("Layer");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        const char *layers[] = {"Non-Moving", "Moving", "Sensor", "Debris"};
        int currentLayer = static_cast<int>(collider.layer);

        EditorComponents::PushComboColors();
        bool changed = UndoableWidgets::Combo("##Layer", &currentLayer, layers, 4);
        EditorComponents::PopComboColors();

        if (changed)
        {
            collider.layer = static_cast<JPH::ObjectLayer>(currentLayer);
            collider.layerID = currentLayer;
            collider.version++;
        }

        return changed;
    });

    // Skip non-reflected fields (these are handled with shapeTypeID)
    ReflectionRenderer::RegisterFieldRenderer("ColliderComponent", "boxHalfExtents",
    [](const char *, void *, Entity, ECSManager &ecs)
    { 
        ecs;
        return false; 
    });

    // ==================== RIGIDBODY COMPONENT ====================
    ReflectionRenderer::RegisterComponentRenderer("RigidBodyComponent",
    [](void *, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)  
    {
        ecs;
        auto &rigidBody = ecs.GetComponent<RigidBodyComponent>(entity);
        auto &transform = ecs.GetComponent<Transform>(entity); // for info tab

        ImGui::PushID("RigidBodyComponent");

        // --- Motion Type dropdown ---
        ImGui::Text("Motion");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);

        const char *motionTypes[] = {"Static", "Kinematic", "Dynamic"};
        int currentMotion = rigidBody.motionID;
        EditorComponents::PushComboColors();
        if (UndoableWidgets::Combo("##MotionType", &currentMotion, motionTypes, IM_ARRAYSIZE(motionTypes)))
        {
            rigidBody.motion = static_cast<Motion>(currentMotion);
            rigidBody.motionID = currentMotion;
            rigidBody.motion_dirty = true; // mark for recreation
        }
        EditorComponents::PopComboColors();

        // --- Is Trigger checkbox ---
        UndoableWidgets::Checkbox("##IsTrigger", &rigidBody.isTrigger);
        ImGui::SameLine();
        ImGui::Text("Is Trigger");

        if (rigidBody.motion == Motion::Dynamic)
        {
            // --- CCD checkbox ---
            if (UndoableWidgets::Checkbox("##CCD", &rigidBody.ccd))
            {
                rigidBody.motion_dirty = true;
            }
            ImGui::SameLine();
            ImGui::Text("CCD");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Continuous Collision Detection - prevents fast-moving objects from tunneling");

            // --- Linear & Angular Damping ---
            UndoableWidgets::DragFloat("##LinearDamping", &rigidBody.linearDamping, 0.1f, -FLT_MAX, FLT_MAX, "%.2f");
            ImGui::SameLine();
            ImGui::Text("Linear Damping");

            UndoableWidgets::DragFloat("##AngularDamping", &rigidBody.angularDamping, 0.1f, -FLT_MAX, FLT_MAX, "%.2f");
            ImGui::SameLine();
            ImGui::Text("Angular Damping");

            // --- Gravity Factor ---
            UndoableWidgets::DragFloat("##GravityFactor", &rigidBody.gravityFactor, 0.1f, -FLT_MAX, FLT_MAX, "%.2f");
            ImGui::SameLine();
            ImGui::Text("Gravity Factor");
        }

        // --- Info Section (Read-only) ---
        if (ImGui::CollapsingHeader("Info", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BeginDisabled();

            // Position
            float position[3] = {transform.localPosition.x, transform.localPosition.y, transform.localPosition.z};
            ImGui::DragFloat3("##Position", position, 0.1f, -FLT_MAX, FLT_MAX, "%.3f");
            ImGui::SameLine();
            ImGui::Text("Position");

            // Rotation
            float rotation[3] = {transform.localRotation.x, transform.localRotation.y, transform.localRotation.z};
            ImGui::DragFloat3("##Rotation", rotation, 1.0f, -180.0f, 180.0f, "%.3f");
            ImGui::SameLine();
            ImGui::Text("Rotation");

            // Linear Velocity
            float linearVel[3] = {rigidBody.linearVel.x, rigidBody.linearVel.y, rigidBody.linearVel.z};
            ImGui::DragFloat3("##LinearVelocity", linearVel, 0.1f, -FLT_MAX, FLT_MAX, "%.2f");
            ImGui::SameLine();
            ImGui::Text("Linear Velocity");

            // Angular Velocity
            float angularVel[3] = {rigidBody.angularVel.x, rigidBody.angularVel.y, rigidBody.angularVel.z};
            ImGui::DragFloat3("##AngularVelocity", angularVel, 0.1f, -FLT_MAX, FLT_MAX, "%.2f");
            ImGui::SameLine();
            ImGui::Text("Angular Velocity");

            ImGui::EndDisabled();
        }

        ImGui::PopID();
        return true; // skip default reflection
    });

    ReflectionRenderer::RegisterComponentRenderer("CharacterControllerComponent",
        [](void*, TypeDescriptor_Struct*, Entity entity, ECSManager& ecs)
        {
            auto& controller = ecs.GetComponent<CharacterControllerComponent>(entity);

            ImGui::PushID("CharacterControllerComponent");

            // Enabled
            UndoableWidgets::Checkbox("Enabled", &controller.enabled);

            // Speed
            UndoableWidgets::DragFloat("Speed", &controller.speed, 0.1f, 0.0f, FLT_MAX, "%.2f");

            // Jump Height
            UndoableWidgets::DragFloat("Jump Height", &controller.jumpHeight, 0.1f, 0.0f, FLT_MAX, "%.2f");

            ImGui::PopID();
            return true; // skip default reflection
        });






    // ==================== CAMERA COMPONENT ====================
    // Camera needs special handling for enum and glm::vec3 properties

    ReflectionRenderer::RegisterComponentRenderer("CameraComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
        ecs;
        CameraComponent &camera = *static_cast<CameraComponent *>(componentPtr);

        // Manually render the non-reflected properties first

        // Projection Type dropdown - using UndoableWidgets
        ImGui::Text("Projection");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        const char *projTypes[] = {"Perspective", "Orthographic"};
        int currentProj = static_cast<int>(camera.projectionType);

        EditorComponents::PushComboColors();
        if (UndoableWidgets::Combo("##Projection", &currentProj, projTypes, 2))
        {
            camera.projectionType = static_cast<ProjectionType>(currentProj);
        }
        EditorComponents::PopComboColors();

        // Target (glm::vec3)
        ImGui::Text("Target");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        float target[3] = {camera.target.x, camera.target.y, camera.target.z};
        if (UndoableWidgets::DragFloat3("##Target", target, 0.1f))
        {
            camera.target = glm::vec3(target[0], target[1], target[2]);
        }

        // Up (glm::vec3)
        ImGui::Text("Up");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        float up[3] = {camera.up.x, camera.up.y, camera.up.z};
        if (UndoableWidgets::DragFloat3("##Up", up, 0.1f))
        {
            camera.up = glm::vec3(up[0], up[1], up[2]);
        }

        ImGui::Text("Clear Flags");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        const char* clearFlagsOptions[] = {"Skybox", "Solid Color", "Depth Only", "Don't Clear"};
        int currentClearFlags = static_cast<int>(camera.clearFlags);
        EditorComponents::PushComboColors();
        if (UndoableWidgets::Combo("##ClearFlags", &currentClearFlags, clearFlagsOptions, 4))
        {
            camera.clearFlags = static_cast<CameraClearFlags>(currentClearFlags);
        }
        EditorComponents::PopComboColors();

        ImGui::Text("Background");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        float bgColor[3] = {camera.backgroundColor.r, camera.backgroundColor.g, camera.backgroundColor.b};
        if (UndoableWidgets::ColorEdit3("##Background", bgColor))
        {
            camera.backgroundColor = glm::vec3(bgColor[0], bgColor[1], bgColor[2]);
        }

        ImGui::Text("Ambient Intensity");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if (ecs.lightingSystem) {
            float ambientIntensity = ecs.lightingSystem->ambientIntensity;
            if (UndoableWidgets::SliderFloat("##AmbientIntensity", &ambientIntensity, 0.0f, 5.0f))
            {
                ecs.lightingSystem->SetAmbientIntensity(ambientIntensity);
            }
        }

        return false;
    });

    // ==================== GUID FIELDS WITH DRAG-DROP ====================
    // Model GUID drag-drop

    ReflectionRenderer::RegisterFieldRenderer("ModelRenderComponent", "modelGUID",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        GUID_128 *guid = static_cast<GUID_128 *>(ptr);

        ImGui::Text("Model:");
        ImGui::SameLine();

        // Display current model path or "None"
        std::string modelPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
        std::string displayText = modelPath.empty() ? "None (Model)" : modelPath.substr(modelPath.find_last_of("/\\") + 1);

        // Use EditorComponents for better drag-drop visual feedback
        float buttonWidth = ImGui::GetContentRegionAvail().x;
        EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

        // Drag-drop target with proper payload type
        if (EditorComponents::BeginDragDropTarget())
        {
            ImGui::SetTooltip("Drop .obj, .fbx, .dae, or .3ds model here");
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("MODEL_DRAG"))
            {
                // Take snapshot before changing model
                SnapshotManager::GetInstance().TakeSnapshot("Assign Model");

                // Load and apply the model
                auto &modelRenderer = ecs.GetComponent<ModelRenderComponent>(entity);

                std::cout << "[Inspector] Applying model - GUID: {" << DraggedModelGuid.high << ", " << DraggedModelGuid.low << "}, Path: " << DraggedModelPath << std::endl;

                try
                {
                    // Load model using ResourceManager
                    std::shared_ptr<Model> loadedModel = nullptr;
                    if (DraggedModelGuid.high != 0 || DraggedModelGuid.low != 0)
                    {
                        loadedModel = ResourceManager::GetInstance().GetResourceFromGUID<Model>(DraggedModelGuid, DraggedModelPath);
                    }
                    else if (!DraggedModelPath.empty())
                    {
                        loadedModel = ResourceManager::GetInstance().GetResource<Model>(DraggedModelPath);
                    }

                    if (loadedModel)
                    {
                        std::cout << "[Inspector] Model loaded successfully!" << std::endl;
                        modelRenderer.model = loadedModel;
                        modelRenderer.modelGUID = DraggedModelGuid;

                        // Load default shader if not already set
                        if (!modelRenderer.shader)
                        {
                            modelRenderer.shader = ResourceManager::GetInstance().GetResource<Shader>(
                                ResourceManager::GetPlatformShaderPath("default"));
                            modelRenderer.shaderGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(
                                ResourceManager::GetPlatformShaderPath("default"));
                        }

                        if (loadedModel->meshes[0].material)
                        {
                            modelRenderer.material = loadedModel->meshes[0].material;
                            std::string materialPath = AssetManager::GetInstance().GetAssetPathFromAssetName(modelRenderer.material->GetName() + ".mat");
                            modelRenderer.materialGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(materialPath);
                        }
                    }
                    else
                    {
                        std::cerr << "[Inspector] Failed to load model!" << std::endl;
                    }
                }
                catch (const std::exception &e)
                {
                    std::cerr << "[Inspector] Exception loading model: " << e.what() << std::endl;
                    std::cerr << "[Inspector] Model may have corrupted material references. Please check the .obj file." << std::endl;
                }

                // Clear the drag state
                DraggedModelGuid = {0, 0};
                DraggedModelPath.clear();

                EditorComponents::EndDragDropTarget();
                return true;
            }
            EditorComponents::EndDragDropTarget();
        }

        return false;
    });

    // Shader GUID drag-drop
    ReflectionRenderer::RegisterFieldRenderer("ModelRenderComponent", "shaderGUID",
    [](const char *, void *ptr, Entity, ECSManager &ecs)
    {
        ecs;
        GUID_128 *guid = static_cast<GUID_128 *>(ptr);

        ImGui::Text("Shader:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);

        std::string shaderPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
        std::string displayText = shaderPath.empty() ? "None" : shaderPath.substr(shaderPath.find_last_of("/\\") + 1);

        ImGui::Button(displayText.c_str(), ImVec2(-1, 0));

        // TODO: Add shader drag-drop support when available

        return false;
    });

    // Material GUID drag-drop
    ReflectionRenderer::RegisterFieldRenderer("ModelRenderComponent", "materialGUID",
    [](const char *, void *ptr, Entity, ECSManager &ecs)
    {
        ecs;
        GUID_128 *guid = static_cast<GUID_128 *>(ptr);

        ImGui::Text("Material:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);

        std::string materialPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
        std::string displayText = materialPath.empty() ? "None" : materialPath.substr(materialPath.find_last_of("/\\") + 1);

        EditorComponents::DrawDragDropButton(displayText.c_str(), -1);

        // Material drag-drop target
        if (EditorComponents::BeginDragDropTarget())
        {
            ImGui::SetTooltip("Drop material here to apply to model");
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("MATERIAL_DRAG"))
            {
                // Take snapshot before changing material
                SnapshotManager::GetInstance().TakeSnapshot("Assign Material");
                *guid = DraggedMaterialGuid;
                EditorComponents::EndDragDropTarget();
                return true;
            }
            EditorComponents::EndDragDropTarget();
        }

        return false;
    });

    // Sprite texture GUID
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "textureGUID",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        GUID_128 *guid = static_cast<GUID_128 *>(ptr);

        ImGui::Text("Texture:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);

        std::string texPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
        std::string displayText = texPath.empty() ? "None (Texture)" : texPath.substr(texPath.find_last_of("/\\") + 1);

        float buttonWidth = ImGui::GetContentRegionAvail().x;
        EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

        if (EditorComponents::BeginDragDropTarget())
        {
            ImGui::SetTooltip("Drop texture file here");

            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("TEXTURE_PAYLOAD"))
            {
                // Take snapshot before changing texture
                SnapshotManager::GetInstance().TakeSnapshot("Assign Texture");

                const char *texturePath = (const char *)payload->Data;
                std::string pathStr(texturePath, payload->DataSize);
                pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

                GUID_128 textureGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(pathStr);
                *guid = textureGUID;

                // Load texture immediately
                auto &spriteComp = ecs.GetComponent<SpriteRenderComponent>(entity);
                std::string newTexturePath = AssetManager::GetInstance().GetAssetPathFromGUID(textureGUID);
                spriteComp.texturePath = newTexturePath;
                spriteComp.texture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(textureGUID, newTexturePath);

                EditorComponents::EndDragDropTarget();
                return true; // Field was modified
            }
            EditorComponents::EndDragDropTarget();
        }

        return false;
    });

    // Hide position, scale, rotation from SpriteRenderComponent (controlled by Transform)
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "position",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "scale",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "rotation",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "saved3DPosition",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });

    // Camera skybox texture GUID
    ReflectionRenderer::RegisterFieldRenderer("CameraComponent", "skyboxTextureGUID",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        GUID_128 *guid = static_cast<GUID_128 *>(ptr);

        ImGui::Text("Skybox Texture:");
        ImGui::SameLine();

        std::string texPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
        std::string displayText = texPath.empty() ? "None (Texture)" : texPath.substr(texPath.find_last_of("/\\") + 1);

        bool hasTexture = (guid->high != 0 || guid->low != 0);
        float availableWidth = ImGui::GetContentRegionAvail().x;
        float buttonWidth = hasTexture ? availableWidth - 30.0f : availableWidth;

        ImGui::SetNextItemWidth(buttonWidth);
        EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

        if (EditorComponents::BeginDragDropTarget())
        {
            ImGui::SetTooltip("Drop texture file here");

            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("TEXTURE_PAYLOAD"))
            {
                // Take snapshot before changing texture
                SnapshotManager::GetInstance().TakeSnapshot("Assign Skybox Texture");

                const char *texturePath = (const char *)payload->Data;
                std::string pathStr(texturePath, payload->DataSize);
                pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

                GUID_128 textureGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(pathStr);
                *guid = textureGUID;

                // Load texture immediately
                auto &cameraComp = ecs.GetComponent<CameraComponent>(entity);
                std::string newTexturePath = AssetManager::GetInstance().GetAssetPathFromGUID(textureGUID);
                cameraComp.skyboxTexturePath = newTexturePath;
                cameraComp.skyboxTexture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(textureGUID, newTexturePath);

                EditorComponents::EndDragDropTarget();
                return true; // Field was modified
            }
            EditorComponents::EndDragDropTarget();
        }

        auto &cameraComp = ecs.GetComponent<CameraComponent>(entity);

        if (guid->high != 0 || guid->low != 0)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_XMARK "##ClearSkybox"))
            {
                SnapshotManager::GetInstance().TakeSnapshot("Clear Skybox Texture");

                *guid = GUID_128{0, 0};
                cameraComp.skyboxTexturePath.clear();
                cameraComp.skyboxTexture = nullptr;

                return true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Clear skybox texture");
            }

            if (!cameraComp.skyboxTexture || cameraComp.skyboxTexturePath.empty())
            {
                std::string newTexturePath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
                if (!newTexturePath.empty())
                {
                    cameraComp.skyboxTexturePath = newTexturePath;
                    cameraComp.skyboxTexture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(*guid, newTexturePath);
                }
            }
        }
        else
        {
            if (cameraComp.skyboxTexture != nullptr || !cameraComp.skyboxTexturePath.empty())
            {
                cameraComp.skyboxTexturePath.clear();
                cameraComp.skyboxTexture = nullptr;
            }
        }

        return false;
    });

    // Custom color picker for SpriteRenderComponent
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "color",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        Vector3D *color = static_cast<Vector3D *>(ptr);
        auto &sprite = ecs.GetComponent<SpriteRenderComponent>(entity);

        // Convert to 0-255 range for display, combine with alpha
        float colorRGBA[4] = {
            color->x,
            color->y,
            color->z,
            sprite.alpha};

        ImGui::Text("Color:");
        ImGui::SameLine();

        if (UndoableWidgets::ColorEdit4("##Color", colorRGBA, ImGuiColorEditFlags_Uint8))
        {
            color->x = colorRGBA[0];
            color->y = colorRGBA[1];
            color->z = colorRGBA[2];
            sprite.alpha = colorRGBA[3];
        }

        return true; // Skip default rendering
    });

    // Hide alpha from SpriteRenderComponent (it's in the color picker now)
    ReflectionRenderer::RegisterFieldRenderer("SpriteRenderComponent", "alpha",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });

    // Particle texture GUID
    ReflectionRenderer::RegisterFieldRenderer("ParticleComponent", "textureGUID",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        GUID_128 *guid = static_cast<GUID_128 *>(ptr);

        ImGui::Text("Texture:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);

        std::string texPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
        std::string displayText = texPath.empty() ? "None (Texture)" : texPath.substr(texPath.find_last_of("/\\") + 1);

        float buttonWidth = ImGui::GetContentRegionAvail().x;
        EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

        if (EditorComponents::BeginDragDropTarget())
        {
            ImGui::SetTooltip("Drop texture file here");

            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("TEXTURE_PAYLOAD"))
            {
                // Take snapshot before changing texture
                SnapshotManager::GetInstance().TakeSnapshot("Assign Texture");

                const char *texturePath = (const char *)payload->Data;
                std::string pathStr(texturePath, payload->DataSize);
                pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

                GUID_128 textureGUID = AssetManager::GetInstance().GetGUID128FromAssetMeta(pathStr);
                *guid = textureGUID;

                // Load texture immediately
                auto &particleComp = ecs.GetComponent<ParticleComponent>(entity);
                std::string newTexturePath = AssetManager::GetInstance().GetAssetPathFromGUID(textureGUID);
                particleComp.particleTexture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(textureGUID, newTexturePath);

                EditorComponents::EndDragDropTarget();
                return true; // Field was modified
            }
            EditorComponents::EndDragDropTarget();
        }

        return false;
    });

    // Text font GUID
    ReflectionRenderer::RegisterFieldRenderer("TextRenderComponent", "fontGUID",
    [](const char *, void *ptr, Entity, ECSManager &ecs)
    {
        ecs;
        GUID_128 *guid = static_cast<GUID_128 *>(ptr);

        ImGui::Text("Font:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);

        std::string fontPath = AssetManager::GetInstance().GetAssetPathFromGUID(*guid);
        std::string displayText = fontPath.empty() ? "None" : fontPath.substr(fontPath.find_last_of("/\\") + 1);

        ImGui::Button(displayText.c_str(), ImVec2(-1, 0));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_FONT"))
            {
                // Take snapshot before changing font
                SnapshotManager::GetInstance().TakeSnapshot("Assign Font");
                *guid = DraggedFontGuid;
                ImGui::EndDragDropTarget();
                return true;
            }
            ImGui::EndDragDropTarget();
        }

        return false;
    });

    // Audio GUID
    ReflectionRenderer::RegisterComponentRenderer("AudioComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
        ecs;
        AudioComponent &audio = *static_cast<AudioComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Audio Resource field
        ImGui::Text("Audio File:");
        ImGui::SameLine(labelWidth);
        std::string audioPath = AssetManager::GetInstance().GetAssetPathFromGUID(audio.audioGUID);
        std::string displayText = audioPath.empty() ? "None (Audio File)" : audioPath.substr(audioPath.find_last_of("/\\") + 1);
        float buttonWidth = ImGui::GetContentRegionAvail().x;
        EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("AUDIO_DRAG"))
            {
                // Take snapshot before changing audio clip
                SnapshotManager::GetInstance().TakeSnapshot("Assign Audio Clip");
                audio.SetClip(DraggedAudioGuid);
                ImGui::EndDragDropTarget();
                return true;
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::Separator();

        // Output section
        ImGui::Text("Output");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        char outputBuf[128];
        std::snprintf(outputBuf, sizeof(outputBuf), "%s", audio.OutputAudioMixerGroup.empty() ? "None (Audio Mixer Group)" : audio.OutputAudioMixerGroup.c_str());
        if (UndoableWidgets::InputText("##Output", outputBuf, sizeof(outputBuf)))
        {
            audio.OutputAudioMixerGroup = outputBuf;
        }

        // Checkboxes (aligned with labels) - using UndoableWidgets
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Mute");
        ImGui::SameLine(labelWidth);
        UndoableWidgets::Checkbox("##Mute", &audio.Mute);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Bypass Listener Effects");
        ImGui::SameLine(labelWidth);
        UndoableWidgets::Checkbox("##BypassListenerEffects", &audio.bypassListenerEffects);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Play On Awake");
        ImGui::SameLine(labelWidth);
        UndoableWidgets::Checkbox("##PlayOnAwake", &audio.PlayOnAwake);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Loop");
        ImGui::SameLine(labelWidth);
        UndoableWidgets::Checkbox("##Loop", &audio.Loop);

        ImGui::Separator();

        // Priority (editable drag)
        EditorComponents::DrawSliderWithInput("Priority", &audio.Priority, 0, 256, true, labelWidth);
        // Volume (editable drag)
        EditorComponents::DrawSliderWithInput("Volume", &audio.Volume, 0.0f, 1.0f, false, labelWidth);
        // Pitch (editable drag)
        EditorComponents::DrawSliderWithInput("Pitch", &audio.Pitch, 0.1f, 3.0f, false, labelWidth);
        // Stereo Pan (editable drag)
        EditorComponents::DrawSliderWithInput("Stereo Pan", &audio.StereoPan, -1.0f, 1.0f, false, labelWidth);
        // Reverb Zone Mix (editable drag)
        EditorComponents::DrawSliderWithInput("Reverb Zone Mix", &audio.reverbZoneMix, 0.0f, 1.0f, false, labelWidth);

        // 3D Sound Settings (collapsible)
        if (ImGui::CollapsingHeader("3D Sound Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();

            ImGui::Text("Spatialize");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            UndoableWidgets::Checkbox("##Spatialize", &audio.Spatialize);

            if (audio.Spatialize)
            {
                // Spatial Blend (editable dra with undo support)
                if (EditorComponents::DrawSliderWithInput("Spatial Blend", &audio.SpatialBlend, 0.0f, 1.0f, false, labelWidth))
                {
                    audio.SetSpatialBlend(audio.SpatialBlend);
                }

                // Doppler Level (editable drag)
                EditorComponents::DrawSliderWithInput("Doppler Level", &audio.DopplerLevel, 0.0f, 5.0f, false, labelWidth);

                //// Spread (placeholder, editable drag)
                // static float spread = 0.0f;
                // ImGui::Text("Spread");
                // ImGui::SameLine(labelWidth);
                // ImGui::SetNextItemWidth(-1);
                // ImGui::DragInt("##Spread", reinterpret_cast<int*>(&spread), 1.0f, 0, 360);

                //// Volume Rolloff dropdown
                // ImGui::Text("Volume Rolloff");
                // ImGui::SameLine(labelWidth);
                // ImGui::SetNextItemWidth(-1);
                // const char* rolloffModes[] = { "Logarithmic Rolloff", "Linear Rolloff", "Custom Rolloff" };
                // static int currentRolloff = 0;
                // EditorComponents::PushComboColors();
                // ImGui::Combo("##VolumeRolloff", &currentRolloff, rolloffModes, 3);
                // EditorComponents::PopComboColors();

                // Min Distance (editable drag)
                ImGui::Text("Min Distance");
                ImGui::SameLine(labelWidth);
                ImGui::SetNextItemWidth(-1);
                UndoableWidgets::DragFloat("##MinDistance", &audio.MinDistance, 0.1f, 0.0f, audio.MaxDistance, "%.2f");

                // Max Distance (editable drag)
                ImGui::Text("Max Distance");
                ImGui::SameLine(labelWidth);
                ImGui::SetNextItemWidth(-1);
                UndoableWidgets::DragFloat("##MaxDistance", &audio.MaxDistance, 0.1f, audio.MinDistance, 10000.0f, "%.2f");
            }
            ImGui::Unindent();
        }
        return true; // Skip default rendering
    });

    ReflectionRenderer::RegisterFieldRenderer("AudioListenerComponent", "isMainListener",
    [](const char *, void *, Entity, ECSManager &ecs)
    {
        ecs;
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("If enabled, this Audio Listener will be the primary listener for 3D audio rendering.");
        }
        return false;
    });

    // ==================== AUDIO REVERB ZONE COMPONENT ====================
    ReflectionRenderer::RegisterComponentRenderer("AudioReverbZoneComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
        ecs;
        AudioReverbZoneComponent &reverbZone = *static_cast<AudioReverbZoneComponent *>(componentPtr);
        const float labelWidth = EditorComponents::GetLabelWidth();

        // Enabled checkbox
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Enabled");
        ImGui::SameLine(labelWidth);
        ImGui::Checkbox("##Enabled", &reverbZone.enabled);

        ImGui::Separator();

        // Zone Distance Settings
        ImGui::Text("Zone Distance");
        ImGui::Spacing();

        // Min Distance (editable drag)
        ImGui::Text("Min Distance");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##MinDistance", &reverbZone.MinDistance, 0.1f, 0.0f, reverbZone.MaxDistance, "%.2f"))
        {
            reverbZone.MinDistance = std::max(0.0f, reverbZone.MinDistance);
        }

        // Max Distance (editable drag)
        ImGui::Text("Max Distance");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##MaxDistance", &reverbZone.MaxDistance, 0.1f, reverbZone.MinDistance, 10000.0f, "%.2f"))
        {
            reverbZone.MaxDistance = std::max(reverbZone.MinDistance, reverbZone.MaxDistance);
        }

        ImGui::Separator();

        // Reverb Preset Dropdown
        ImGui::Text("Reverb Preset");
        ImGui::SameLine(labelWidth);
        ImGui::SetNextItemWidth(-1);

        const char *presetNames[] = {
            "Off", "Generic", "Padded Cell", "Room", "Bathroom", "Living Room",
            "Stone Room", "Auditorium", "Concert Hall", "Cave", "Arena", "Hangar",
            "Carpetted Hallway", "Hallway", "Stone Corridor", "Alley", "Forest",
            "City", "Mountains", "Quarry", "Plain", "Parking Lot", "Sewer Pipe",
            "Underwater", "Drugged", "Dizzy", "Psychotic", "Custom"};

        int currentPresetIndex = reverbZone.reverbPresetIndex;
        EditorComponents::PushComboColors();
        if (ImGui::Combo("##ReverbPreset", &currentPresetIndex, presetNames, IM_ARRAYSIZE(presetNames)))
        {
            reverbZone.SetReverbPresetByIndex(currentPresetIndex);
        }
        EditorComponents::PopComboColors();

        ImGui::Separator();

        // Advanced Reverb Parameters (collapsible)
        if (ImGui::CollapsingHeader("Advanced Reverb Parameters", ImGuiTreeNodeFlags_None))
        {
            ImGui::Indent();

            ImGui::Text("Decay Time (s)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##DecayTime", &reverbZone.decayTime, 0.01f, 0.1f, 20.0f, "%.2f");

            ImGui::Text("Early Delay (s)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##EarlyDelay", &reverbZone.earlyDelay, 0.001f, 0.0f, 0.3f, "%.3f");

            ImGui::Text("Late Delay (s)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##LateDelay", &reverbZone.lateDelay, 0.001f, 0.0f, 0.1f, "%.3f");

            ImGui::Text("HF Reference (Hz)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##HFReference", &reverbZone.hfReference, 10.0f, 20.0f, 20000.0f, "%.0f");

            ImGui::Text("HF Decay Ratio");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##HFDecayRatio", &reverbZone.hfDecayRatio, 0.01f, 0.1f, 2.0f, "%.2f");

            ImGui::Text("Diffusion (%)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##Diffusion", &reverbZone.diffusion, 1.0f, 0.0f, 100.0f, "%.0f");

            ImGui::Text("Density (%)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##Density", &reverbZone.density, 1.0f, 0.0f, 100.0f, "%.0f");

            ImGui::Text("Low Shelf Freq (Hz)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##LowShelfFreq", &reverbZone.lowShelfFrequency, 10.0f, 20.0f, 1000.0f, "%.0f");

            ImGui::Text("Low Shelf Gain (dB)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##LowShelfGain", &reverbZone.lowShelfGain, 0.1f, -36.0f, 12.0f, "%.1f");

            ImGui::Text("High Cut (Hz)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##HighCut", &reverbZone.highCut, 10.0f, 20.0f, 20000.0f, "%.0f");

            ImGui::Text("Early/Late Mix (%)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##EarlyLateMix", &reverbZone.earlyLateMix, 1.0f, 0.0f, 100.0f, "%.0f");

            ImGui::Text("Wet Level (dB)");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##WetLevel", &reverbZone.wetLevel, 0.1f, -80.0f, 20.0f, "%.1f");

            ImGui::Unindent();
        }

        // Note about preset changes
        if (reverbZone.reverbPresetIndex != static_cast<int>(AudioReverbZoneComponent::ReverbPreset::Custom))
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Note: Changing advanced parameters will set preset to 'Custom'");
        }

        return true; // Skip default rendering
    });

    // ==================== PARTICLE COMPONENT ====================
    // Add Play/Pause/Stop buttons at the beginning of ParticleComponent rendering

    ReflectionRenderer::RegisterComponentRenderer("ParticleComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
        ecs;
        ParticleComponent &particle = *static_cast<ParticleComponent *>(componentPtr);

        // Play/Pause/Stop buttons for editor preview
        float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        if (EditorComponents::DrawPlayButton(particle.isPlayingInEditor && !particle.isPausedInEditor, buttonWidth))
        {
            SnapshotManager::GetInstance().TakeSnapshot("Play Particles");
            particle.isPlayingInEditor = true;
            particle.isPausedInEditor = false;
        }

        ImGui::SameLine();

        if (EditorComponents::DrawPauseButton(particle.isPausedInEditor, buttonWidth))
        {
            if (particle.isPlayingInEditor)
            {
                SnapshotManager::GetInstance().TakeSnapshot("Pause Particles");
                particle.isPausedInEditor = !particle.isPausedInEditor;
            }
        }

        if (EditorComponents::DrawStopButton())
        {
            SnapshotManager::GetInstance().TakeSnapshot("Stop Particles");
            particle.isPlayingInEditor = false;
            particle.isPausedInEditor = false;
            particle.particles.clear();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Show active particle count
        ImGui::Text("Active Particles: %zu / %d", particle.particles.size(), particle.maxParticles);

        // Is Emitting checkbox (not in reflection, so we render it manually)
        UndoableWidgets::Checkbox("Is Emitting", &particle.isEmitting);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Whether the particle system is actively emitting new particles");
        }

        ImGui::Separator();

        // Continue with normal field rendering
        return false; // Return false to continue with default field rendering
    });

    // ==================== DIRECTIONAL LIGHT COMPONENT ====================

    ReflectionRenderer::RegisterComponentRenderer("DirectionalLightComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
            ecs;
        DirectionalLightComponent &light = *static_cast<DirectionalLightComponent *>(componentPtr);

        // Basic properties with automatic undo/redo
        UndoableWidgets::Checkbox("Enabled", &light.enabled);

        // Color and Intensity with automatic undo/redo
        UndoableWidgets::ColorEdit3("Color", &light.color.x);
        UndoableWidgets::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 10.0f);

        // Note: Direction is controlled via Transform rotation
        ImGui::Separator();
        ImGui::Text("Lighting Properties");
        UndoableWidgets::ColorEdit3("Ambient", &light.ambient.x);
        UndoableWidgets::ColorEdit3("Diffuse", &light.diffuse.x);
        UndoableWidgets::ColorEdit3("Specular", &light.specular.x);

        return true; // Return true to skip default field rendering
    });

    // ==================== POINT LIGHT COMPONENT ====================

    ReflectionRenderer::RegisterComponentRenderer("PointLightComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
            ecs;
        PointLightComponent &light = *static_cast<PointLightComponent *>(componentPtr);

        UndoableWidgets::Checkbox("Enabled", &light.enabled);

        // Color and Intensity with automatic undo/redo
        UndoableWidgets::ColorEdit3("Color", &light.color.x);
        UndoableWidgets::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 10.0f);

        ImGui::Separator();
        ImGui::Text("Attenuation");
        UndoableWidgets::DragFloat("Constant", &light.constant, 0.01f, 0.0f, 2.0f);
        UndoableWidgets::DragFloat("Linear", &light.linear, 0.01f, 0.0f, 1.0f);
        UndoableWidgets::DragFloat("Quadratic", &light.quadratic, 0.01f, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::Text("Lighting Properties");
        UndoableWidgets::ColorEdit3("Ambient", &light.ambient.x);
        UndoableWidgets::ColorEdit3("Diffuse", &light.diffuse.x);
        UndoableWidgets::ColorEdit3("Specular", &light.specular.x);

        return true; // Return true to skip default field rendering
    });

    // ==================== SPOT LIGHT COMPONENT ====================

    ReflectionRenderer::RegisterComponentRenderer("SpotLightComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity, ECSManager &ecs)
    {
            ecs;
        SpotLightComponent &light = *static_cast<SpotLightComponent *>(componentPtr);

        UndoableWidgets::Checkbox("Enabled", &light.enabled);

        // Color and Intensity with automatic undo/redo
        UndoableWidgets::ColorEdit3("Color", &light.color.x);
        UndoableWidgets::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 10.0f);

        // Note: Direction is controlled via Transform rotation
        ImGui::Separator();
        ImGui::Text("Cone Settings");

        // Convert from cosine to degrees for easier editing
        float cutOffDegrees = glm::degrees(glm::acos(light.cutOff));
        float outerCutOffDegrees = glm::degrees(glm::acos(light.outerCutOff));

        if (UndoableWidgets::DragFloat("Inner Cutoff (degrees)", &cutOffDegrees, 1.0f, 0.0f, 90.0f))
        {
            light.cutOff = glm::cos(glm::radians(cutOffDegrees));
        }
        if (UndoableWidgets::DragFloat("Outer Cutoff (degrees)", &outerCutOffDegrees, 1.0f, 0.0f, 90.0f))
        {
            light.outerCutOff = glm::cos(glm::radians(outerCutOffDegrees));
        }

        ImGui::Separator();
        ImGui::Text("Attenuation");
        UndoableWidgets::DragFloat("Constant", &light.constant, 0.01f, 0.0f, 2.0f);
        UndoableWidgets::DragFloat("Linear", &light.linear, 0.01f, 0.0f, 1.0f);
        UndoableWidgets::DragFloat("Quadratic", &light.quadratic, 0.01f, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::Text("Lighting Properties");
        UndoableWidgets::ColorEdit3("Ambient", &light.ambient.x);
        UndoableWidgets::ColorEdit3("Diffuse", &light.diffuse.x);
        UndoableWidgets::ColorEdit3("Specular", &light.specular.x);

        return true;
    });

    ReflectionRenderer::RegisterComponentRenderer("AnimationComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
            ecs;
        AnimationComponent &animComp = *static_cast<AnimationComponent *>(componentPtr);

        enum class PreviewState
        {
            Stopped,
            Playing,
            Paused
        };
        static std::unordered_map<Entity, PreviewState> previewState;

        if (previewState.find(entity) == previewState.end())
        {
            previewState[entity] = PreviewState::Stopped;
        }

        // Inspector preview - uses separate editorPreviewTime (doesn't affect runtime)
        if (EditorState::GetInstance().GetState() == EditorState::State::EDIT_MODE)
        {
            if (previewState[entity] == PreviewState::Playing && animComp.enabled)
            {
                Animator *animator = animComp.GetAnimatorPtr();
                if (animator && !animComp.GetClips().empty())
                {
                    const auto& clips = animComp.GetClips();
                    size_t activeClipIndex = animComp.GetActiveClipIndex();

                    if (activeClipIndex < clips.size())
                    {
                        const Animation& clip = *clips[activeClipIndex];
                        float tps = clip.GetTicksPerSecond();
                        if (tps <= 0.0f) tps = 25.0f;

                        // Update preview time
                        animComp.editorPreviewTime += tps * ImGui::GetIO().DeltaTime * animComp.speed;

                        // Handle looping
                        float duration = clip.GetDuration();
                        if (animComp.isLoop)
                        {
                            animComp.editorPreviewTime = fmod(animComp.editorPreviewTime, duration);
                        }
                        else
                        {
                            if (animComp.editorPreviewTime > duration)
                            {
                                animComp.editorPreviewTime = duration;
                                previewState[entity] = PreviewState::Paused;
                            }
                        }

                        // Set animator time for visualization (doesn't persist to runtime)
                        animator->SetCurrentTime(animComp.editorPreviewTime);
                    }
                }
            }
            else if (previewState[entity] == PreviewState::Paused || previewState[entity] == PreviewState::Stopped)
            {
                // When paused or stopped, keep animator at preview time for visualization
                Animator *animator = animComp.GetAnimatorPtr();
                if (animator && !animComp.GetClips().empty())
                {
                    animator->SetCurrentTime(animComp.editorPreviewTime);
                }
            }
        }

        ImGui::Text("Animation Clips");

        int prevClipCount = animComp.clipCount;
        if (UndoableWidgets::InputInt("Size", &animComp.clipCount, 1, 1))
        {
            if (animComp.clipCount < 0)
                animComp.clipCount = 0;
            if (animComp.clipCount != prevClipCount)
            {
                animComp.SetClipCount(animComp.clipCount);
            }
        }

        for (int i = 0; i < animComp.clipCount; ++i)
        {
            ImGui::PushID(i);

            std::string slotLabel = "Element " + std::to_string(i);
            ImGui::Text("%s", slotLabel.c_str());
            ImGui::SameLine();

            std::string clipName = animComp.clipPaths[i].empty() ? "None (Animation)" : animComp.clipPaths[i];

            size_t lastSlash = clipName.find_last_of("/\\");
            if (lastSlash != std::string::npos)
            {
                clipName = clipName.substr(lastSlash + 1);
            }

            float buttonWidth = ImGui::GetContentRegionAvail().x;
            EditorComponents::DrawDragDropButton(clipName.c_str(), buttonWidth);

            if (EditorComponents::BeginDragDropTarget())
            {
                ImGui::SetTooltip("Drop .fbx animation file here");

                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("MODEL_DRAG"))
                {
                    // Take snapshot before changing animation clip
                    SnapshotManager::GetInstance().TakeSnapshot("Assign Animation Clip");
                    animComp.clipPaths[i] = DraggedModelPath;
                    animComp.clipGUIDs[i] = DraggedModelGuid;

                    if (ecs.HasComponent<ModelRenderComponent>(entity))
                    {
                        auto &modelComp = ecs.GetComponent<ModelRenderComponent>(entity);
                        if (modelComp.model)
                        {
                            // Load animation clips from paths
                            animComp.LoadClipsFromPaths(modelComp.model->GetBoneInfoMap(), modelComp.model->GetBoneCount());

                            // CRITICAL: Link animator to model (same as AnimationSystem::Initialise)
                            Animator* animator = animComp.EnsureAnimator();
                            modelComp.SetAnimator(animator);

                            // If clips were loaded successfully, set up the animator
                            if (!animComp.GetClips().empty()) {
                                animComp.GetAnimatorPtr()->PlayAnimation(animComp.GetClips()[animComp.GetActiveClipIndex()].get());
                            }
                        }
                    }
                }
                EditorComponents::EndDragDropTarget();
            }

            if (!animComp.clipPaths[i].empty())
            {
                ImGui::SameLine();
                ImGui::PushID("clear");
                if (ImGui::SmallButton(ICON_FA_XMARK))
                {
                    animComp.clipPaths[i].clear();
                    animComp.clipGUIDs[i] = {0, 0};

                    if (ecs.HasComponent<ModelRenderComponent>(entity))
                    {
                        auto &modelComp = ecs.GetComponent<ModelRenderComponent>(entity);
                        if (modelComp.model)
                        {
                            // Reload clips from paths
                            animComp.LoadClipsFromPaths(modelComp.model->GetBoneInfoMap(), modelComp.model->GetBoneCount());

                            // Update animator link
                            if (!animComp.GetClips().empty()) {
                                Animator* animator = animComp.EnsureAnimator();
                                modelComp.SetAnimator(animator);
                                animComp.GetAnimatorPtr()->PlayAnimation(animComp.GetClips()[animComp.GetActiveClipIndex()].get());
                            } else {
                                // No clips left, unlink animator
                                modelComp.SetAnimator(nullptr);
                            }
                        }
                    }
                }
                ImGui::PopID();
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Clear Animation");
                }
            }

            ImGui::PopID();
        }

        const auto &clips = animComp.GetClips();
        size_t activeClipIndex = animComp.GetActiveClipIndex();

        if (!clips.empty())
        {
            ImGui::Separator();
            ImGui::Text("Active Clip");

            int currentClip = static_cast<int>(activeClipIndex);
            if (ImGui::SliderInt("##ActiveClip", &currentClip, 0, static_cast<int>(clips.size()) - 1))
            {
                animComp.SetClip(currentClip);
            }

            const Animation &clip = animComp.GetClip(activeClipIndex);
            ImGui::Text("Duration: %.2f ticks", clip.GetDuration());
            ImGui::Text("Ticks Per Second: %.2f", clip.GetTicksPerSecond());
        }

        ImGui::Separator();
        ImGui::Text("Playback Controls (Preview Only)");

        bool isEditMode = (EditorState::GetInstance().GetState() == EditorState::State::EDIT_MODE);
        ImGui::BeginDisabled(!isEditMode);

        float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        bool isPlaying = (previewState[entity] == PreviewState::Playing);

        if (EditorComponents::DrawPlayButton(isPlaying, buttonWidth))
        {
            previewState[entity] = PreviewState::Playing;
            // Preview continues from current editorPreviewTime
        }

        ImGui::SameLine();

        if (EditorComponents::DrawPauseButton(!isPlaying, buttonWidth))
        {
            previewState[entity] = PreviewState::Paused;
        }

        if (EditorComponents::DrawStopButton())
        {
            previewState[entity] = PreviewState::Stopped;
            animComp.ResetPreview(); // Reset preview time to 0
        }

        ImGui::EndDisabled();

        if (!clips.empty() && activeClipIndex < clips.size())
        {
            const Animator *animator = animComp.GetAnimatorPtr();
            if (animator)
            {
                float currentTime = animator->GetCurrentTime();
                const Animation &clip = animComp.GetClip(activeClipIndex);
                float duration = clip.GetDuration();

                ImGui::Separator();
                ImGui::Text("Current Time: %.2f / %.2f", currentTime, duration);

                float progress = duration > 0.0f ? (currentTime / duration) : 0.0f;
                ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
            }
        }

        ImGui::Separator();

        return false;
    });

    ReflectionRenderer::RegisterComponentRenderer("BrainComponent",
    [](void *componentPtr, TypeDescriptor_Struct *, Entity entity, ECSManager &ecs)
    {
            ecs;
        BrainComponent &brain = *static_cast<BrainComponent *>(componentPtr);

        // Combo for Kind
        static const char *kKinds[] = {"None", "Grunt", "Boss"};
        int kindIdx = static_cast<int>(brain.kind);
        if (ImGui::Combo("Kind", &kindIdx, kKinds, IM_ARRAYSIZE(kKinds)))
        {
            brain.kind = static_cast<BrainKind>(kindIdx);
            brain.kindInt = kindIdx;
            // Mark as needing rebuild (optional UX)
        }

        // Read-only current state
        ImGui::Text("Active State: %s", brain.activeState.empty() ? "None" : brain.activeState.c_str());

        // Build / Rebuild
        if (ImGui::Button(brain.impl && brain.started ? "Rebuild" : "Build"))
        {
            if (brain.impl && brain.started)
                brain.impl->onExit(ecs, entity);

            brain.enabled = true; // ensure init system will start it
            brain.impl.reset();
            brain.started = false;

            brain.impl = game_ai::CreateFor(ecs, entity, brain.kind); // optional pre-create
        }

        // Stop
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
        {
            if (brain.impl && brain.started)
                brain.impl->onExit(ecs, entity);

            brain.enabled = false; // <-- prevents re-entry
            brain.impl.reset();
            brain.started = false;
            brain.activeState.clear(); // shows "None"
        }

        return true;
    });

    // ==================== SCRIPT COMPONENT ====================
    // Custom renderer for ScriptComponentData scriptPath field with drag-drop support

    ReflectionRenderer::RegisterFieldRenderer("ScriptComponentData", "scriptPath",
    [](const char *, void *ptr, Entity entity, ECSManager &ecs)
    {
        ecs;
        std::string *scriptPath = static_cast<std::string *>(ptr);

        ImGui::Text("Script:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);

        // Display the script file name (or "None" if empty)
        std::string displayText = scriptPath->empty() ? "None (Lua Script)" :
                                  scriptPath->substr(scriptPath->find_last_of("/\\") + 1);

        float buttonWidth = ImGui::GetContentRegionAvail().x;
        EditorComponents::DrawDragDropButton(displayText.c_str(), buttonWidth);

        if (!scriptPath->empty() && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            std::filesystem::path absolutePath = std::filesystem::absolute(*scriptPath);
            std::string command;
            #ifdef _WIN32
                command = "code \"" + absolutePath.string() + "\"";
            #elif __linux__
                command = "code \"" + absolutePath.string() + "\" &";
            #elif __APPLE__
                command = "code \"" + absolutePath.string() + "\"";
            #endif

            system(command.c_str());
        }

        if (ImGui::IsItemHovered() && !scriptPath->empty())
        {
            ImGui::SetTooltip("Double-click to open in VS Code");
        }

        // Handle drag-drop from asset browser
        if (ImGui::BeginDragDropTarget())
        {
            ImGui::SetTooltip("Drop .lua script here to assign");

            // Accept both SCRIPT_PAYLOAD and direct path payload
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SCRIPT_PAYLOAD"))
            {
                // Take snapshot before changing script
                SnapshotManager::GetInstance().TakeSnapshot("Assign Script");

                const char *droppedPath = (const char *)payload->Data;
                std::string pathStr(droppedPath, payload->DataSize);
                pathStr.erase(std::find(pathStr.begin(), pathStr.end(), '\0'), pathStr.end());

                *scriptPath = pathStr;

                // Notify the ScriptSystem that the script has changed
                // The system will handle reloading on next update
                auto &scriptData = ecs.GetComponent<ScriptComponentData>(entity);
                scriptData.instanceCreated = false;  // Force recreation
                scriptData.instanceId = -1;

                ImGui::EndDragDropTarget();
                return true; // Field was modified
            }
            ImGui::EndDragDropTarget();
        }

        // Add a small "Clear" button next to the script field
        if (!scriptPath->empty())
        {
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_XMARK "##ClearScript"))
            {
                SnapshotManager::GetInstance().TakeSnapshot("Clear Script");
                scriptPath->clear();

                auto &scriptData = ecs.GetComponent<ScriptComponentData>(entity);
                scriptData.instanceCreated = false;
                scriptData.instanceId = -1;

                return true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Clear script");
            }

            // Add an "Open" button to edit the script in external editor
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_PEN_TO_SQUARE "##EditScript"))
            {
                #ifdef _WIN32
                    std::string command = "start \"\" \"" + *scriptPath + "\"";
                    system(command.c_str());
                #elif __linux__
                    std::string command = "xdg-open \"" + *scriptPath + "\"";
                    system(command.c_str());
                #elif __APPLE__
                    std::string command = "open \"" + *scriptPath + "\"";
                    system(command.c_str());
                #endif
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Open script in external editor");
            }
        }

        return true; // Skip default rendering
    });

    // Hide internal/runtime fields from inspector
    ReflectionRenderer::RegisterFieldRenderer("ScriptComponentData", "instanceId",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("ScriptComponentData", "instanceCreated",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });
    ReflectionRenderer::RegisterFieldRenderer("ScriptComponentData", "pendingInstanceState",
                                              [](const char *, void *, Entity, ECSManager &)
                                              { return true; });
}
