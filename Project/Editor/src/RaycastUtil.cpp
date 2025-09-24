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

RaycastUtil::RaycastHit RaycastUtil::RaycastScene(const Ray& ray) {
    RaycastHit closestHit;

    try {
        // Get the active ECS manager
        ECSRegistry& registry = ECSRegistry::GetInstance();
        ECSManager& ecsManager = registry.GetActiveECSManager();

        std::cout << "[RaycastUtil] Ray origin: (" << ray.origin.x << ", " << ray.origin.y << ", " << ray.origin.z
                  << ") direction: (" << ray.direction.x << ", " << ray.direction.y << ", " << ray.direction.z << ")" << std::endl;

        int entitiesWithComponent = 0;

        // Test against entities 0-50, looking for Transform components
        for (Entity entity = 0; entity <= 50; ++entity) {
            // Check if entity has Transform component
            if (!ecsManager.HasComponent<Transform>(entity)) {
                continue;  // Skip if entity has no transform
            }

            try {
                auto& transform = ecsManager.GetComponent<Transform>(entity);

                entitiesWithComponent++;
                std::cout << "[RaycastUtil] Found entity " << entity << " with Transform component" << std::endl;

                // Create AABB from the entity's transform
                AABB entityAABB = CreateAABBFromTransform(transform.model);

                std::cout << "[RaycastUtil] Entity " << entity << " AABB: min("
                          << entityAABB.min.x << ", " << entityAABB.min.y << ", " << entityAABB.min.z
                          << ") max(" << entityAABB.max.x << ", " << entityAABB.max.y << ", " << entityAABB.max.z << ")" << std::endl;

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
                std::cerr << "[RaycastUtil] Error processing entity " << entity << ": " << e.what() << std::endl;
                continue;
            }
        }

        std::cout << "[RaycastUtil] Tested " << entitiesWithComponent << " entities with Transform components" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[RaycastUtil] Error during raycast: " << e.what() << std::endl;
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
            outMatrix[0]  = transform.model.m.m00; outMatrix[4]  = transform.model.m.m01; outMatrix[8]  = transform.model.m.m02; outMatrix[12] = transform.model.m.m03;
            outMatrix[1]  = transform.model.m.m10; outMatrix[5]  = transform.model.m.m11; outMatrix[9]  = transform.model.m.m12; outMatrix[13] = transform.model.m.m13;
            outMatrix[2]  = transform.model.m.m20; outMatrix[6]  = transform.model.m.m21; outMatrix[10] = transform.model.m.m22; outMatrix[14] = transform.model.m.m23;
            outMatrix[3]  = transform.model.m.m30; outMatrix[7]  = transform.model.m.m31; outMatrix[11] = transform.model.m.m32; outMatrix[15] = transform.model.m.m33;

            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "[RaycastUtil] Error getting transform for entity " << entity << ": " << e.what() << std::endl;
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
            transform.position = newPosition;
            transform.scale = newScale;  // Make sure scale is updated too
            transform.model = newMatrix;

            // Update last known values to prevent TransformSystem from recalculating
            transform.lastPosition = newPosition;
            transform.lastScale = newScale;

            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "[RaycastUtil] Error setting transform for entity " << entity << ": " << e.what() << std::endl;
    }

    return false;
}

