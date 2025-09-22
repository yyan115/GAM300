#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <limits>
#include "EditorState.hpp"  // This already defines Entity and INVALID_ENTITY
#include "Math/Matrix4x4.hpp"

/**
 * @brief Utility class for raycasting in 3D space for entity selection.
 */
class RaycastUtil {
public:
    struct Ray {
        glm::vec3 origin;
        glm::vec3 direction;

        Ray(const glm::vec3& o, const glm::vec3& d) : origin(o), direction(glm::normalize(d)) {}
    };

    struct AABB {
        glm::vec3 min;
        glm::vec3 max;

        AABB(const glm::vec3& minimum, const glm::vec3& maximum) : min(minimum), max(maximum) {}
    };

    struct RaycastHit {
        Entity entity = INVALID_ENTITY;
        float distance = std::numeric_limits<float>::max();
        glm::vec3 point;
        bool hit = false;
    };

    /**
     * @brief Create a ray from camera through screen coordinates.
     * @param mouseX Screen X coordinate (0 to screenWidth)
     * @param mouseY Screen Y coordinate (0 to screenHeight)
     * @param screenWidth Screen width in pixels
     * @param screenHeight Screen height in pixels
     * @param viewMatrix Camera view matrix
     * @param projMatrix Camera projection matrix
     * @return Ray in world space
     */
    static Ray ScreenToWorldRay(float mouseX, float mouseY,
                               float screenWidth, float screenHeight,
                               const Matrix4x4& viewMatrix, const Matrix4x4& projMatrix);

    /**
     * @brief Test ray intersection with axis-aligned bounding box.
     * @param ray The ray to test
     * @param aabb The bounding box to test against
     * @param distance Output distance to intersection point
     * @return true if ray intersects the AABB
     */
    static bool RayAABBIntersection(const Ray& ray, const AABB& aabb, float& distance);

    /**
     * @brief Create AABB from transform matrix.
     * @param transform The entity's transform matrix
     * @param modelSize Optional model size (default 1x1x1 cube)
     * @return AABB in world space
     */
    static AABB CreateAABBFromTransform(const Matrix4x4& transform,
                                       const glm::vec3& modelSize = glm::vec3(1.0f));

    /**
     * @brief Perform raycast against all entities in the scene.
     * @param ray The ray to cast
     * @return The closest hit entity, or INVALID_ENTITY if no hit
     */
    static RaycastHit RaycastScene(const Ray& ray);

    /**
     * @brief Get transform matrix for an entity (avoids including Graphics headers in ScenePanel).
     * @param entity The entity to get transform for
     * @param outMatrix Output array of 16 floats for the transform matrix
     * @return true if entity has transform, false otherwise
     */
    static bool GetEntityTransform(Entity entity, float outMatrix[16]);

    /**
     * @brief Set transform matrix for an entity (avoids including Graphics headers in ScenePanel).
     * @param entity The entity to set transform for
     * @param matrix Input array of 16 floats for the transform matrix
     * @return true if entity transform was updated successfully, false otherwise
     */
    static bool SetEntityTransform(Entity entity, const float matrix[16]);


private:
    static constexpr float EPSILON = 1e-6f;
};