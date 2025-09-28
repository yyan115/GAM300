#include "pch.h"
#include "Physics/PhysicsSystem.hpp"
#include "Physics/JoltInclude.hpp"
#include "Physics/CollisionFilters.hpp"
#include "Physics/RigidbodyComponent.hpp"
#include "Physics/ColliderComponent.hpp"
#include "Transform/TransformComponent.hpp"
#include "Math/Matrix4x4.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ECSManager.hpp"
#include <cstdarg>
#include <cstdio>

// Static trace function for Jolt physics
static void JoltTraceImpl(const char* inFMT, ...) {
    va_list args;
    va_start(args, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, args);
    va_end(args);

    // Output to engine logging system
    std::string message = "Jolt Physics: " + std::string(buffer);
    ENGINE_LOG_ERROR(message.c_str());
}

PhysicsSystem::PhysicsSystem() = default;

PhysicsSystem::~PhysicsSystem() = default;

void PhysicsSystem::Initialise() {
    // Register allocation hook for Jolt
    JPH::RegisterDefaultAllocator();

    // Install callbacks - provide a safe trace function
    JPH::Trace = JoltTraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = [](const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine) {
        // Handle assertion failure - avoid unused variable warnings
        (void)inExpression; (void)inMessage; (void)inFile; (void)inLine;
        return true; // Continue execution
    });

    // Create a factory
    JPH::Factory::sInstance = new JPH::Factory();

    // Register all Jolt physics types
    JPH::RegisterTypes();

    // Create temp allocator with more memory
    m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(32 * 1024 * 1024); // 32 MB

    // Create job system
    m_jobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

    // Create mapping table from object layer to broadphase layer
    m_broadPhaseLayerInterface = std::make_unique<MyBroadPhaseLayerInterface>();

    // Create collision filters
    m_objectVsBroadPhaseLayerFilter = std::make_unique<MyObjectVsBroadPhaseLayerFilter>();
    m_objectLayerPairFilter = std::make_unique<MyObjectLayerPairFilter>();

    // Initialize the physics system
    m_physicsSystem = std::make_unique<JPH::PhysicsSystem>();
    m_physicsSystem->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
                         *m_broadPhaseLayerInterface, *m_objectVsBroadPhaseLayerFilter, *m_objectLayerPairFilter);

    // Set default gravity
    SetGravity(Vector3D(0.0f, -9.81f, 0.0f));

    // Process any existing entities with physics components
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    for (const auto& entity : entities) {
        if (ecsManager.HasComponent<RigidbodyComponent>(entity)) {
            CreatePhysicsBody(entity);
        }
    }
}

void PhysicsSystem::Update(float deltaTime) {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    // Update any dirty physics bodies
    for (const auto& entity : entities) {
        if (ecsManager.HasComponent<RigidbodyComponent>(entity)) {
            auto& rigidbody = ecsManager.GetComponent<RigidbodyComponent>(entity);

            // Create physics body if it doesn't exist
            if (rigidbody.physicsBodyHandle == nullptr) {
                CreatePhysicsBody(entity);
            }
            // Update physics body if it's dirty
            else if (rigidbody.isDirty) {
                UpdatePhysicsBody(entity);
            }
        }
    }

    // Step the physics simulation
    const int cCollisionSteps = 1;
    m_physicsSystem->Update(deltaTime, cCollisionSteps, m_tempAllocator.get(), m_jobSystem.get());

    // Sync physics transforms back to game transforms
    for (const auto& entity : entities) {
        if (ecsManager.HasComponent<RigidbodyComponent>(entity)) {
            auto& rigidbody = ecsManager.GetComponent<RigidbodyComponent>(entity);
            if (rigidbody.physicsBodyHandle != nullptr && rigidbody.bodyType == BodyType::Dynamic) {
                SyncTransformFromPhysics(entity);
            }
        }
    }
}

void PhysicsSystem::Shutdown() {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    // Remove all physics bodies
    for (const auto& entity : entities) {
        if (ecsManager.HasComponent<RigidbodyComponent>(entity)) {
            RemovePhysicsBody(entity);
        }
    }

    // Destroy physics system
    m_physicsSystem.reset();
    m_objectLayerPairFilter.reset();
    m_objectVsBroadPhaseLayerFilter.reset();
    m_broadPhaseLayerInterface.reset();
    m_jobSystem.reset();
    m_tempAllocator.reset();

    // Destroy factory
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    // Note: There's no UnregisterDefaultAllocator in Jolt, cleanup is automatic
}

void PhysicsSystem::SetGravity(const Vector3D& gravity) {
    if (m_physicsSystem) {
        m_physicsSystem->SetGravity(JPH::Vec3(gravity.x, gravity.y, gravity.z));
    }
}

Vector3D PhysicsSystem::GetGravity() const {
    if (m_physicsSystem) {
        JPH::Vec3 gravity = m_physicsSystem->GetGravity();
        return Vector3D(gravity.GetX(), gravity.GetY(), gravity.GetZ());
    }
    return Vector3D(0.0f, -9.81f, 0.0f);
}

void PhysicsSystem::CreatePhysicsBody(Entity entity) {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    if (!ecsManager.HasComponent<RigidbodyComponent>(entity) || !ecsManager.HasComponent<Transform>(entity)) {
        return;
    }

    auto& rigidbody = ecsManager.GetComponent<RigidbodyComponent>(entity);
    auto& transform = ecsManager.GetComponent<Transform>(entity);

    // Create collider shape
    JPH::RefConst<JPH::Shape> shape;
    if (ecsManager.HasComponent<ColliderComponent>(entity)) {
        auto& collider = ecsManager.GetComponent<ColliderComponent>(entity);
        CreateColliderShape(entity);
        // For now, create a default box shape if no valid shape is created
        shape = new JPH::BoxShape(JPH::Vec3(collider.size.x * 0.5f, collider.size.y * 0.5f, collider.size.z * 0.5f));
    } else {
        // Default box shape
        shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
    }

    // Create body creation settings
    JPH::BodyCreationSettings settings;
    settings.SetShape(shape);

    // Set position and rotation from transform
    Vector3D worldPos = Matrix4x4::ExtractTranslation(transform.worldMatrix);
    Vector3D worldRot = transform.localRotation.ToEulerDegrees();
    settings.mPosition = JPH::RVec3(worldPos.x, worldPos.y, worldPos.z);
    settings.mRotation = JPH::Quat::sEulerAngles(JPH::Vec3(worldRot.x * (M_PI / 180.0f), worldRot.y * (M_PI / 180.0f), worldRot.z * (M_PI / 180.0f)));

    // Set motion type based on BodyType
    switch (rigidbody.bodyType) {
        case BodyType::Static:
            settings.mMotionType = JPH::EMotionType::Static;
            settings.mObjectLayer = Layers::NON_MOVING;
            break;
        case BodyType::Kinematic:
            settings.mMotionType = JPH::EMotionType::Kinematic;
            settings.mObjectLayer = Layers::MOVING;
            break;
        case BodyType::Dynamic:
            settings.mMotionType = JPH::EMotionType::Dynamic;
            settings.mObjectLayer = Layers::MOVING;
            break;
    }

    // Set physics properties
    settings.mRestitution = rigidbody.restitution;
    settings.mFriction = rigidbody.friction;
    settings.mGravityFactor = rigidbody.isGravityEnabled ? 1.0f : 0.0f;
    settings.mIsSensor = rigidbody.isTrigger;

    if (rigidbody.bodyType == BodyType::Dynamic) {
        settings.mMassPropertiesOverride.mMass = rigidbody.mass;
    }

    // Create the body
    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();
    JPH::BodyID bodyID = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate);

    // Store the body ID in the rigidbody component
    rigidbody.physicsBodyHandle = new JPH::BodyID(bodyID);
    rigidbody.isDirty = false;
}

void PhysicsSystem::UpdatePhysicsBody(Entity entity) {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    if (!ecsManager.HasComponent<RigidbodyComponent>(entity)) {
        return;
    }

    auto& rigidbody = ecsManager.GetComponent<RigidbodyComponent>(entity);

    if (rigidbody.physicsBodyHandle == nullptr) {
        CreatePhysicsBody(entity);
        return;
    }

    JPH::BodyID* bodyID = static_cast<JPH::BodyID*>(rigidbody.physicsBodyHandle);
    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

    // Update physics properties
    bodyInterface.SetRestitution(*bodyID, rigidbody.restitution);
    bodyInterface.SetFriction(*bodyID, rigidbody.friction);
    bodyInterface.SetGravityFactor(*bodyID, rigidbody.isGravityEnabled ? 1.0f : 0.0f);

    // Sync transform to physics
    SyncTransformToPhysics(entity);

    rigidbody.isDirty = false;
}

void PhysicsSystem::RemovePhysicsBody(Entity entity) {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    if (!ecsManager.HasComponent<RigidbodyComponent>(entity)) {
        return;
    }

    auto& rigidbody = ecsManager.GetComponent<RigidbodyComponent>(entity);

    if (rigidbody.physicsBodyHandle != nullptr) {
        JPH::BodyID* bodyID = static_cast<JPH::BodyID*>(rigidbody.physicsBodyHandle);
        JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

        bodyInterface.RemoveBody(*bodyID);
        bodyInterface.DestroyBody(*bodyID);

        delete bodyID;
        rigidbody.physicsBodyHandle = nullptr;
    }
}

void PhysicsSystem::SyncTransformFromPhysics(Entity entity) {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    if (!ecsManager.HasComponent<RigidbodyComponent>(entity) || !ecsManager.HasComponent<Transform>(entity)) {
        return;
    }

    auto& rigidbody = ecsManager.GetComponent<RigidbodyComponent>(entity);
    auto& transform = ecsManager.GetComponent<Transform>(entity);

    if (rigidbody.physicsBodyHandle == nullptr) {
        return;
    }

    JPH::BodyID* bodyID = static_cast<JPH::BodyID*>(rigidbody.physicsBodyHandle);
    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

    // Get physics position and rotation
    JPH::RVec3 position = bodyInterface.GetPosition(*bodyID);
    JPH::Quat rotation = bodyInterface.GetRotation(*bodyID);

    // Convert to engine types and update transform
    transform.localPosition = Vector3D(static_cast<float>(position.GetX()), static_cast<float>(position.GetY()), static_cast<float>(position.GetZ()));

    // Convert quaternion to Euler angles
    JPH::Vec3 eulerAngles = rotation.GetEulerAngles();
    transform.localRotation = Quaternion::FromEulerDegrees(Vector3D(eulerAngles.GetX() * (180.0f / M_PI), eulerAngles.GetY() * (180.0f / M_PI), eulerAngles.GetZ() * (180.0f / M_PI)));

    // Mark transform as dirty so it gets updated
    transform.isDirty = true;
}

void PhysicsSystem::SyncTransformToPhysics(Entity entity) {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    if (!ecsManager.HasComponent<RigidbodyComponent>(entity) || !ecsManager.HasComponent<Transform>(entity)) {
        return;
    }

    auto& rigidbody = ecsManager.GetComponent<RigidbodyComponent>(entity);
    auto& transform = ecsManager.GetComponent<Transform>(entity);

    if (rigidbody.physicsBodyHandle == nullptr) {
        return;
    }

    JPH::BodyID* bodyID = static_cast<JPH::BodyID*>(rigidbody.physicsBodyHandle);
    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

    // Convert transform to Jolt types
    Vector3D worldPos = Matrix4x4::ExtractTranslation(transform.worldMatrix);
    Vector3D eulerRot = transform.localRotation.ToEulerDegrees();

    JPH::RVec3 position(worldPos.x, worldPos.y, worldPos.z);
    JPH::Quat rotation = JPH::Quat::sEulerAngles(JPH::Vec3(eulerRot.x * (M_PI / 180.0f), eulerRot.y * (M_PI / 180.0f), eulerRot.z * (M_PI / 180.0f)));

    // Update physics body
    bodyInterface.SetPosition(*bodyID, position, JPH::EActivation::Activate);
    bodyInterface.SetRotation(*bodyID, rotation, JPH::EActivation::Activate);
}

void PhysicsSystem::CreateColliderShape(Entity entity) {
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

    if (!ecsManager.HasComponent<ColliderComponent>(entity)) {
        return;
    }

    auto& collider = ecsManager.GetComponent<ColliderComponent>(entity);

    // For now, we'll implement this later as needed
    // This would create the appropriate Jolt shape based on collider type
    collider.isDirty = false;
}

void PhysicsSystem::UpdateColliderShape(Entity entity) {
    // Implementation for updating collider shapes
    CreateColliderShape(entity);
}

bool PhysicsSystem::Raycast(const Vector3D& origin, const Vector3D& direction, float maxDistance, Entity& hitEntity, Vector3D& hitPoint) const {
    if (!m_physicsSystem) {
        return false;
    }

    // Create ray cast settings
    JPH::RRayCast ray;
    ray.mOrigin = JPH::RVec3(origin.x, origin.y, origin.z);
    ray.mDirection = JPH::Vec3(direction.x, direction.y, direction.z) * maxDistance;

    // Perform raycast
    JPH::RayCastResult hit;
    if (m_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, hit)) {
        // We got a hit, but we need to convert BodyID back to Entity
        // This would require maintaining a mapping from BodyID to Entity
        // For now, we'll return false as this mapping isn't implemented yet
        return false;
    }

    return false;
}

void PhysicsSystem::ApplyForce(Entity entity, const Vector3D& force) {
    if (!m_physicsSystem) {
        return;
    }

    // Get the ECS manager to check if entity has physics components
    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    if (!ecsManager.HasComponent<RigidbodyComponent>(entity)) {
        return;
    }

    auto& rigidbody = ecsManager.GetComponent<RigidbodyComponent>(entity);
    if (rigidbody.physicsBodyHandle == nullptr) {
        return; // Body not created yet
    }

    // Get body interface and apply force
    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();
    JPH::Vec3 joltForce(force.x, force.y, force.z);
    JPH::BodyID* bodyIDPtr = static_cast<JPH::BodyID*>(rigidbody.physicsBodyHandle);
    bodyInterface.AddForce(*bodyIDPtr, joltForce);
}

void PhysicsSystem::SetVelocity(Entity entity, const Vector3D& velocity) {
    if (!m_physicsSystem) {
        return;
    }

    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    if (!ecsManager.HasComponent<RigidbodyComponent>(entity)) {
        return;
    }

    auto& rigidbody = ecsManager.GetComponent<RigidbodyComponent>(entity);
    if (rigidbody.physicsBodyHandle == nullptr) {
        return;
    }

    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();
    JPH::Vec3 joltVelocity(velocity.x, velocity.y, velocity.z);
    JPH::BodyID* bodyIDPtr = static_cast<JPH::BodyID*>(rigidbody.physicsBodyHandle);
    bodyInterface.SetLinearVelocity(*bodyIDPtr, joltVelocity);
}

Vector3D PhysicsSystem::GetVelocity(Entity entity) const {
    if (!m_physicsSystem) {
        return Vector3D(0, 0, 0);
    }

    ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
    if (!ecsManager.HasComponent<RigidbodyComponent>(entity)) {
        return Vector3D(0, 0, 0);
    }

    auto& rigidbody = ecsManager.GetComponent<RigidbodyComponent>(entity);
    if (rigidbody.physicsBodyHandle == nullptr) {
        return Vector3D(0, 0, 0);
    }

    JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();
    JPH::BodyID* bodyIDPtr = static_cast<JPH::BodyID*>(rigidbody.physicsBodyHandle);
    JPH::Vec3 joltVelocity = bodyInterface.GetLinearVelocity(*bodyIDPtr);
    return Vector3D(joltVelocity.GetX(), joltVelocity.GetY(), joltVelocity.GetZ());
}