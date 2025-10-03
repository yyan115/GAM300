#include "RaycastUtil.hpp"
#include <algorithm>
#include <limits>
#include <iostream>
#include <optional>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>

// Include ECS system from Engine (using configured include paths)
#include "ECS/ECSRegistry.hpp"
#include "Transform/TransformComponent.hpp"
#include "Math/Vector3D.hpp"
#include "Logging.hpp"

RaycastUtil::Ray RaycastUtil::ScreenToWorldRay(float mouseX, float mouseY,
                                              float screenWidth, float screenHeight,
                                              const Matrix4x4& viewMatrix, const Matrix4x4& projMatrix) {
    // Normalize screen coordinates to NDC [-1, 1]
    float x = (2.0f * mouseX) / screenWidth - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / screenHeight;  // Flip Y axis

    // Create points in NDC space (near and far plane)
    Vector3D rayStartNDC(x, y, -1.0f);  // Near plane
    Vector3D rayEndNDC(x, y, 1.0f);     // Far plane

    // Transform to world space
    Matrix4x4 invView = viewMatrix.Inversed();
    Matrix4x4 invProj = projMatrix.Inversed();
    Matrix4x4 invViewProj = invView * invProj;

    // Transform points (assuming homogeneous coordinate w=1 for both points)
    Vector3D rayStartWorld = invViewProj.TransformPoint(rayStartNDC);
    Vector3D rayEndWorld = invViewProj.TransformPoint(rayEndNDC);

    // Create ray
    glm::vec3 rayOrigin(rayStartWorld.x, rayStartWorld.y, rayStartWorld.z);
    Vector3D direction = rayEndWorld - rayStartWorld;
    glm::vec3 rayDirection = glm::normalize(glm::vec3(direction.x, direction.y, direction.z));

    return Ray(rayOrigin, rayDirection);
}

bool RaycastUtil::RayAABBIntersection(const Ray& ray, const AABB& aabb, float& distance) {
    glm::vec3 invDir = 1.0f / ray.direction;
    glm::vec3 t1 = (aabb.min - ray.origin) * invDir;
    glm::vec3 t2 = (aabb.max - ray.origin) * invDir;

    glm::vec3 tMin = glm::min(t1, t2);
    glm::vec3 tMax = glm::max(t1, t2);

    float tNear = std::max({tMin.x, tMin.y, tMin.z});
    float tFar = std::min({tMax.x, tMax.y, tMax.z});

    // Ray misses the box if tNear > tFar or tFar < 0
    if (tNear > tFar || tFar < 0.0f) {
        return false;
    }

    // Use tNear if it's positive, otherwise use tFar
    distance = (tNear >= 0.0f) ? tNear : tFar;
    return true;
}

RaycastUtil::AABB RaycastUtil::CreateAABBFromTransform(const Matrix4x4& transform,
                                                     const glm::vec3& modelSize) {
    // Extract translation from transform matrix (last column)
    glm::vec3 translation(transform.m.m03, transform.m.m13, transform.m.m23);

    // Extract scale from transform matrix (length of basis vectors)
    glm::vec3 scale;
    scale.y = sqrt(transform.m.m01*transform.m.m01 + transform.m.m11*transform.m.m11 + transform.m.m21*transform.m.m21);
    scale.z = sqrt(transform.m.m02*transform.m.m02 + transform.m.m12*transform.m.m12 + transform.m.m22*transform.m.m22);
    scale.x = sqrt(transform.m.m00*transform.m.m00 + transform.m.m10*transform.m.m10 + transform.m.m20*transform.m.m20);

    // Create AABB around the transformed model
    glm::vec3 halfSize = (modelSize * scale) * 0.5f;

    return AABB(translation - halfSize, translation + halfSize);
}

RaycastUtil::RaycastHit RaycastUtil::RaycastScene(const Ray& ray, Entity excludeEntity) {
    RaycastHit closestHit;

    try {
        // Get the active ECS manager
        ECSRegistry& registry = ECSRegistry::GetInstance();
        ECSManager& ecsManager = registry.GetActiveECSManager();

        ENGINE_PRINT("[RaycastUtil] Ray origin: (" , ray.origin.x , ", " , ray.origin.y , ", " , ray.origin.z
            , ") direction: (" , ray.direction.x , ", " , ray.direction.y , ", " , ray.direction.z , ")\n");

        int entitiesWithComponent = 0;

        // Test against entities 0-50, looking for Transform components
        for (Entity entity = 0; entity <= 50; ++entity) {
            // Skip excluded entity (e.g., preview entity)
            if (entity == excludeEntity) {
                continue;
            }

            // Check if entity has Transform component
            if (!ecsManager.HasComponent<Transform>(entity)) {
                continue;  // Skip if entity has no transform
            }

            try {
                auto& transform = ecsManager.GetComponent<Transform>(entity);

                entitiesWithComponent++;
                ENGINE_PRINT("[RaycastUtil] Found entity " , entity , " with Transform component\n");

                    // Get sprite position from Transform if it exists, otherwise use sprite's position
                    glm::vec3 spritePosition = sprite.position.ConvertToGLM();
                    if (ecsManager.HasComponent<Transform>(entity)) {
                        auto& transform = ecsManager.GetComponent<Transform>(entity);
                        spritePosition = glm::vec3(transform.worldMatrix.m.m03,
                                                   transform.worldMatrix.m.m13,
                                                   transform.worldMatrix.m.m23);
                    }

                    // Create AABB from the sprite's position and scale
                    entityAABB = CreateAABBFromSprite(spritePosition, sprite.scale.ConvertToGLM(), sprite.is3D);
                    hasValidAABB = true;


                // Test ray intersection
                float distance;
                if (RayAABBIntersection(ray, entityAABB, distance)) {
                    // Check if this is the closest hit
                    if (!closestHit.hit || distance < closestHit.distance) {
                        closestHit.hit = true;
                        closestHit.entity = entity;
                        closestHit.distance = distance;
                        closestHit.point = ray.origin + ray.direction * distance;
                    }
                }
            } catch (const std::exception& e) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[RaycastUtil] Error processing entity ", entity, ": ", e.what(), "\n");
                continue;
            }
        }
        ENGINE_PRINT("[RaycastUtil] Tested " , entitiesWithComponent , " entities with Transform components\n");

    } catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[RaycastUtil] Error during raycast: ", e.what(), "\n"); 
    }

    return closestHit;
}

bool RaycastUtil::GetEntityTransform(Entity entity, float outMatrix[16]) {
    try {
        // Get the active ECS manager
        ECSRegistry& registry = ECSRegistry::GetInstance();
        ECSManager& ecsManager = registry.GetActiveECSManager();

        // Check if entity has Transform component
        if (ecsManager.HasComponent<Transform>(entity)) {
            auto& transform = ecsManager.GetComponent<Transform>(entity);

            // Convert Matrix4x4 to column-major float array for ImGuizmo (GLM format)
            // Matrix4x4 is row-major, ImGuizmo expects column-major
            outMatrix[0]  = transform.worldMatrix.m.m00; outMatrix[4]  = transform.worldMatrix.m.m01; outMatrix[8]  = transform.worldMatrix.m.m02; outMatrix[12] = transform.worldMatrix.m.m03;
            outMatrix[1]  = transform.worldMatrix.m.m10; outMatrix[5]  = transform.worldMatrix.m.m11; outMatrix[9]  = transform.worldMatrix.m.m12; outMatrix[13] = transform.worldMatrix.m.m13;
            outMatrix[2]  = transform.worldMatrix.m.m20; outMatrix[6]  = transform.worldMatrix.m.m21; outMatrix[10] = transform.worldMatrix.m.m22; outMatrix[14] = transform.worldMatrix.m.m23;
            outMatrix[3]  = transform.worldMatrix.m.m30; outMatrix[7]  = transform.worldMatrix.m.m31; outMatrix[11] = transform.worldMatrix.m.m32; outMatrix[15] = transform.worldMatrix.m.m33;

            return true;
        }
        // If no Transform, check for SpriteRenderComponent (for sprites without Transform)
        else if (ecsManager.HasComponent<SpriteRenderComponent>(entity)) {
            auto& sprite = ecsManager.GetComponent<SpriteRenderComponent>(entity);

            // In 3D mode: only allow 3D sprites
            // In 2D mode: only allow 2D sprites
            if (!is2DMode && !sprite.is3D) {
                return false;  // Don't provide transform for 2D screen-space sprites in 3D mode
            }
            if (is2DMode && sprite.is3D) {
                return false;  // Don't provide transform for 3D sprites in 2D mode
            }

            // Note: This case should rarely happen since 3D sprites typically have Transform components
            // But if a 3D sprite exists without Transform, use sprite properties

            // For 3D sprites: position is already the center (used directly in TRS transform)
            // Build a transform matrix from position, scale, and rotation
            // Create a TRS (Translation * Rotation * Scale) matrix

            // Convert rotation from degrees to radians
            float rotationRadians = glm::radians(sprite.rotation);

            // Create transformation matrices
            glm::mat4 translation = glm::translate(glm::mat4(1.0f), sprite.position.ConvertToGLM());
            glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), rotationRadians, glm::vec3(0.0f, 0.0f, 1.0f)); // Rotate around Z axis
            glm::mat4 scale = glm::scale(glm::mat4(1.0f), sprite.scale.ConvertToGLM());

            // Combine: TRS order
            glm::mat4 transformMatrix = translation * rotation * scale;

            // Convert glm::mat4 (column-major) to column-major float array for ImGuizmo
            // GLM is already column-major, so we can directly copy
            const float* matrixPtr = glm::value_ptr(transformMatrix);
            for (int i = 0; i < 16; ++i) {
                outMatrix[i] = matrixPtr[i];
            }

            return true;
        }
    } catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[RaycastUtil] Error getting transform for entity ", entity, ": ", e.what(), "\n");
    }

    return false;
}

bool RaycastUtil::SetEntityTransform(Entity entity, const float matrix[16]) {
    try {
        // Get the active ECS manager
        ECSRegistry& registry = ECSRegistry::GetInstance();
        ECSManager& ecsManager = registry.GetActiveECSManager();

        // Check if entity has Transform component
        if (ecsManager.HasComponent<Transform>(entity)) {
            auto& transform = ecsManager.GetComponent<Transform>(entity);

            // Convert column-major float array (ImGuizmo/GLM format) to row-major Matrix4x4
            // ImGuizmo provides column-major, Matrix4x4 is row-major
            Matrix4x4 newMatrix;
            newMatrix.m.m00 = matrix[0];  newMatrix.m.m01 = matrix[4];  newMatrix.m.m02 = matrix[8];   newMatrix.m.m03 = matrix[12];
            newMatrix.m.m10 = matrix[1];  newMatrix.m.m11 = matrix[5];  newMatrix.m.m12 = matrix[9];   newMatrix.m.m13 = matrix[13];
            newMatrix.m.m20 = matrix[2];  newMatrix.m.m21 = matrix[6];  newMatrix.m.m22 = matrix[10];  newMatrix.m.m23 = matrix[14];
            newMatrix.m.m30 = matrix[3];  newMatrix.m.m31 = matrix[7];  newMatrix.m.m32 = matrix[11];  newMatrix.m.m33 = matrix[15];

            // Extract transform components properly
            Vector3D newPosition(newMatrix.m.m03, newMatrix.m.m13, newMatrix.m.m23);

            // Extract scale from the matrix
            Vector3D newScale;
            newScale.x = sqrt(newMatrix.m.m00*newMatrix.m.m00 + newMatrix.m.m10*newMatrix.m.m10 + newMatrix.m.m20*newMatrix.m.m20);
            newScale.y = sqrt(newMatrix.m.m01*newMatrix.m.m01 + newMatrix.m.m11*newMatrix.m.m11 + newMatrix.m.m21*newMatrix.m.m21);
            newScale.z = sqrt(newMatrix.m.m02*newMatrix.m.m02 + newMatrix.m.m12*newMatrix.m.m12 + newMatrix.m.m22*newMatrix.m.m22);

            // Update all components to stay in sync
            ecsManager.transformSystem->SetWorldPosition(entity, newPosition);
            ecsManager.transformSystem->SetWorldScale(entity, newScale);

            //transform.position = newPosition;
            //transform.scale = newScale;  // Make sure scale is updated too
            //transform.worldMatrix = newMatrix;

            //// Update last known values to prevent TransformSystem from recalculating
            //transform.lastPosition = newPosition;
            //transform.lastScale = newScale;

            return true;
        }
        // If no Transform, check for SpriteRenderComponent
        else if (ecsManager.HasComponent<SpriteRenderComponent>(entity)) {
            auto& sprite = ecsManager.GetComponent<SpriteRenderComponent>(entity);

            // In 3D mode: only allow 3D sprites
            // In 2D mode: only allow 2D sprites
            if (!is2DMode && !sprite.is3D) {
                return false;  // Don't apply transform for 2D screen-space sprites in 3D mode
            }
            if (is2DMode && sprite.is3D) {
                return false;  // Don't apply transform for 3D sprites in 2D mode
            }

            // Convert column-major float array (ImGuizmo/GLM format) to glm::mat4
            glm::mat4 transformMatrix;
            const float* matrixPtr = matrix;
            transformMatrix = glm::make_mat4(matrixPtr);

            // Decompose the matrix to get position, rotation, and scale
            glm::vec3 position;
            glm::vec3 scale;
            glm::quat rotation;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(transformMatrix, scale, rotation, position, skew, perspective);

            // Convert quaternion to Euler angles (in radians)
            glm::vec3 eulerAngles = glm::eulerAngles(rotation);

            // For 3D sprites, position is already the center (no conversion needed)
            // Update sprite properties directly
            sprite.position = Vector3D::ConvertGLMToVector3D(position);
            sprite.scale = Vector3D::ConvertGLMToVector3D(scale);
            sprite.rotation = glm::degrees(eulerAngles.z); // Convert back to degrees and use Z rotation

            return true;
        }
    } catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[RaycastUtil] Error setting transform for entity ", entity, ": ", e.what(), "\n");
    }

    return false;
}

