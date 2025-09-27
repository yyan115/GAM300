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
    glm::vec3 translation(transform.m[0][3], transform.m[1][3], transform.m[2][3]);

    // Extract scale from transform matrix (length of basis vectors)
    glm::vec3 scale;
    scale.x = sqrt(transform.m[0][0]*transform.m[0][0] + transform.m[1][0]*transform.m[1][0] + transform.m[2][0]*transform.m[2][0]);
    scale.y = sqrt(transform.m[0][1]*transform.m[0][1] + transform.m[1][1]*transform.m[1][1] + transform.m[2][1]*transform.m[2][1]);
    scale.z = sqrt(transform.m[0][2]*transform.m[0][2] + transform.m[1][2]*transform.m[1][2] + transform.m[2][2]*transform.m[2][2]);

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
            outMatrix[0]  = transform.model.m[0][0]; outMatrix[4]  = transform.model.m[0][1]; outMatrix[8]  = transform.model.m[0][2]; outMatrix[12] = transform.model.m[0][3];
            outMatrix[1]  = transform.model.m[1][0]; outMatrix[5]  = transform.model.m[1][1]; outMatrix[9]  = transform.model.m[1][2]; outMatrix[13] = transform.model.m[1][3];
            outMatrix[2]  = transform.model.m[2][0]; outMatrix[6]  = transform.model.m[2][1]; outMatrix[10] = transform.model.m[2][2]; outMatrix[14] = transform.model.m[2][3];
            outMatrix[3]  = transform.model.m[3][0]; outMatrix[7]  = transform.model.m[3][1]; outMatrix[11] = transform.model.m[3][2]; outMatrix[15] = transform.model.m[3][3];

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
            newMatrix.m[0][0] = matrix[0];  newMatrix.m[0][1] = matrix[4];  newMatrix.m[0][2] = matrix[8];   newMatrix.m[0][3] = matrix[12];
            newMatrix.m[1][0] = matrix[1];  newMatrix.m[1][1] = matrix[5];  newMatrix.m[1][2] = matrix[9];   newMatrix.m[1][3] = matrix[13];
            newMatrix.m[2][0] = matrix[2];  newMatrix.m[2][1] = matrix[6];  newMatrix.m[2][2] = matrix[10];  newMatrix.m[2][3] = matrix[14];
            newMatrix.m[3][0] = matrix[3];  newMatrix.m[3][1] = matrix[7];  newMatrix.m[3][2] = matrix[11];  newMatrix.m[3][3] = matrix[15];

            // Extract transform components properly
            Vector3D newPosition(newMatrix.m[0][3], newMatrix.m[1][3], newMatrix.m[2][3]);

            // Extract scale from the matrix
            Vector3D newScale;
            newScale.x = sqrt(newMatrix.m[0][0]*newMatrix.m[0][0] + newMatrix.m[1][0]*newMatrix.m[1][0] + newMatrix.m[2][0]*newMatrix.m[2][0]);
            newScale.y = sqrt(newMatrix.m[0][1]*newMatrix.m[0][1] + newMatrix.m[1][1]*newMatrix.m[1][1] + newMatrix.m[2][1]*newMatrix.m[2][1]);
            newScale.z = sqrt(newMatrix.m[0][2]*newMatrix.m[0][2] + newMatrix.m[1][2]*newMatrix.m[1][2] + newMatrix.m[2][2]*newMatrix.m[2][2]);

            // Extract rotation from the matrix (remove scale first)
            Matrix4x4 rotationMatrix = Matrix4x4::RemoveScale(newMatrix);
            Quaternion newRotation = Quaternion::FromMatrix(rotationMatrix);
            Vector3D newRotationEuler = newRotation.ToEulerDegrees();

            // Update all components to stay in sync
            ecsManager.transformSystem->SetWorldPosition(entity, newPosition);
            ecsManager.transformSystem->SetWorldScale(entity, newScale);
            ecsManager.transformSystem->SetWorldRotation(entity, newRotationEuler);

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

