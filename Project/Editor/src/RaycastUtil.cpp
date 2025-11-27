#include "RaycastUtil.hpp"
#include <algorithm>
#include <limits>
#include <iostream>
#include <optional>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

// Include ECS system from Engine (using configured include paths)
#include "ECS/ECSRegistry.hpp"
#include "Transform/TransformComponent.hpp"
#include "Transform/Quaternion.hpp"
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Graphics/Model/ModelRenderComponent.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Graphics/TextRendering/TextRenderComponent.hpp"
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

RaycastUtil::AABB RaycastUtil::CreateAABBFromSprite(const glm::vec3& position, const glm::vec3& scale, bool is3D) {
	(void)is3D;
    glm::vec3 halfSize = scale * 0.5f;

    // For both 2D and 3D sprites, position is now the center (after rendering fix)
    // The sprite quad is 0,0 to 1,1, and we offset by -0.5,-0.5 in the shader to center it
    glm::vec3 center = position;

    // Create AABB around the sprite's center
    return AABB(center - halfSize, center + halfSize);
}

RaycastUtil::RaycastHit RaycastUtil::RaycastScene(const Ray& ray, Entity excludeEntity, bool filterByMode, bool is2DMode) {
    RaycastHit closestHit;

    try {
        // Get the active ECS manager
        ECSRegistry& registry = ECSRegistry::GetInstance();
        ECSManager& ecsManager = registry.GetActiveECSManager();

        ENGINE_PRINT("[RaycastUtil] Ray origin: (" , ray.origin.x , ", " , ray.origin.y , ", " , ray.origin.z
            , ") direction: (" , ray.direction.x , ", " , ray.direction.y , ", " , ray.direction.z , ")\n");

        int entitiesWithComponent = 0;

        // Test against all active entities, looking for Transform components OR SpriteRenderComponents
        for (auto entity : ecsManager.GetActiveEntities()) {
            // Skip excluded entity (e.g., preview entity)
            if (entity == excludeEntity) {
                continue;
            }

            // Filter entities based on 2D/3D mode (if filtering is enabled)
            if (filterByMode) {
                bool entityIs3D = IsEntity3D(entity);
                if (is2DMode && entityIs3D) {
                    // In 2D mode, skip 3D entities
                    continue;
                }
                if (!is2DMode && !entityIs3D) {
                    // In 3D mode, skip 2D entities
                    continue;
                }
            }

            AABB entityAABB(glm::vec3(0.0f), glm::vec3(0.0f));
            bool hasValidAABB = false;

            try {
                // First, check if entity has SpriteRenderComponent (prioritize sprites for better selection)
                if (ecsManager.HasComponent<SpriteRenderComponent>(entity)) {
                    auto& sprite = ecsManager.GetComponent<SpriteRenderComponent>(entity);

                    entitiesWithComponent++;
                    ENGINE_PRINT("[RaycastUtil] Found entity " , entity , " with SpriteRenderComponent (is3D=", sprite.is3D, ")\n");

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

                    ENGINE_PRINT("[RaycastUtil] Entity ", entity, " (Sprite) AABB: min("
                        , entityAABB.min.x, ", ", entityAABB.min.y, ", ", entityAABB.min.z
                        , ") max(", entityAABB.max.x, ", ", entityAABB.max.y, ", ", entityAABB.max.z, ")\n");
                }
                // Second, check if entity has Transform component (for 3D models without sprites)
                else if (ecsManager.HasComponent<Transform>(entity)) {
                    auto& transform = ecsManager.GetComponent<Transform>(entity);

                    entitiesWithComponent++;
                    ENGINE_PRINT("[RaycastUtil] Found entity " , entity , " with Transform component\n");

                    // Create AABB from the entity's transform
                    if (ecsManager.HasComponent<ModelRenderComponent>(entity)) {
                        auto& modelComp = ecsManager.GetComponent<ModelRenderComponent>(entity);
                        if (modelComp.model) {
                            auto modelAABB = modelComp.model->GetBoundingBox();
                        RaycastUtil::AABB localAABB = {modelAABB.min, modelAABB.max};

                            // Transform the 8 corners to world space
                            glm::vec3 corners[8] = {
                                glm::vec3(localAABB.min.x, localAABB.min.y, localAABB.min.z),
                                glm::vec3(localAABB.max.x, localAABB.min.y, localAABB.min.z),
                                glm::vec3(localAABB.min.x, localAABB.max.y, localAABB.min.z),
                                glm::vec3(localAABB.max.x, localAABB.max.y, localAABB.min.z),
                                glm::vec3(localAABB.min.x, localAABB.min.y, localAABB.max.z),
                                glm::vec3(localAABB.max.x, localAABB.min.y, localAABB.max.z),
                                glm::vec3(localAABB.min.x, localAABB.max.y, localAABB.max.z),
                                glm::vec3(localAABB.max.x, localAABB.max.y, localAABB.max.z)
                            };

                            glm::vec3 worldMin = glm::vec3(std::numeric_limits<float>::max());
                            glm::vec3 worldMax = glm::vec3(std::numeric_limits<float>::lowest());

                            for (auto& corner : corners) {
                                Vector3D cornerVec(corner.x, corner.y, corner.z);
                                Vector3D worldCornerVec = transform.worldMatrix.TransformPoint(cornerVec);
                                glm::vec3 worldCorner(worldCornerVec.x, worldCornerVec.y, worldCornerVec.z);
                                worldMin = glm::min(worldMin, worldCorner);
                                worldMax = glm::max(worldMax, worldCorner);
                            }

                            entityAABB = AABB(worldMin, worldMax);
                        } else {
                            entityAABB = CreateAABBFromTransform(transform.worldMatrix);
                        }
                    } else {
                        entityAABB = CreateAABBFromTransform(transform.worldMatrix);
                    }
                    hasValidAABB = true;

                    ENGINE_PRINT("[RaycastUtil] Entity ", entity, " (Transform) AABB: min("
                        , entityAABB.min.x, ", ", entityAABB.min.y, ", ", entityAABB.min.z
                        , ") max(", entityAABB.max.x, ", ", entityAABB.max.y, ", ", entityAABB.max.z, ")\n");
                }

                // Test ray intersection if we have a valid AABB
                if (hasValidAABB) {
                    float distance;
                    if (RayAABBIntersection(ray, entityAABB, distance)) {
                        ENGINE_PRINT("[RaycastUtil] Ray hit entity " , entity , " at distance " , distance , "\n");

                        // Update closest hit if this is closer
                        if (!closestHit.hit || distance < closestHit.distance) {
                            closestHit.hit = true;
                            closestHit.entity = entity;
                            closestHit.distance = distance;
                        }
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

bool RaycastUtil::GetEntityTransform(Entity entity, float outMatrix[16], bool is2DMode) {
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

bool RaycastUtil::SetEntityTransform(Entity entity, const float matrix[16], bool is2DMode) {
    try {
        // Get the active ECS manager
        ECSRegistry& registry = ECSRegistry::GetInstance();
        ECSManager& ecsManager = registry.GetActiveECSManager();

        // Check if entity has Transform component
        if (ecsManager.HasComponent<Transform>(entity)) {
            //auto& transform = ecsManager.GetComponent<Transform>(entity);

            // Convert column-major float array (ImGuizmo/GLM format) to row-major Matrix4x4
            // ImGuizmo provides column-major, Matrix4x4 is row-major
            Matrix4x4 newMatrix;
            newMatrix.m.m00 = matrix[0];  newMatrix.m.m01 = matrix[4];  newMatrix.m.m02 = matrix[8];   newMatrix.m.m03 = matrix[12];
            newMatrix.m.m10 = matrix[1];  newMatrix.m.m11 = matrix[5];  newMatrix.m.m12 = matrix[9];   newMatrix.m.m13 = matrix[13];
            newMatrix.m.m20 = matrix[2];  newMatrix.m.m21 = matrix[6];  newMatrix.m.m22 = matrix[10];  newMatrix.m.m23 = matrix[14];
            newMatrix.m.m30 = matrix[3];  newMatrix.m.m31 = matrix[7];  newMatrix.m.m32 = matrix[11];  newMatrix.m.m33 = matrix[15];

            // Manual robust matrix decomposition (glm::decompose fails with very small scales)
            // Check for NaN/Inf in input matrix first
            bool hasInvalidValues = false;
            for (int i = 0; i < 16; ++i) {
                if (!std::isfinite(matrix[i])) {
                    hasInvalidValues = true;
                    ENGINE_PRINT(EngineLogging::LogLevel::Error, "[RaycastUtil] Invalid matrix value detected at index ", i, ": ", matrix[i], "\n");
                    break;
                }
            }

            if (hasInvalidValues) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[RaycastUtil] Rejecting invalid transform matrix\n");
                return false;
            }

            // Manually extract transform components (more robust than glm::decompose for small scales)
            glm::mat4 glmMatrix = glm::make_mat4(matrix);

            // Extract position (translation) - last column
            glm::vec3 position(glmMatrix[3][0], glmMatrix[3][1], glmMatrix[3][2]);

            // Extract scale - length of basis vectors
            glm::vec3 scale;
            glm::vec3 col0(glmMatrix[0][0], glmMatrix[0][1], glmMatrix[0][2]);
            glm::vec3 col1(glmMatrix[1][0], glmMatrix[1][1], glmMatrix[1][2]);
            glm::vec3 col2(glmMatrix[2][0], glmMatrix[2][1], glmMatrix[2][2]);

            scale.x = glm::length(col0);
            scale.y = glm::length(col1);
            scale.z = glm::length(col2);

            // Check for zero or near-zero scale (would cause division by zero)
            // Renamed to fix warning C4458 - EPSILON hides class member
            const float SCALE_EPSILON = 1e-8f;
            if (scale.x < SCALE_EPSILON || scale.y < SCALE_EPSILON || scale.z < SCALE_EPSILON) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[RaycastUtil] Scale too small for rotation extraction: (",
                    scale.x, ", ", scale.y, ", ", scale.z, ")\n");
                return false;
            }

            // Extract rotation by normalizing the basis vectors
            glm::mat3 rotMat;
            rotMat[0] = col0 / scale.x;
            rotMat[1] = col1 / scale.y;
            rotMat[2] = col2 / scale.z;

            // Check for NaN/Inf in extracted values
            if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
                !std::isfinite(scale.x) || !std::isfinite(scale.y) || !std::isfinite(scale.z)) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[RaycastUtil] Extraction produced NaN/Inf values. Position: (",
                    position.x, ", ", position.y, ", ", position.z, ") Scale: (", scale.x, ", ", scale.y, ", ", scale.z, ")\n");
                return false;
            }

            // Clamp scales to prevent precision issues and extreme values
            const float MIN_SCALE = 0.00001f;  // Minimum to prevent division by zero
            const float MAX_SCALE = 10000.0f;   // Maximum to prevent extremely large objects

            auto clampScale = [](float s, float minS, float maxS) -> float {
                float absS = std::abs(s);
                if (absS < minS) {
                    ENGINE_PRINT("[RaycastUtil] Scale too small (", s, "), clamping to ", minS, "\n");
                    return minS * (s < 0 ? -1.0f : 1.0f);
                }
                if (absS > maxS) {
                    ENGINE_PRINT("[RaycastUtil] Scale too large (", s, "), clamping to ", maxS, "\n");
                    return maxS * (s < 0 ? -1.0f : 1.0f);
                }
                return s;
            };

            scale.x = clampScale(scale.x, MIN_SCALE, MAX_SCALE);
            scale.y = clampScale(scale.y, MIN_SCALE, MAX_SCALE);
            scale.z = clampScale(scale.z, MIN_SCALE, MAX_SCALE);

            // Convert glm vectors to Vector3D
            Vector3D newPosition(position.x, position.y, position.z);
            Vector3D newScale(scale.x, scale.y, scale.z);

            // Convert rotation matrix directly to quaternion (avoids Euler angle gimbal lock and precision issues)
            Matrix4x4 rotMatrix4x4;
            rotMatrix4x4.m.m00 = rotMat[0][0]; rotMatrix4x4.m.m01 = rotMat[1][0]; rotMatrix4x4.m.m02 = rotMat[2][0]; rotMatrix4x4.m.m03 = 0.0f;
            rotMatrix4x4.m.m10 = rotMat[0][1]; rotMatrix4x4.m.m11 = rotMat[1][1]; rotMatrix4x4.m.m12 = rotMat[2][1]; rotMatrix4x4.m.m13 = 0.0f;
            rotMatrix4x4.m.m20 = rotMat[0][2]; rotMatrix4x4.m.m21 = rotMat[1][2]; rotMatrix4x4.m.m22 = rotMat[2][2]; rotMatrix4x4.m.m23 = 0.0f;
            rotMatrix4x4.m.m30 = 0.0f;         rotMatrix4x4.m.m31 = 0.0f;         rotMatrix4x4.m.m32 = 0.0f;         rotMatrix4x4.m.m33 = 1.0f;

            Quaternion newRotation = Quaternion::FromMatrix(rotMatrix4x4);
            newRotation.Normalize();

            // Directly update Transform component to avoid multiple recalculations (prevents flickering)
            if (ecsManager.HasComponent<Transform>(entity)) {
                auto& transform = ecsManager.GetComponent<Transform>(entity);

                // Set local transform components directly
                transform.localPosition = newPosition;
                transform.localScale = newScale;
                transform.localRotation = newRotation;

                // Update world matrix directly to avoid recalculation
                transform.worldMatrix = newMatrix;

                // Mark as dirty so TransformSystem knows to update children if needed
                // (TransformSystem will detect the change on next update)
            } else {
                // Fallback: use TransformSystem methods (slower but safer)
                ecsManager.transformSystem->SetWorldPosition(entity, newPosition);
                ecsManager.transformSystem->SetWorldScale(entity, newScale);
                // Convert quaternion to Euler for SetWorldRotation
                Vector3D euler = newRotation.ToEulerDegrees();
                ecsManager.transformSystem->SetWorldRotation(entity, euler);
            }

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

            // Check for invalid values in matrix
            bool hasInvalidValues = false;
            for (int i = 0; i < 16; ++i) {
                if (!std::isfinite(matrix[i])) {
                    hasInvalidValues = true;
                    ENGINE_PRINT(EngineLogging::LogLevel::Error, "[RaycastUtil] Invalid sprite matrix value at index ", i, "\n");
                    break;
                }
            }

            if (hasInvalidValues) {
                return false;
            }

            // Decompose the matrix to get position, rotation, and scale
            glm::vec3 position;
            glm::vec3 scale;
            glm::quat rotation;
            glm::vec3 skew;
            glm::vec4 perspective;

            bool decomposeSuccess = glm::decompose(transformMatrix, scale, rotation, position, skew, perspective);

            if (!decomposeSuccess) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[RaycastUtil] Sprite matrix decomposition failed\n");
                return false;
            }

            // Check for NaN/Inf in decomposed values
            if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
                !std::isfinite(scale.x) || !std::isfinite(scale.y) || !std::isfinite(scale.z)) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[RaycastUtil] Sprite decomposition produced NaN/Inf\n");
                return false;
            }

            // Clamp sprite scale to reasonable values
            const float MIN_SCALE = 0.00001f;
            const float MAX_SCALE = 10000.0f;
            scale.x = std::clamp(std::abs(scale.x), MIN_SCALE, MAX_SCALE) * (scale.x < 0 ? -1.0f : 1.0f);
            scale.y = std::clamp(std::abs(scale.y), MIN_SCALE, MAX_SCALE) * (scale.y < 0 ? -1.0f : 1.0f);
            scale.z = std::clamp(std::abs(scale.z), MIN_SCALE, MAX_SCALE) * (scale.z < 0 ? -1.0f : 1.0f);

            // Extract Z rotation robustly for 2D sprites
            glm::mat4 rotMat = glm::mat4_cast(rotation);
            float zRotationRadians = atan2f(rotMat[1][0], rotMat[0][0]);

            // Check for NaN in rotation
            if (!std::isfinite(zRotationRadians)) {
                ENGINE_PRINT(EngineLogging::LogLevel::Error, "[RaycastUtil] Sprite rotation extraction produced NaN\n");
                return false;
            }

            // For 3D sprites, position is already the center (no conversion needed)
            // Update sprite properties directly
            sprite.position = Vector3D::ConvertGLMToVector3D(position);
            sprite.scale = Vector3D::ConvertGLMToVector3D(scale);
            sprite.rotation = glm::degrees(zRotationRadians); // Convert back to degrees

            return true;
        }
    } catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[RaycastUtil] Error setting transform for entity ", entity, ": ", e.what(), "\n");
    }

    return false;
}

bool RaycastUtil::IsEntity3D(Entity entity) {
    try {
        // Get the active ECS manager
        ECSRegistry& registry = ECSRegistry::GetInstance();
        ECSManager& ecsManager = registry.GetActiveECSManager();

        // Check for ModelRenderComponent (always 3D)
        if (ecsManager.HasComponent<ModelRenderComponent>(entity)) {
            return true;
        }

        // Check for SpriteRenderComponent with is3D flag
        if (ecsManager.HasComponent<SpriteRenderComponent>(entity)) {
            auto& sprite = ecsManager.GetComponent<SpriteRenderComponent>(entity);
            return sprite.is3D;
        }

        // Check for TextRenderComponent with is3D flag
        if (ecsManager.HasComponent<TextRenderComponent>(entity)) {
            auto& text = ecsManager.GetComponent<TextRenderComponent>(entity);
            return text.is3D;
        }

        // Default: entities without render components are considered 3D
        // (Transform-only entities, lights, cameras, etc.)
        return true;
    } catch (const std::exception& e) {
        ENGINE_PRINT(EngineLogging::LogLevel::Error, "[RaycastUtil] Error checking if entity is 3D: ", e.what(), "\n");
        return true; // Default to 3D on error
    }
}

