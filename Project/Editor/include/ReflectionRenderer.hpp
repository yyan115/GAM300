/* Start Header ************************************************************************/
/*!
\file       ReflectionRenderer.hpp
\author     Lucas Yee
\date       2025
\brief      Utility class for automatic ImGui rendering of reflected components.
            Eliminates hardcoded component rendering by using the reflection system.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once

#include "Reflection/ReflectionBase.hpp"
#include "ECS/Entity.hpp"
#include "imgui.h"
#include <string>
#include <functional>
#include <unordered_map>

class ECSManager;

/**
 * @brief Result of rendering a component, tracking which specific field was modified
 *
 * Unity-style approach: Instead of just returning bool for "was anything modified",
 * we track exactly which field was modified so multi-entity editing only copies
 * that specific field to other selected entities.
 */
struct FieldModificationResult {
    bool wasModified = false;
    std::string modifiedFieldName;  // Empty if not modified, field name if modified

    // Implicit conversion to bool for backward compatibility
    operator bool() const { return wasModified; }
};

/**
 * @brief Automatically renders component fields using reflection metadata
 *
 * This class eliminates the need for hardcoded DrawXComponent() methods by:
 * 1. Iterating over reflected component fields via TypeDescriptor_Struct
 * 2. Mapping type names to appropriate ImGui widgets
 * 3. Handling special cases (Vector3D, GUID_128, enums, etc.)
 */
class ReflectionRenderer {
public:
    /**
     * @brief Renders all reflected fields of a component using ImGui
     * @param componentPtr Pointer to the component instance
     * @param typeDesc Type descriptor from reflection system
     * @param entity The entity that owns this component (for special handlers)
     * @param ecsManager ECS manager for accessing other components if needed
     * @return true if any field was modified
     */
    static bool RenderComponent(void* componentPtr, TypeDescriptor_Struct* typeDesc,
                                Entity entity, ECSManager& ecsManager);

    /**
     * @brief Renders all reflected fields and tracks which specific field was modified (Unity-style)
     * @param componentPtr Pointer to the component instance
     * @param typeDesc Type descriptor from reflection system
     * @param entity The entity that owns this component (for special handlers)
     * @param ecsManager ECS manager for accessing other components if needed
     * @return FieldModificationResult containing modification status and field name
     *
     * Use this for multi-entity editing to only copy the specific modified field.
     */
    static FieldModificationResult RenderComponentWithFieldTracking(void* componentPtr, TypeDescriptor_Struct* typeDesc,
                                                                     Entity entity, ECSManager& ecsManager);

    /**
     * @brief Renders a single field with appropriate ImGui widget
     * @param fieldName Name of the field
     * @param fieldPtr Pointer to the field value
     * @param fieldType Type descriptor of the field
     * @param entity The entity (for context-specific rendering)
     * @param ecsManager ECS manager reference
     * @return true if field was modified
     */
    static bool RenderField(const char* fieldName, void* fieldPtr,
                           TypeDescriptor* fieldType, Entity entity, ECSManager& ecsManager);

    /**
     * @brief Custom field renderer callback type
     * @param fieldName Name of the field
     * @param fieldPtr Pointer to the field value
     * @param entity The entity
     * @param ecsManager ECS manager
     * @return true if field was modified
     */
    using CustomFieldRenderer = std::function<bool(const char*, void*, Entity, ECSManager&)>;

    /**
     * @brief Custom component renderer callback type
     * @param componentPtr Pointer to the component instance
     * @param typeDesc Type descriptor from reflection system
     * @param entity The entity
     * @param ecsManager ECS manager
     * @return true to skip default field rendering, false to continue with field rendering
     */
    using CustomComponentRenderer = std::function<bool(void*, TypeDescriptor_Struct*, Entity, ECSManager&)>;

    /**
     * @brief Register a custom renderer for a specific field type
     * @param typeName Type name (e.g., "GUID_128", "Motion")
     * @param renderer Custom rendering function
     */
    static void RegisterCustomRenderer(const std::string& typeName, CustomFieldRenderer renderer);

    /**
     * @brief Register a custom renderer for a specific component+field combination
     * @param componentType Component type name (e.g., "ColliderComponent")
     * @param fieldName Field name (e.g., "shapeType")
     * @param renderer Custom rendering function
     */
    static void RegisterFieldRenderer(const std::string& componentType,
                                      const std::string& fieldName,
                                      CustomFieldRenderer renderer);

    /**
     * @brief Register a custom renderer for an entire component
     * @param componentType Component type name (e.g., "ParticleComponent")
     * @param renderer Custom rendering function that can render the whole component
     *                 Return true to skip default field rendering, false to append to it
     */
    static void RegisterComponentRenderer(const std::string& componentType,
                                          CustomComponentRenderer renderer);

private:
    // Type-specific renderers
    static bool RenderPrimitive(const char* fieldName, void* fieldPtr, const std::string& typeName);
    static bool RenderVector3D(const char* fieldName, void* fieldPtr);
    static bool RenderQuaternion(const char* fieldName, void* fieldPtr);
    static bool RenderGUID(const char* fieldName, void* fieldPtr, Entity entity, ECSManager& ecsManager);
    static bool RenderString(const char* fieldName, void* fieldPtr);
    static bool RenderStruct(const char* fieldName, void* fieldPtr, TypeDescriptor* fieldType,
                            Entity entity, ECSManager& ecsManager);

    // Registry for custom renderers
    static std::unordered_map<std::string, CustomFieldRenderer>& GetCustomRenderers();
    static std::unordered_map<std::string, CustomFieldRenderer>& GetFieldRenderers();
    static std::unordered_map<std::string, CustomComponentRenderer>& GetComponentRenderers();

    // Helper to create unique field ID for ImGui
    static std::string MakeFieldID(const char* fieldName, const void* fieldPtr);
};

/**
 * @brief Register all Inspector-specific custom field renderers
 *
 * This function registers custom renderers for components that need special handling:
 * - Transform: Uses TransformSystem for SetLocalPosition/Rotation/Scale
 * - ColliderComponent: Shape type dropdown and physics layer dropdown
 * - CameraComponent: Projection type dropdown
 * - GUID fields: Drag-drop support for assets
 *
 * Call this once during editor initialization.
 */
void RegisterInspectorCustomRenderers();
